#include "uploadtransfer.h"

UploadTransfer::UploadTransfer(QObject *parent) : Transfer(parent)
{
    upload = 0;
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

    // temp, just get it working again in new structure
    upload = new FSTPTransferSegment(this);
    //Used to intercept the amount of data actually transmitted
    connect(upload, SIGNAL(transmitDatagram(QHostAddress, QByteArray *)), this, SLOT(dataTransmitted(QHostAddress, QByteArray *)));
    connect(upload, SIGNAL(transmitDatagram(QHostAddress, QByteArray *)), this, SIGNAL(transmitDatagram(QHostAddress, QByteArray *)));
}

UploadTransfer::~UploadTransfer()
{
    transferRateCalculationTimer->deleteLater();
    transferInactivityTimer->deleteLater();
    upload->deleteLater();
}

void UploadTransfer::setFileName(QString filename)
{
    filePathName = filename;
    if (upload)
        upload->setFileName(filename);
}

void UploadTransfer::setTTH(QByteArray tth)
{
    TTH = tth;
    upload->setTTH(TTH);
}

int UploadTransfer::getTransferType()
{
    return TRANSFER_TYPE_UPLOAD;
}

void UploadTransfer::startTransfer()
{
    upload->setSegmentStart(fileOffset);
    upload->setSegmentEnd(fileOffset + segmentLength);
    upload->setRemoteHost(remoteHost);
    status = TRANSFER_STATE_RUNNING;
    upload->startUploading();
    //We cannot assume that the whole segment will be sent in a second. It will either be more or less.
    //bytesWrittenSinceUpdate += segmentLength;
    transferInactivityTimer->start(TIMER_INACTIVITY_MSECS);
}

void UploadTransfer::pauseTransfer()
{
    status = TRANSFER_STATE_PAUSED; // TODO
}

void UploadTransfer::abortTransfer()
{
    status = TRANSFER_STATE_ABORTING;
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

void UploadTransfer::dataTransmitted(QHostAddress host, QByteArray *data)
{
    //Measure the true amount of data sent from the segment to Dispatcher by intercepting the data and getting its size
    bytesWrittenSinceUpdate += data->size();
}
