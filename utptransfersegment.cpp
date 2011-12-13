#include "utptransfersegment.h"

uTPTransferSegment::uTPTransferSegment(Transfer *parent) : TransferSegment(parent)
{
    pParent = parent;
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
