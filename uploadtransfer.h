#ifndef UPLOADTRANSFER_H
#define UPLOADTRANSFER_H
#include "transfer.h"
#include "protocoldef.h"

#define TIMER_INACTIVITY_MSECS 60000

class UploadTransfer : public Transfer
{
public:
    UploadTransfer();
    ~UploadTransfer();

private:
    QTimer* transferInactivityTimer;
    int getTransferType();
    void startTransfer();
    void pauseTransfer();
    void abortTransfer();
    void transferRateCalculation();

    QFile inputFile;
    quint64 bytesWrittenSinceUpdate;
};

#endif // UPLOADTRANSFER_H
