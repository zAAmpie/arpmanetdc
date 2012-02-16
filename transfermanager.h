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

#ifndef TRANSFERMANAGER_H
#define TRANSFERMANAGER_H

#include <QObject>
#include <QList>
#include <QHostAddress>
//#include "transfer.h"
#include "uploadtransfer.h"
#include "downloadtransfer.h"
#include "execthread.h"

typedef struct
{
    quint8 protocol;
    QHostAddress requestingHost;
    quint64 fileOffset;
    quint64 requestLength;
} UploadTransferQueueItem;

typedef struct
{
    QString filePathName;
    QByteArray tth;
    quint64 fileSize;
    QHostAddress fileHost;
} DownloadTransferQueueItem;

typedef struct
{
    QByteArray TTH;
    QString filePathName;
    int transferType;
    int transferStatus;
    int transferProgress;
    quint64 transferRate;
    QHostAddress host;
    qint64 uptime;
    int onlineSegments;
    SegmentStatusStruct segmentStatuses;
} TransferItemStatus;

class TransferManager : public QObject
{
    Q_OBJECT
public:
    explicit TransferManager(QHash<QString, QString> *settings, QObject *parent = 0);
    ~TransferManager();

signals:
    void filePathNameRequest(QByteArray tth);
    void saveTTHSourceToDatabase(QByteArray tth, QHostAddress peerAddress);
    void loadTTHSourcesFromDatabase(QByteArray tth);
    void deleteTTHSourcesFromDatabase(QByteArray tth);
    void searchTTHAlternateSources(QByteArray tth);
    void TTHTreeRequest(QHostAddress hostAddr,QByteArray rootTTH, quint32 startBucket, quint32 bucketCount);
    void sendDownloadRequest(quint8 protocolPreference, QHostAddress dstHost, QByteArray tth, quint64 offset, quint64 length);
    void sendTransferError(QHostAddress dstHost, quint8 error, QByteArray tth, quint64 offset);
    void flushBucket(QString filename, QByteArray *bucket);
    void assembleOutputFile(QString tmpfilebase, QString outfile, int startbucket, int lastbucket);
    void flushBucketDirect(QString outfile, int bucketno, QByteArray *bucket, QByteArray tth);
    void renameIncompleteFile(QString filename);

    // Request hashing of a bucket that has finished downloading
    void hashBucketRequest(QByteArray rootTTH, int bucketNumber, QByteArray bucket);

    void transmitDatagram(QHostAddress dstHost, QByteArray *datagram);

    // GUI updates
    void downloadStarted(QByteArray tth);
    void downloadCompleted(QByteArray tth);

    //Transfer status
    void returnGlobalTransferStatus(QList<TransferItemStatus> status);

    // Request peer protocol capabilities
    void requestProtocolCapability(QHostAddress peer);

public slots:
    void incomingDataPacket(quint8 transferProtocolVersion, QHostAddress fromHost, QByteArray datagram);
    void incomingTransferError(QHostAddress fromHost, QByteArray tth, quint64 offset, quint8 error);

    // Request file name for given TTH from sharing engine, reply with empty string if not found.
    void filePathNameReply(QByteArray tth, QString filename, quint64 fileSize);

    void incomingUploadRequest(quint8 protocol, QHostAddress fromHost, QByteArray tth, quint64 offset, quint64 length);
    void queueDownload(int priority, QByteArray tth, QString filePathName, quint64 fileSize, QHostAddress fileHost);
    void changeQueuedDownloadPriority(int oldPriority, int newPriority, QByteArray tth);
    void removeQueuedDownload(int priority, QByteArray tth);

    //Stop a transfer already running
    void stopTransfer(QByteArray tth, int type, QHostAddress host);

    //Download completed
    void transferDownloadCompleted(QByteArray tth);

    //Get transfer status
    void requestGlobalTransferStatus();

    // Response from hashing engine when bucket finished hashing
    void hashBucketReply(QByteArray rootTTH, int bucketNumber, QByteArray bucketTTH);

    void incomingTTHSource(QByteArray tth, QHostAddress sourcePeer);
    void incomingTTHTree(QByteArray tth, QByteArray tree);

    // Protocol capability
    void incomingProtocolCapabilityResponse(QHostAddress fromHost, char protocols);
    void requestPeerProtocolCapability(QHostAddress peer, Transfer* transferObject);

    // Bucket flush callbacks
    void bucketFlushed(QByteArray tth, int bucketNo);
    void bucketFlushFailed(QByteArray tth, int bucketNo);

    QList<TransferItemStatus> getGlobalTransferStatus();

    void destroyTransferObject(Transfer*);

    // Set functions
    void setMaximumSimultaneousDownloads(int n);
    void setMaximumSimultaneousUploads(int n);

private:
    Transfer* getTransferObjectPointer(QByteArray &tth, int transferType, QHostAddress hostAddr = QHostAddress("0.0.0.0"));
    DownloadTransferQueueItem getNextQueuedDownload();
    void startNextDownload();
    QMap<int, QList<DownloadTransferQueueItem>* > downloadTransferQueue;
    QMultiHash<QByteArray, Transfer*> transferObjectTable;
    QMultiHash<QByteArray, UploadTransferQueueItem*> uploadTransferQueue;
    QHash<QHostAddress, char> peerProtocolCapabilities;
    QMultiHash<QHostAddress, Transfer*> peerProtocolDiscoveryWaitingPool;
    int maximumSimultaneousDownloads;
    int maximumSimultaneousUploads;
    int currentDownloadCount;
    int currentUploadCount;

    const QHash<QString, QString> *pSettings;
};

#endif // TRANSFERMANAGER_H
