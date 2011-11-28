#include "uploadtransfer.h"

UploadTransfer::UploadTransfer()
{
    bytesWrittenSinceUpdate = 0;

    // perhaps 0 is sufficient, it is anyway retarded to draw progress bars for partial upload segments...
    transferProgress = 0;

    status = TRANSFER_STATE_INITIALIZING;
    transferRateCalculationTimer = new QTimer(this);
    connect(transferRateCalculationTimer, SIGNAL(timeout()), this, SLOT(transferRateCalculation()));
    transferRateCalculationTimer->setSingleShot(false);
    transferRateCalculationTimer->start(1000);

    transferInactivityTimer = new QTimer(this);
    connect(transferInactivityTimer, SIGNAL(timeout()), this, SLOT(abortTransfer()));
    transferInactivityTimer->start(TIMER_INACTIVITY_MSECS);
}

UploadTransfer::~UploadTransfer()
{
    delete transferRateCalculationTimer;
    delete transferInactivityTimer;
}

void UploadTransfer::setFileName(QString &filename)
{
    filePathName = filename;
    inputFile.setFileName(filePathName);
    inputFile.open(QIODevice::ReadOnly);
    fileSize = inputFile.size();
}

int UploadTransfer::getTransferType()
{
    return TRANSFER_TYPE_UPLOAD;
}

void UploadTransfer::startTransfer()
{
    status = TRANSFER_STATE_RUNNING;
    if (fileOffset > fileSize)
        return;
    else if (fileOffset + segmentLength > fileSize)
        segmentLength = fileSize - fileOffset;

    const char * f = (char*)inputFile.map(fileOffset, segmentLength);
    quint64 wptr = 0;
    QByteArray header;
    header.append(DataPacket);
    header.append(ProtocolADataPacket);
    QByteArray data(QByteArray::fromRawData(f, segmentLength));
    while (wptr < segmentLength)
    {
        QByteArray packet(header);
        packet.append(toQByteArray((quint64)(fileOffset + wptr)));
        packet.append(TTH);
        qDebug() << "Write data fileOffset " << fileOffset << " segmentLength " << segmentLength << " wptr " << wptr;
        if (wptr + PACKET_DATA_MTU < segmentLength)
        {
            packet.append(data.mid(wptr, PACKET_DATA_MTU));
            wptr += PACKET_DATA_MTU;
        }
        else
        {
            packet.append(data.mid(wptr, segmentLength - wptr));
            wptr += segmentLength - wptr;
        }
        emit transmitDatagram(remoteHost, packet);
    }
    bytesWrittenSinceUpdate += segmentLength;
    inputFile.unmap((unsigned char *)f);
    transferInactivityTimer->start(TIMER_INACTIVITY_MSECS);
}

void UploadTransfer::pauseTransfer()
{
    status = TRANSFER_STATE_PAUSED; // TODO
}

void UploadTransfer::abortTransfer()
{
    status = TRANSFER_STATE_ABORTING;
    inputFile.close();
    emit abort(this);
}

void UploadTransfer::transferRateCalculation()
{
    if ((status == TRANSFER_STATE_RUNNING) && (bytesWrittenSinceUpdate == 0))
        status = TRANSFER_STATE_STALLED;
    else if (status == TRANSFER_STATE_STALLED)
        status = TRANSFER_STATE_RUNNING;

    // snapshot the transfer rate as the amount of bytes written in the last second
    transferRate = bytesWrittenSinceUpdate;
    if (bytesWrittenSinceUpdate > 0)
    {
        bytesWrittenSinceUpdate = 0;
        transferInactivityTimer->start(TIMER_INACTIVITY_MSECS);
    }
}
