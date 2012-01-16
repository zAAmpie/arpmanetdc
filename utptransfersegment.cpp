#include "utptransfersegment.h"

uTPTransferSegment::uTPTransferSegment(Transfer *parent)
{
    pParent = parent;

    utpSocket = UTP_Create(&uTPTransferSegment::utp_sendto, this, NULL, sizeof(NULL));

    UTP_SetSockopt(utpSocket, SO_SNDBUF, 100*300);

    UTPFunctionTable utp_callbacks = {
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
}

void uTPTransferSegment::transferTimerEvent(){}

void uTPTransferSegment::incomingDataPacket(quint64 offset, QByteArray data)
{
    // suggestion: since this thing uses its own socket, we can trigger an upload by sending the leeching side's
    // host address and listening port as a data packet, parse it here and reverse connect.
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

}

void uTPTransferSegment::startDownloading()
{

}

qint64 uTPTransferSegment::getBytesReceivedNotFlushed()
{
    return 0;
}

// uTP callback functions
void uTPTransferSegment::uTPRead(const byte *bytes, size_t count)
{
    //TODO: read count bytes from bytes and tip them into the buckets
    // while manually keeping track of the offset
}

void uTPTransferSegment::uTPWrite(byte *bytes, size_t count)
{
    // TODO: put count bytes into bytes, keep track of offset

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

void uTPTransferSegment::uTPSendTo(const byte *p, size_t len, const struct sockaddr *to, socklen_t tolen)
{

}

void uTPTransferSegment::uTPIncomingConnection(UTPSocket *s)
{

}


// --------------============= uTP callbacks =============--------------
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
