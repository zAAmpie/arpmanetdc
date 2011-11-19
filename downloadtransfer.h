#ifndef DOWNLOADTRANSFER_H
#define DOWNLOADTRANSFER_H
#include "transfer.h"

#define BUCKET_SIZE 1<<20

class DownloadTransfer : public Transfer
{
public:
    DownloadTransfer();
    ~DownloadTransfer();

public slots:
    void hashBucketReply(QByteArray &rootTTH, int &bucketNumber, QByteArray &bucketTTH);

private:
    void incomingDataPacket(quint8 transferProtocolVersion, quint64 &offset, QByteArray &data);
    int getTransferType();
    void startTransfer();
    void pauseTransfer();
    void abortTransfer();
    void transferRateCalculation();

    int calculateBucketNumber(quint64 fileOffset);

    QHash<int, QByteArray*> downloadBucketTable;

    int bytesWrittenSinceUpdate;
};

#endif // DOWNLOADTRANSFER_H
