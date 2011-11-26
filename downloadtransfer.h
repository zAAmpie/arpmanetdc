#ifndef DOWNLOADTRANSFER_H
#define DOWNLOADTRANSFER_H
#include "transfer.h"
#include "protocoldef.h"

#define BUCKET_SIZE (1<<20)

class DownloadTransfer : public Transfer
{
public:
    DownloadTransfer();
    ~DownloadTransfer();

public slots:
    void hashBucketReply(int &bucketNumber, QByteArray &bucketTTH);
    void TTHTreeReply(QByteArray &tree);
    void setProtocolPreference(QByteArray &preference);

private slots:
    void transferTimerEvent();

private:
    void incomingDataPacket(quint8 transferProtocolVersion, quint64 &offset, QByteArray &data);
    int getTransferType();
    void startTransfer();
    void pauseTransfer();
    void abortTransfer();
    void transferRateCalculation();

    void sendDownloadRequest(QHostAddress &dstHost, QByteArray &tth, quint64 &offset, quint64 &length);

    int calculateBucketNumber(quint64 fileOffset);
    void flushBucketToDisk(int &bucketNumber);

    QHash<int, QByteArray*> downloadBucketTable;
    QHash<int, QByteArray*> downloadBucketHashLookupTable;

    int bytesWrittenSinceUpdate;
    QByteArray protocolPreference;

    QTimer *transferTimer;
};

#endif // DOWNLOADTRANSFER_H
