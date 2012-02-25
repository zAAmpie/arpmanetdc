#include "utptransfersegment.h"

uTPTransferSegment::uTPTransferSegment(Transfer *parent)
{
    pParent = parent;
    connect_called = false;

    // UTP_Create allocates memory for utpSocket struct, this must be called first
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    // address and port do not really matter, packets get hijacked for dispatch anyway
    // for all I care libutp can think it connects to "itself"
    addr.sin_port = htons(4012);
#ifdef Q_OS_WIN
    //Load an unsigned long into the sockaddr
    addr.sin_addr.S_un.S_addr = QHostAddress("127.0.0.1").toIPv4Address();
#else
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
#endif
    //Test to ensure the ip is built correctly
    QHostAddress test((const struct sockaddr*)&addr);
    QString testStr = test.toString();

    utpSocket = UTP_Create(uTPTransferSegment::utp_sendto, this, (const struct sockaddr*)&addr, sizeof(addr));

    UTP_SetSockopt(utpSocket, SO_SNDBUF, 100*300);

    UTPFunctionTable utp_callbacks = 
    {
        &uTPTransferSegment::utp_read,
        &uTPTransferSegment::utp_write,
        &uTPTransferSegment::utp_get_rb_size,
        &uTPTransferSegment::utp_state,
        &uTPTransferSegment::utp_error,
        &uTPTransferSegment::utp_overhead
    };

    UTP_SetCallbacks(utpSocket, &utp_callbacks, this);
}

uTPTransferSegment::~uTPTransferSegment()
{
    if (inputFile.isOpen())
        inputFile.close();
    if (fileMap)
        inputFile.unmap((unsigned char *)fileMap);
    qDebug() << "uTPTransferSegment destroyed";
}

void uTPTransferSegment::transferTimerEvent()
{
    UTP_CheckTimeouts();
}

// not interested in offset here, for uTP it only denotes the relevant segment start
void uTPTransferSegment::incomingDataPacket(quint64 offset, QByteArray data)
{
    qDebug() << "uTPTransferSegment::incomingDataPacket()" << offset;
    UTP_IsIncomingUTP(uTPTransferSegment::utp_incoming, uTPTransferSegment::utp_sendto, this,
                      (const unsigned char *)data.constData(), data.length(),
                      (const sockaddr *)&addr, sizeof(addr));
}

void uTPTransferSegment::setFileName(QString filename)
{
    filePathName = filename;
    inputFile.setFileName(filePathName);
    inputFile.open(QIODevice::ReadOnly);
    fileSize = inputFile.size();
    qDebug() << "uTPTransferSegment::setFileName()" << filePathName << fileSize;
}

void uTPTransferSegment::setFileSize(quint64 size)
{
    qDebug() << "uTPTransferSegment::setFileSize()" << size;
    fileSize = size;
}

void uTPTransferSegment::startUploading()
{
    qDebug() << "uTPTransferSegment::startUploading()";
    if (segmentStart > fileSize)
        return;
    else if (segmentStart + segmentLength > fileSize)
        segmentLength = fileSize - segmentStart;

    segmentOffset = 0;

    if (fileMap)
        inputFile.unmap((unsigned char *)fileMap);
    fileMap = inputFile.map(segmentStart, segmentLength);
}

void uTPTransferSegment::startDownloading()
{
    qDebug() << "uTPTransferSegment::startDownloading()";
    segmentOffset = 0;
    checkSendDownloadRequest(uTPProtocol, remoteHost, TTH, segmentStart, segmentLength, status);
    if (!connect_called)
    {
        UTP_Connect(utpSocket);
        connect_called = true;
    }
}

void uTPTransferSegment::pauseDownload()
{

}

void uTPTransferSegment::unpauseDownload()
{

}

qint64 uTPTransferSegment::getBytesReceivedNotFlushed()
{
    return 0;
}

// --------------============= uTP callback functions =============--------------
void uTPTransferSegment::uTPRead(const byte *bytes, size_t count)
{
    qDebug() << "uTPTransferSegment::uTPRead()" << count;
    int bucketNumber = calculateBucketNumber(segmentOffset);
    if (!pDownloadBucketTable->contains(bucketNumber))
    {
        QByteArray *bucket = new QByteArray();
        pDownloadBucketTable->insert(bucketNumber, bucket);
    }
    QByteArray data = QByteArray((const char *)bytes, count); // TODO: optimize this to work directly with *bytes
    if ((pDownloadBucketTable->value(bucketNumber)->length() + data.length()) > HASH_BUCKET_SIZE)
    {
        int bucketRemaining = HASH_BUCKET_SIZE - pDownloadBucketTable->value(bucketNumber)->length();
        pDownloadBucketTable->value(bucketNumber)->append(data.mid(0, bucketRemaining));
        if (!pDownloadBucketTable->contains(bucketNumber + 1))
        {
            QByteArray *nextBucket = new QByteArray(data.mid(bucketRemaining));
            pDownloadBucketTable->insert(bucketNumber + 1, nextBucket);
        }
        // there should be no else - if the next bucket exists and data is sticking over, there is an error,
        // since we segment on bucket boundaries. tth checksumming will catch the problems.
    }
    else
    {
        pDownloadBucketTable->value(bucketNumber)->append(data);
        //qDebug() << "Append data " << requestingOffset << offset << pDownloadBucketTable->value(bucketNumber)->length();
    }

    if (pDownloadBucketTable->value(bucketNumber)->length() == HASH_BUCKET_SIZE)
    {
        emit hashBucketRequest(TTH, bucketNumber, pDownloadBucketTable->value(bucketNumber));
        qDebug() << "uTPTransferSegment emit hashBucketRequest() " << bucketNumber << pDownloadBucketTable->value(bucketNumber)->length();
    }

    // these last bucket numbers are for the *segment*, not the file.
    // the length check is for in case it is also the last segment of the file.
    if ((bucketNumber == lastBucketNumber) && (lastBucketSize == pDownloadBucketTable->value(bucketNumber)->length())) // End of Segment
    {
        //status = TRANSFER_STATE_FINISHED;  // local segment
        qDebug() << "uTPTransferSegment emit hashBucketRequest() on finish " << bucketNumber << pDownloadBucketTable->value(bucketNumber)->length();
        emit hashBucketRequest(TTH, bucketNumber, pDownloadBucketTable->value(bucketNumber));
    }
    segmentOffset += count;
    bytesTransferred += count;
    emit updateDirectBytesStats(count);
}

