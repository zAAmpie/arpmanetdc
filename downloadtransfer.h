#ifndef DOWNLOADTRANSFER_H
#define DOWNLOADTRANSFER_H
#include "transfer.h"
#include "protocoldef.h"
#include "fstptransfersegment.h"

#define MAXIMUM_SIMULTANEOUS_SEGMENTS 10

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

// somehow this thing becomes read-only, hence the pointers and double pointer
typedef struct
{
    quint64 bytesTransferred;
    quint8 protocolCapability;
    TransferSegment *transferSegment;
    QByteArray triedProtocols;
} RemotePeerInfoStruct;

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
    void receivedPeerProtocolCapability(QHostAddress peer, quint8 protocols);
    void incomingDataPacket(quint8 transferProtocolVersion, quint64 offset, QByteArray data);
    int getTransferType();
    void startTransfer();
    void pauseTransfer();
    void abortTransfer();
    void addPeer(QHostAddress peer);

private slots:
    void transferTimerEvent();
    void TTHSearchTimerEvent();
    void segmentCompleted(TransferSegment *segment);
    void segmentFailed(TransferSegment *segment);

private:
    void transferRateCalculation();
    void flushBucketToDisk(int &bucketNumber);
    inline int calculateBucketNumber(quint64 fileOffset);
    SegmentOffsetLengthStruct getSegmentForDownloading(int segmentNumberOfBucketsHint);
    TransferSegment* newConnectedTransferSegment(TransferProtocol p);
    //void updateTransferSegmentTableRange(TransferSegment *segment, quint64 newStart, quint64 newEnd);
    void newPeer(QHostAddress peer, quint8 protocols);
    TransferSegment* createTransferSegment(QHostAddress peer);
    void downloadNextAvailableChunk(TransferSegment *download, int length = 1);

    QHash<int, QByteArray*> *downloadBucketTable;
    QMap<int, QByteArray*> downloadBucketHashLookupTable;

    QTimer *transferTimer;
    QTimer *TTHSearchTimer;

    int lastBucketNumber;
    int lastBucketSize;
    int initializationStateTimerBrakes;
    int bytesWrittenSinceUpdate;
    int totalBucketsFlushed;
    int currentActiveSegments;

    QMap<quint64, TransferSegmentTableStruct> transferSegmentTable;
    QByteArray transferSegmentStateBitmap;
    QHash<QHostAddress, RemotePeerInfoStruct> remotePeerInfoTable;
};

#endif // DOWNLOADTRANSFER_H
