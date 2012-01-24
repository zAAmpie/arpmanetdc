#include "utptransfersegment.h"

uTPTransferSegment::uTPTransferSegment(Transfer *parent)
{
    pParent = parent;

    // UTP_Create allocates memory for utpSocket struct, this must be called first
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    // address and port do not really matter, packets get hijacked for dispatch anyway
    // for all I care libutp can think it connects to "itself"
    addr.sin_port = htons(4012);
    inet_pton(AF_INET, "1.2.3.4", &addr.sin_addr);
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
}

void uTPTransferSegment::transferTimerEvent()
{
    UTP_CheckTimeouts();
}

// not interested in offset here, for uTP it only denotes the relevant segment start
void uTPTransferSegment::incomingDataPacket(quint64, QByteArray data)
{
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
}

void uTPTransferSegment::setFileSize(quint64 size)
{
    fileSize = size;
}

void uTPTransferSegment::startUploading()
{
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
    segmentOffset = 0;
    UTP_Connect(utpSocket);
    checkSendDownloadRequest(uTPProtocol, remoteHost, TTH, segmentStart, segmentLength);
}

qint64 uTPTransferSegment::getBytesReceivedNotFlushed()
{
    return 0;
}

// --------------============= uTP callback functions =============--------------
void uTPTransferSegment::uTPRead(const byte *bytes, size_t count)
{
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
}

void uTPTransferSegment::uTPWrite(byte *bytes, size_t count)
{
    if (segmentOffset + count > segmentLength)
        count = segmentLength - segmentOffset;  // TODO + check
    bytes = (unsigned char *)fileMap + segmentOffset;
    segmentOffset += count;

}

size_t uTPTransferSegment::uTPGetRBSize()
{
    return 0;
}

void uTPTransferSegment::uTPState(int state)
{
    // TODO (or ignore?)
}

void uTPTransferSegment::uTPError(int errcode)
{
    if (utpSocket)
    {
        UTP_Close(utpSocket);
        utpSocket = NULL;
    }
}

void uTPTransferSegment::uTPOverhead(bool send, size_t count, int type)
{

}

// this thing gets called when the uTP layer wants to push a packet onto the "wire"
void uTPTransferSegment::uTPSendTo(const byte *p, size_t len, const struct sockaddr *to, socklen_t tolen)
{
    QByteArray *packet = new QByteArray;
    packet->append(DataPacket);
    packet->append(uTPProtocol);
    packet->append(toQByteArray((quint64)segmentStart));
    packet->append(TTH);
    packet->append((const char *)p, len);
    emit transmitDatagram(remoteHost, packet);
}

// this thing gets called when a connection has been established and it's safe to start pushing packets onto the "wire"
void uTPTransferSegment::uTPIncomingConnection(UTPSocket *s)
{
    // TODO: Call UTP_Write to tell the socket to start filling the write buffer with x number of bytes through uTPWrite?
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