void uTPTransferSegment::uTPWrite(byte *bytes, size_t count)
{
    qDebug() << "uTPTransferSegment::uTPWrite()" << count;
    if (segmentOffset + count > segmentLength)
        count = segmentLength - segmentOffset;  // TODO + check
    bytes = (unsigned char *)fileMap + segmentOffset;
    segmentOffset += count;

}

size_t uTPTransferSegment::uTPGetRBSize()
{
    qDebug() << "uTPTransferSegment::uTPGetRBSize()";
    return 0;
}

void uTPTransferSegment::uTPState(int state)
{
    qDebug() << "uTPTransferSegment::uTPState()" << state;
    /*if (state == UTP_STATE_CONNECT || state == UTP_STATE_WRITABLE)
    {
        if (UTP_Write(utpSocket, segmentLength))
        {
            // Testing only, this can really be improved, persistent connections will perform better.
            UTP_Close(utpSocket);
        }
    }*/
}

void uTPTransferSegment::uTPError(int errcode)
{
    qDebug() << "uTPTransferSegment::uTPError()" << errcode;
    if (utpSocket)
    {
        UTP_Close(utpSocket);
        utpSocket = NULL;
    }
}

void uTPTransferSegment::uTPOverhead(bool send, size_t count, int type)
{
    qDebug() << "uTPTransferSegment::uTPOverhead()" << send << count << type;
}

// this thing gets called when the uTP layer wants to push a packet onto the "wire"
void uTPTransferSegment::uTPSendTo(const byte *p, size_t len, const struct sockaddr *to, socklen_t tolen)
{
    QByteArray *packet = new QByteArray;
    packet->append(DirectDataPacket);
    packet->append(uTPProtocol);
    packet->append(quint64ToByteArray((quint64)segmentStart));
    packet->append(quint32ToByteArray(segmentId));
    packet->append((const char *)p, len);
    qDebug() << "uTPTransferSegment::uTPSendTo()" << len << packet->length() << remoteHost;
    emit transmitDatagram(remoteHost, packet);
}

// this thing gets called when a connection has been established and it's safe to start pushing packets onto the "wire"
void uTPTransferSegment::uTPIncomingConnection(UTPSocket *s)
{
    qDebug() << "uTPTransferSegment::uTPIncomingConnection()";
    // TODO: Call UTP_Write to tell the socket to start filling the write buffer with x number of bytes through uTPWrite?
    UTP_Write(utpSocket, segmentLength);
}


// --------------============= static uTP callback wrappers =============--------------
// count bytes arrived over uTP connection
void uTPTransferSegment::utp_read(void* data, const byte* bytes, size_t count)
{
    ((uTPTransferSegment *)data)->uTPRead(bytes, count);
}

// phone in to get next count bytes to write
void uTPTransferSegment::utp_write(void* data, byte* bytes, size_t count)
{
    ((uTPTransferSegment *)data)->uTPWrite(bytes, count);
}

// get receive buffer length?
size_t uTPTransferSegment::utp_get_rb_size(void* data)
{
    return ((uTPTransferSegment *)data)->uTPGetRBSize();
}   

// phone state changes in
void uTPTransferSegment::utp_state(void* data, int state)
{
    ((uTPTransferSegment *)data)->uTPState(state);
}

// phone errors in
void uTPTransferSegment::utp_error(void* data, int errcode)
{
    ((uTPTransferSegment *)data)->uTPError(errcode);
}

void uTPTransferSegment::utp_overhead(void *data, bool send, size_t count, int type)
{
    ((uTPTransferSegment *)data)->uTPOverhead(send, count, type);
}

void uTPTransferSegment::utp_sendto(void *data, const byte *p, size_t len, const struct sockaddr *to, socklen_t tolen)
{
    ((uTPTransferSegment *)data)->uTPSendTo(p, len, to, tolen);
}

void uTPTransferSegment::utp_incoming(void *data, UTPSocket *s)
{
    ((uTPTransferSegment *)data)->uTPIncomingConnection(s);
}
