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
    char protocol;
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
} TransferItemStatus;

class TransferManager : public QObject
{
    Q_OBJECT
public:
    explicit TransferManager(QObject *parent = 0);
    ~TransferManager();

signals:
    void filePathNameRequest(QByteArray tth);
    void saveTTHSourceToDatabase(QByteArray tth, QHostAddress peerAddress);
    void loadTTHSourcesFromDatabase(QByteArray tth);
    void deleteTTHSourcesFromDatabase(QByteArray tth);
    void searchTTHAlternateSources(QByteArray tth);
    void TTHTreeRequest(QHostAddress hostAddr,QByteArray rootTTH);
    void sendDownloadRequest(quint8 protocolPreference, QHostAddress dstHost, QByteArray tth, quint64 offset, quint64 length);
    void flushBucket(QString filename, QByteArray *bucket);
    void assembleOutputFile(QString tmpfilebase, QString outfile, int startbucket, int lastbucket);

    // Request hashing of a bucket that has finished downloading
    void hashBucketRequest(QByteArray rootTTH, int bucketNumber, QByteArray *bucket);

    void transmitDatagram(QHostAddress dstHost, QByteArray *datagram);

    // GUI updates
    void downloadStarted(QByteArray tth);
    void downloadCompleted(QByteArray tth);

    // Request peer protocol capabilities
    void requestProtocolCapability(QHostAddress peer);

public slots:
    void incomingDataPacket(quint8 transferProtocolVersion, QByteArray datagram);

    // Request file name for given TTH from sharing engine, reply with empty string if not found.
    void filePathNameReply(QByteArray tth, QString filename);

    void incomingUploadRequest(char protocol, QHostAddress fromHost, QByteArray tth, quint64 offset, quint64 length);
    void queueDownload(int priority, QByteArray tth, QString filePathName, quint64 fileSize, QHostAddress fileHost);
    void changeQueuedDownloadPriority(int oldPriority, int newPriority, QByteArray tth);
    void removeQueuedDownload(int priority, QByteArray tth);

    //Stop a transfer already running
    void stopTransfer(QByteArray tth, int type, QHostAddress host);

    // Response from hashing engine when bucket finished hashing
    void hashBucketReply(QByteArray rootTTH, int bucketNumber, QByteArray bucketTTH);

    void incomingTTHSource(QByteArray tth, QHostAddress sourcePeer);
    void incomingTTHTree(QByteArray tth, QByteArray tree);

    // Protocol capability
    void incomingProtocolCapabilityResponse(QHostAddress fromHost, char protocols);
    void requestPeerProtocolCapability(QHostAddress peer, Transfer* transferObject);

    QList<TransferItemStatus> getGlobalTransferStatus();

    void destroyTransferObject(Transfer*);

    // Set functions
    void setMaximumSimultaneousDownloads(int n);

private:
    Transfer* getTransferObjectPointer(QByteArray &tth, int transferType, QHostAddress hostAddr = QHostAddress("0.0.0.0"));
    DownloadTransferQueueItem getNextQueuedDownload();
    void startNextDownload();
    QMap<int, QList<DownloadTransferQueueItem>* > downloadTransferQueue;
    QMultiHash<QByteArray, Transfer*> transferObjectTable;
    QMultiHash<QByteArray, UploadTransferQueueItem*> uploadTransferQueue;
    QHash<QHostAddress, char> peerProtocolCapabilities;
    QHash<QHostAddress, Transfer*> peerProtocolDiscoveryWaitingPool;
    int maximumSimultaneousDownloads;
    int currentDownloadCount;

};

#endif // TRANSFERMANAGER_H
