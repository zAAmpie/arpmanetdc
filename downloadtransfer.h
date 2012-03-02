/* This file is part of ArpmanetDC. Copyright (C) 2012
 * Source code can be found at http://code.google.com/p/arpmanetdc/
 * 
 * ArpmanetDC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ArpmanetDC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with ArpmanetDC.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DOWNLOADTRANSFER_H
#define DOWNLOADTRANSFER_H
#include "transfer.h"
#include "protocoldef.h"
#include "fstptransfersegment.h"
#include "utptransfersegment.h"

#define MAXIMUM_SIMULTANEOUS_SEGMENTS 5// When downloading a ton of segments, the sheer number of ACKs and stalls actually slow down the transfer

#define HASH_TREE_WINDOW_LENGTH 184 // 8 datagrams

typedef struct
{
    int segmentBucketOffset;
    int segmentBucketCount;
} SegmentOffsetLengthStruct;

// This looks like duplication, but it is for easy dispatch of incoming packets to segments.
typedef struct
{
    quint64 segmentEnd;
    TransferSegment *transferSegment;
} TransferSegmentTableStruct;

typedef struct
{
    quint64 bytesTransferred;
    quint8 protocolCapability;
    TransferSegment *transferSegment;
    QByteArray triedProtocols;
    int failureCount;
    bool blacklisted;
    qint64 lastStartTime;
    quint64 lastBytesQueued;
    qint64 lastTransferRate;
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
    void incomingDataPacket(quint8 transferProtocolVersion, qint64 offset, QByteArray data);
    int getTransferType();
    void startTransfer();
    void pauseTransfer();
    void abortTransfer();
    void addPeer(QHostAddress peer);
    qint64 getTransferRate();
    int getTransferProgress();
    QByteArray getTransferStateBitmap();
    int getSegmentCount();
    SegmentStatusStruct getSegmentStatuses();
    void incomingTransferError(qint64 offset, quint8 error);
    void setBucketFlushStateBitmap(QByteArray bitmap);

    // Bucket flush callbacks
    void bucketFlushed(int bucketNo);
    void bucketFlushFailed(int bucketNo);

private slots:
    void transferTimerEvent();
    void TTHSearchTimerEvent();
    void newSegmentTimerEvent();
    void protocolCapabilityRequestTimerEvent();
    void segmentCompleted(TransferSegment *segment);
    void segmentFailed(TransferSegment *segment, quint8 error=0, bool startIdleSegment = true);
    void requestHashBucket(QByteArray rootTTH, int bucketNumber, QByteArray *bucket);
    void updateDirectBytesStats(int bytes);

private:
    void transferRateCalculation();
    void flushBucketToDisk(int &bucketNumber);
    inline int calculateBucketNumber(quint64 fileOffset);
    SegmentOffsetLengthStruct getSegmentForDownloading(int segmentNumberOfBucketsHint);
    TransferSegment* newConnectedTransferSegment(TransferProtocol p);
    //void updateTransferSegmentTableRange(TransferSegment *segment, quint64 newStart, quint64 newEnd);
    void newPeer(QHostAddress peer, quint8 protocols);
    TransferSegment* createTransferSegment(QHostAddress peer);
    void downloadNextAvailableChunk(TransferSegment *download, int length = 1, int recursionLimit = 5);
    int getLastHashBucketNumberReceived();
    void congestionTest();
    void requestHashTree(int lastHashBucketReceived, bool timerRequest = false);
    QHostAddress getBestIdlePeer();
    void saveBucketStateBitmap();
    bool isNonDispatchedProtocol(TransferProtocol protocol);
    TransferSegment* getSlowestActivePeer();
    int getSegmentsDone();
    int getTotalFileSegments();

    QHash<int, QByteArray*> *downloadBucketTable;
    QMap<int, QByteArray*> downloadBucketHashLookupTable;

    QTimer *transferTimer;
    QTimer *TTHSearchTimer;
    QTimer *protocolCapabilityRequestTimer;
    QTimer *newSegmentTimer;

    int lastBucketNumber;
    int lastBucketSize;
    int initializationStateTimerBrakes;
    qint64 bytesWrittenSinceUpdate;
    qint64 bytesWrittenSinceCalculation;
    //int currentActiveSegments;
    int currentActiveSegments();
    int timerBrakes;
    int hashTreeWindowEnd;
    int tthSearchInterval;
    int bucketHashQueueLength;
    int bucketFlushQueueLength;
    int treeUpdatesSinceTimer;
    bool iowait;
    QHostAddress treeRequestHost;
    int zeroSegmentTimeoutCount;

    QMap<qint64, TransferSegmentTableStruct> transferSegmentTable;
    QByteArray transferSegmentStateBitmap;
    QByteArray bucketFlushStateBitmap;
    QHash<QHostAddress, RemotePeerInfoStruct> remotePeerInfoTable;
    QHash<QHostAddress,int> remotePeerInfoRequestPool;

    QByteArray r;
};

#endif // DOWNLOADTRANSFER_H
