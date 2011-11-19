#ifndef TRANSFERMANAGER_H
#define TRANSFERMANAGER_H

#include <QObject>
#include <QList>
#include <QHostAddress>
//#include "transfer.h"
#include "uploadtransfer.h"
#include "downloadtransfer.h"

typedef struct
{
    quint8 protocol;
    QHostAddress requestingHost;
    quint64 fileOffset;
    quint64 requestLength;
} UploadTransferQueueItem;

typedef struct
{
    QByteArray* TTH;
    QString* filePathName;
    int transferType;
    int* transferStatus;
    int* transferProgress;
    quint64* transferRate;
} TransferItemStatus;

class TransferManager : public QObject
{
    Q_OBJECT
public:
    explicit TransferManager(QObject *parent = 0);
    ~TransferManager();

signals:
    void filePathNameRequest(QByteArray &tth);
    void saveTTHSourceToDatabase(QByteArray &tth, QHostAddress &peerAddress);
    void loadTTHSourcesFromDatabase(QByteArray &tth);
    void deleteTTHSourcesFromDatabase(QByteArray &tth);
    void searchTTHAlternateSources(QByteArray &tth);

public slots:
    void incomingDataPacket(quint8 transferProtocolVersion, QByteArray &datagram);

    // Request file name for given TTH from sharing engine, reply with empty string if not found.
    void filePathNameReply(QByteArray &tth, QString &filename);

    void incomingUploadRequest(quint8 protocolVersion, QHostAddress &fromHost, QByteArray &tth, quint64 &offset, quint64 &length);
    void incomingDownloadRequest(quint8 protocolVersion, QString &filePathName, QByteArray &tth);

    void incomingTTHSource(QByteArray &tth, QHostAddress &sourcePeer);

    QList<TransferItemStatus> getGlobalTransferStatus();

    void destroyTransferObject(Transfer*);

private:
    Transfer* getTransferObjectPointer(QByteArray &tth, QHostAddress &hostAddr, int transferType);
    QMultiHash<QByteArray, Transfer*> transferObjectTable;
    QMultiHash<QByteArray, UploadTransferQueueItem*> uploadTransferQueue;

};

#endif // TRANSFERMANAGER_H
