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

int UploadTransfer::getTransferType()
{
    return TRANSFER_TYPE_UPLOAD;
}

void UploadTransfer::startTransfer()
{
    //
    if (!inputFile.isOpen())
    {
        inputFile.setFileName(filePathName);
        inputFile.open(QIODevice::ReadOnly);
    }
    status = TRANSFER_STATE_RUNNING;
}

void UploadTransfer::pauseTransfer()
{
    status = TRANSFER_STATE_PAUSED;
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
    bytesWrittenSinceUpdate = 0;
}
