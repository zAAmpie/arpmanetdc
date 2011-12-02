#ifndef TRANSFERSEGMENT_H
#define TRANSFERSEGMENT_H

#include <QObject>
#include <QHostAddress>
#include <QFile>
#include <QHash>
#include "protocoldef.h"
#include "util.h"

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

public slots:
    virtual void incomingDataPacket(quint64 offset, QByteArray data) = 0;
    virtual void transferTimerEvent();
    virtual void setFileName(QString filename) = 0;
    virtual void setFileSize(qint64 size);
    void setTTH(QByteArray tth);
    void setFileOffset(quint64 offset);
    void setFileOffsetLength(quint64 length);
    void setRemoteHost(QHostAddress host);
    virtual void startUploading() = 0;
    virtual void startDownloading() = 0;
    void setDownloadBucketTablePointer(QHash<int, QByteArray*> *dbt);
    virtual void setLastBucketSize(int size);
    virtual void setLastBucketNumber(int n);

protected:
    QByteArray TTH;
    QString filePathName;
    qint64 fileSize;
    QHostAddress remoteHost;
    quint64 fileOffset;
    quint64 segmentLength;

    int lastBucketNumber;
    int lastBucketSize;

    QFile inputFile;
    QHash<int, QByteArray*> *pDownloadBucketTable;
};

#endif // TRANSFERSEGMENT_H
