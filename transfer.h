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

#ifndef TRANSFER_H
#define TRANSFER_H

#include <QObject>
#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QFile>
#include <QTimer>
#include <QDateTime>
#include "util.h"
#include "transfersegment.h"
class TransferSegment;

enum transferSegmentState
{
    SegmentNotDownloaded = 0x01,
    SegmentDownloaded = 0x02,
    SegmentCurrentlyDownloading=0x04,
    SegmentCurrentlyHashing=0x08,
    SegmentCurrentlyFlushing=0x10,
    SegmentCurrentlyUploading=0x40,
    SegmentNotUploaded=0x80,
    SegmentUploaded=0x20
};

enum bucketFlushState
{
    BucketNotFlushed = 0x01,
    BucketFlushed = 0x02
};

enum transferErrors
{
    FileIOError=0x01,
    InvalidOffsetError=0x02,
    PeerAlreadyTransferring=0x04,
    NoSlotsAvailable=0x08,
    TransferAbortingError=0x10
};

struct SegmentStatusStruct
{
    int initializing;
    int running;
    int finished;
    int stalled;
    int failed;
};

class Transfer : public QObject
{
    Q_OBJECT
public:
    explicit Transfer(QObject *parent = 0);
    virtual ~Transfer();

signals:
    void abort(Transfer*);
    void hashBucketRequest(QByteArray rootTTH, int bucketNumber, QByteArray bucket);
    void TTHTreeRequest(QHostAddress hostAddr, QByteArray rootTTH, quint32 startBucket, quint32 bucketCount);
    void searchTTHAlternateSources(QByteArray tth);
    void loadTTHSourcesFromDatabase(QByteArray tth);
    void sendDownloadRequest(quint8 protocol, QHostAddress dstHost, QByteArray tth, qint64 offset, qint64 length, quint32 segmentId);
    void sendTransferError(QHostAddress dstHost, quint8 error, QByteArray tth, quint64 offset);
    void transmitDatagram(QHostAddress dstHost, QByteArray *datagram);
    void transferFinished(QByteArray tth);
    void flushBucket(QString filename, QByteArray *bucket);
    void assembleOutputFile(QString tmpfilebase, QString outfile, int startbucket, int lastbucket);
    void flushBucketDirect(QString outfile, int bucketno, QByteArray *bucket, QByteArray tth);
    void renameIncompleteFile(QString filename);
    void requestProtocolCapability(QHostAddress peer, Transfer *obj);
    void requestNextSegmentId();
    void saveBucketFlushStateBitmap(QByteArray tth, QByteArray bitmap);

public slots:
    virtual void setFileName(QString filename);
    virtual void setTTH(QByteArray tth);
    void setFileOffset(quint64 offset);
    void setSegmentLength(quint64 length);
    void setRemoteHost(QHostAddress remote);
    virtual TransferSegment* createUploadObject(quint8 protocol, quint32 segmentId);
    void setFileSize(quint64 size);
    QByteArray* getTTH();
    QString* getFileName();
    QHostAddress* getRemoteHost();
    quint64 getTransferRate();
    quint64 getFileSize();
    qint64 getUptime();
    int getTransferStatus();
    virtual int getTransferProgress();
    virtual QByteArray getTransferStateBitmap();
    virtual int getSegmentCount();
    virtual SegmentStatusStruct getSegmentStatuses();
    virtual void addPeer(QHostAddress peer);
    void setProtocolOrderPreference(QByteArray p);
    virtual void hashBucketReply(int bucketNumber, QByteArray bucketTTH);
    virtual void TTHTreeReply(QByteArray tree);
    virtual void receivedPeerProtocolCapability(QHostAddress peer, quint8 protocols);
    virtual void incomingTransferError(quint64 offset, quint8 error);
    virtual void setNextSegmentId(quint32 id);
    virtual void setBucketFlushStateBitmap(QByteArray bitmap);

    virtual void incomingDataPacket(quint8 transferProtocolVersion, quint64 offset, QByteArray data);
    virtual int getTransferType() = 0;
    virtual void startTransfer() = 0;
    virtual void pauseTransfer() = 0;
    virtual void abortTransfer() = 0;
    virtual void transferRateCalculation() = 0;

    // Bucket flush callbacks
    virtual void bucketFlushed(int bucketNo);
    virtual void bucketFlushFailed(int bucketNo);

protected:
    QByteArray TTH;
    QByteArray TTHBase32;
    QString filePathName;
    quint8 transferProtocol;
    //QByteArray transferProtocolHint;
    QHostAddress remoteHost;
    quint64 fileOffset;
    quint64 segmentLength;
    int status;
    //QList<QHostAddress> listOfPeers;
    QTimer *transferRateCalculationTimer;
    quint64 transferRate;
    int transferProgress;
    quint64 fileSize;
    QByteArray protocolOrderPreference;
    QDateTime upTime;
};

#endif // TRANSFER_H
