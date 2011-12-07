#ifndef TRANSFERSEGMENT_H
#define TRANSFERSEGMENT_H

#include <QObject>
#include <QHostAddress>
#include <QFile>
#include <QHash>
#include <QDateTime>
#include "protocoldef.h"
#include "util.h"
#include "transfer.h"

class TransferSegment : public QObject
{
    Q_OBJECT
public:
    explicit TransferSegment(QObject *parent = 0);
    virtual ~TransferSegment();

signals:
    void transmitDatagram(QHostAddress dstHost, QByteArray *datagram);
    void sendDownloadRequest(quint8 protocol, QHostAddress dstHost, QByteArray tth, quint64 offset, quint64 length);
    void hashBucketRequest(QByteArray rootTTH, int bucketNumber, QByteArray *bucket);
    void requestNextSegment(TransferSegment *requestingSegmentObject);
    void transferRequestFailed(TransferSegment *requestingSegmentObject);
    //void requestPeerProtocolCapability(QHostAddress peer);

public slots:
    virtual void incomingDataPacket(quint64 offset, QByteArray data) = 0;
    virtual void transferTimerEvent();
    virtual void setFileName(QString filename) = 0;
    virtual void setFileSize(quint64 size);
    void setTTH(QByteArray tth);
    void setSegmentStart(quint64 start);
    void setSegmentEnd(quint64 end);
    quint64 getSegmentStart();
    quint64 getSegmentEnd();
    qint64 getSegmentStartTime();
    QHostAddress getSegmentRemotePeer();
    void setRemoteHost(QHostAddress host);
    virtual void startUploading() = 0;
    virtual void startDownloading() = 0;
    void setDownloadBucketTablePointer(QHash<int, QByteArray*> *dbt);
    //virtual void receivedPeerProtocolCapability(char protocols);

protected:
    void calculateLastBucketParams();
    QByteArray TTH;
    QString filePathName;
    quint64 fileSize;
    QHostAddress remoteHost;
    quint64 segmentStart;
    quint64 segmentLength;
    quint64 segmentEnd;
    qint64 segmentStartTime;

    int lastBucketNumber;
    int lastBucketSize;

    QFile inputFile;
    QHash<int, QByteArray*> *pDownloadBucketTable;

    Transfer *pParent;
};

#endif // TRANSFERSEGMENT_H
