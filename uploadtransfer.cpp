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
}

UploadTransfer::~UploadTransfer()
{
    transferRateCalculationTimer->deleteLater();
    transferInactivityTimer->deleteLater();
    upload->deleteLater();
}

void UploadTransfer::incomingDataPacket(quint8 transferProtocolVersion, quint64 offset, QByteArray data)
{
    if (upload)
        upload->incomingDataPacket(offset, data);
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

TransferSegment* UploadTransfer::createUploadObject(quint8 protocol, quint32 segmentId)
{
    if (upload)
        delete upload;

    upload = 0;

    switch (protocol)
    {
    case FailsafeTransferProtocol:
        upload = new FSTPTransferSegment(this);
        break;
    case uTPProtocol:
        upload = new uTPTransferSegment(this);
        break;
    case BasicTransferProtocol:
    case ArpmanetFECProtocol:
        break;
    }

    if (!upload)
        return 0;

    upload->setSegmentId(segmentId);

    //Used to intercept the amount of data actually transmitted
    connect(upload, SIGNAL(transmitDatagram(QHostAddress, QByteArray *)), this, SLOT(dataTransmitted(QHostAddress, QByteArray *)));
    connect(upload, SIGNAL(transmitDatagram(QHostAddress, QByteArray *)), this, SIGNAL(transmitDatagram(QHostAddress, QByteArray *)));
    connect(upload, SIGNAL(sendTransferError(QHostAddress,quint8,QByteArray,quint64)), this, SIGNAL(sendTransferError(QHostAddress,quint8,QByteArray,quint64)));
    return upload;
}

void UploadTransfer::startTransfer()
{
    if (!upload)
        return;

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
        status = TRANSFER_STATE_IDLE;
    else if ((status == TRANSFER_STATE_IDLE) && (bytesWrittenSinceUpdate > 0))
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

int UploadTransfer::getTransferProgress()
{
    //Only a decent guess for upload progress - cannot determine exactly what the downstream client received or in what order/segment
    qint64 maxUploadRequestOffset = upload->getMaxUploadRequestOffset();
    int progress = (maxUploadRequestOffset * 100) / fileSize;
    return progress > 100 ? 100 : progress;
}


QByteArray UploadTransfer::getTransferStateBitmap()
{
    int totalBuckets = (fileSize>>20);
    totalBuckets += ((fileSize % HASH_BUCKET_SIZE) != 0) ? 1 : 0;

    //Return an empty array for the time being
    return QByteArray(totalBuckets, SegmentNotUploaded);
}

int UploadTransfer::getSegmentCount()
{
    return 1;
}

SegmentStatusStruct UploadTransfer::getSegmentStatuses()
{
    SegmentStatusStruct s;
    s.failed = 0;
    s.finished = 0;
    s.initializing = 0;
    s.running = 0;
    s.stalled = 0;
    if (upload)
    {
        switch (upload->getSegmentStatus())
        {
        case TRANSFER_STATE_RUNNING:
            s.running++;
            break;
        case TRANSFER_STATE_FAILED:
            s.failed++;
            break;
        case TRANSFER_STATE_FINISHED:
            s.finished++;
            break;
        case TRANSFER_STATE_INITIALIZING:
            s.initializing++;
            break;
        case TRANSFER_STATE_STALLED:
            s.stalled++;
            break;
        }
    }

    return s;
}
