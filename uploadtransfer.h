#ifndef UPLOADTRANSFER_H
#define UPLOADTRANSFER_H
#include "transfer.h"
#include "fstptransfersegment.h"
#include "protocoldef.h"

#define TIMER_INACTIVITY_MSECS 60000

class UploadTransfer : public Transfer
{
    Q_OBJECT
public:
    UploadTransfer(QObject *parent = 0);
    ~UploadTransfer();
    void setFileName(QString filename);
    void setTTH(QByteArray tth);
    void createUploadObject(quint8 protocol);

private slots:
    void dataTransmitted(QHostAddress host, QByteArray *data);

private:
    QTimer* transferInactivityTimer;
    int getTransferType();
    void startTransfer();
    void pauseTransfer();
    void abortTransfer();
    void transferRateCalculation();

    quint64 bytesWrittenSinceUpdate;

    TransferSegment *upload;
};

#endif // UPLOADTRANSFER_H
