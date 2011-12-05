#ifndef DOWNLOADTRANSFER_H
#define DOWNLOADTRANSFER_H
#include "transfer.h"
#include "protocoldef.h"
#include "fstptransfersegment.h"

enum transferSegmentState
{
    SegmentNotDownloaded = 0x00,
    SegmentDownloaded = 0x01,
    SegmentCurrentlyDownloading=0x02
};

typedef struct
{
    int segmentBucketOffset;
    int segmentBucketCount;
} SegmentOffsetLengthStruct;

typedef struct
{
    quint64 segmentEnd;
    TransferSegment *transferSegment;
} TransferSegmentTableStruct;

class DownloadTransfer : public Transfer
{
    Q_OBJECT
public:
    DownloadTransfer(QObject *parent = 0);
    ~DownloadTransfer();

public slots:
    void hashBucketReply(int bucketNumber, QByteArray bucketTTH);
    void TTHTreeReply(QByteArray tree);
    //void setProtocolPreference(QByteArray &preference);
    void setPeerProtocolCapability(QHostAddress peer, char protocols);

private slots:
    void transferTimerEvent();
    void segmentCompleted(TransferSegment *segment);
    void segmentFailed(TransferSegment *segment);

private:
    void incomingDataPacket(quint8 transferProtocolVersion, quint64 offset, QByteArray data);
    int getTransferType();
    void startTransfer();
    void pauseTransfer();
    void abortTransfer();
    void transferRateCalculation();

    void flushBucketToDisk(int &bucketNumber);
    inline int calculateBucketNumber(quint64 fileOffset);
    SegmentOffsetLengthStruct getSegmentForDownloading(int segmentNumberOfBucketsHint);
    TransferSegment* newConnectedTransferSegment(TransferProtocol p);
    void updateTransferSegmentTableRange(TransferSegment *segment, quint64 newStart, quint64 newEnd);

    QHash<int, QByteArray*> *downloadBucketTable;
    QHash<int, QByteArray*> downloadBucketHashLookupTable;

    int lastBucketNumber;
    int lastBucketSize;
    int initializationStateTimerBrakes;

    int bytesWrittenSinceUpdate;
    int totalBucketsFlushed;
    //QByteArray protocolPreference;

    QMap<quint64, TransferSegmentTableStruct> transferSegmentTable;
    QByteArray transferSegmentStateBitmap;

    TransferSegment *download;
};

#endif // DOWNLOADTRANSFER_H
