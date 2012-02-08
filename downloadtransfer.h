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

#define MAXIMUM_SIMULTANEOUS_SEGMENTS 10

#define HASH_TREE_WINDOW_LENGTH 184 // 8 datagrams

enum transferSegmentState
{
    SegmentNotDownloaded = 0x00,
    SegmentDownloaded = 0x01,
    SegmentCurrentlyDownloading=0x02
};

enum bucketFlushState
{
    BucketNotFlushed = 0x00,
    BucketFlushed = 0x01
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
    int getTransferProgress();

private slots:
    void transferTimerEvent();
    void TTHSearchTimerEvent();
    void segmentCompleted(TransferSegment *segment);
    void segmentFailed(TransferSegment *segment);
    void requestHashBucket(QByteArray rootTTH, int bucketNumber, QByteArray *bucket);

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
    int getLastHashBucketNumberReceived();

    QHash<int, QByteArray*> *downloadBucketTable;
    QMap<int, QByteArray*> downloadBucketHashLookupTable;

    QTimer *transferTimer;
    QTimer *TTHSearchTimer;

    int lastBucketNumber;
    int lastBucketSize;
    int initializationStateTimerBrakes;
    int bytesWrittenSinceUpdate;
    int currentActiveSegments;
    int timerBrakes;
    int hashTreeWindowEnd;

    QMap<quint64, TransferSegmentTableStruct> transferSegmentTable;
    QByteArray transferSegmentStateBitmap;
    QByteArray bucketFlushStateBitmap;
    QHash<QHostAddress, RemotePeerInfoStruct> remotePeerInfoTable;
};

#endif // DOWNLOADTRANSFER_H
