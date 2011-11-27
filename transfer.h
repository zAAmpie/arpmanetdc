#ifndef TRANSFER_H
#define TRANSFER_H

#include <QObject>
#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QFile>
#include <QTimer>
#include "util.h"

#define TRANSFER_STATE_PAUSED 0
#define TRANSFER_STATE_INITIALIZING 1
#define TRANSFER_STATE_RUNNING 2
#define TRANSFER_STATE_STALLED 3
#define TRANSFER_STATE_ABORTING 4
#define TRANSFER_STATE_FINISHED 5

#define TRANSFER_TYPE_UPLOAD 0
#define TRANSFER_TYPE_DOWNLOAD 1

#define PACKET_MTU 1436
#define PACKET_DATA_MTU 1402
#define TRANSFER_MAXIMUM_SEGMENT 262144

class Transfer : public QObject
{
    Q_OBJECT
public:
    explicit Transfer(QObject *parent = 0);
    virtual ~Transfer();

signals:
    void abort(Transfer*);
    void hashBucketRequest(QByteArray &rootTTH, int &bucketNumber, QByteArray *bucket);
    void TTHTreeRequest(QHostAddress hostAddr, QByteArray rootTTH);
    void searchTTHAlternateSources(QByteArray &tth);
    void loadTTHSourcesFromDatabase(QByteArray tth);
    void sendDownloadRequest(QByteArray &protocolPreference, QHostAddress &dstHost, QByteArray &tth, quint64 &offset, quint64 &length);
    void transmitDatagram(QHostAddress &dstHost, QByteArray &datagram);

public slots:
    void setFileName(QString &filename);
    void setTTH(QByteArray &tth);
    void setFileOffset(quint64 offset);
    void setSegmentLength(quint64 length);
    void setRemoteHost(QHostAddress remote);
    void setTransferProtocol(quint8 protocol);
    void setTransferProtocolHint(QByteArray &protocolHint);
    void setFileSize(quint64 size);
    QByteArray* getTTH();
    QString* getFileName();
    QHostAddress* getRemoteHost();
    quint64 getTransferRate();
    int getTransferStatus();
    int getTransferProgress();
    void hashBucketReply(int &bucketNumber, QByteArray &bucketTTH);
    void addPeer(QHostAddress peer);
    virtual void TTHTreeReply(QByteArray tree);

    virtual void incomingDataPacket(quint8 transferProtocolVersion, quint64 &offset, QByteArray &data);
    virtual int getTransferType() = 0;
    virtual void startTransfer() = 0;
    virtual void pauseTransfer() = 0;
    virtual void abortTransfer() = 0;
    virtual void transferRateCalculation() = 0;

 //   virtual void receiveData(QByteArray &data) = 0;
    virtual void transferTimerEvent();

protected:
    QByteArray TTH;
    QByteArray TTHBase32;
    QString filePathName;
    quint8 transferProtocol;
    QByteArray transferProtocolHint;
    QHostAddress remoteHost;
    quint64 fileOffset;
    quint64 segmentLength;
    int status;
    QList<QHostAddress> listOfPeers;
    QTimer *transferRateCalculationTimer;
    QTimer *transferTimer;
    quint64 transferRate;
    int transferProgress;
    quint64 fileSize;
};

#endif // TRANSFER_H
