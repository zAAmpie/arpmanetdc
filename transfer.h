#ifndef TRANSFER_H
#define TRANSFER_H

#include <QObject>
#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QFile>
#include <QTimer>
#include "util.h"

class Transfer : public QObject
{
    Q_OBJECT
public:
    explicit Transfer(QObject *parent = 0);
    virtual ~Transfer();

signals:
    void abort(Transfer*);
    void hashBucketRequest(QByteArray rootTTH, int bucketNumber, QByteArray *bucket);
    void TTHTreeRequest(QHostAddress hostAddr, QByteArray rootTTH);
    void searchTTHAlternateSources(QByteArray tth);
    void loadTTHSourcesFromDatabase(QByteArray tth);
    void sendDownloadRequest(quint8 protocol, QHostAddress dstHost, QByteArray tth, quint64 offset, quint64 length);
    void transmitDatagram(QHostAddress dstHost, QByteArray *datagram);
    void transferFinished(QByteArray tth);

public slots:
    virtual void setFileName(QString filename);
    virtual void setTTH(QByteArray tth);
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
    void addPeer(QHostAddress peer);
    virtual void hashBucketReply(int &bucketNumber, QByteArray &bucketTTH);
    virtual void TTHTreeReply(QByteArray tree);

    virtual void incomingDataPacket(quint8 transferProtocolVersion, quint64 offset, QByteArray data);
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
