#ifndef DOWNLOADTRANSFER_H
#define DOWNLOADTRANSFER_H
#include "transfer.h"
#include "protocoldef.h"
#include "fstptransfersegment.h"

class DownloadTransfer : public Transfer
{
public:
    DownloadTransfer(QObject *parent = 0);
    ~DownloadTransfer();

public slots:
    void hashBucketReply(int bucketNumber, QByteArray bucketTTH);
    void TTHTreeReply(QByteArray tree);
    void setProtocolPreference(QByteArray &preference);

private slots:
    void transferTimerEvent();

private:
    void incomingDataPacket(quint8 transferProtocolVersion, quint64 offset, QByteArray data);
    int getTransferType();
    void startTransfer();
    void pauseTransfer();
    void abortTransfer();
    void transferRateCalculation();

    void flushBucketToDisk(int &bucketNumber);
    inline int calculateBucketNumber(quint64 fileOffset);

    QHash<int, QByteArray*> *downloadBucketTable;
    QHash<int, QByteArray*> downloadBucketHashLookupTable;

    int lastBucketNumber;
    int lastBucketSize;
    int initializationStateTimerBrakes;

    int bytesWrittenSinceUpdate;
    QByteArray protocolPreference;

    TransferSegment *download;
};

#endif // DOWNLOADTRANSFER_H
