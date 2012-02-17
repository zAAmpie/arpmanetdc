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
class Transfer;

class TransferSegment : public QObject
{
    Q_OBJECT
public:
    explicit TransferSegment(QObject *parent = 0);
    virtual ~TransferSegment();

signals:
    void transmitDatagram(QHostAddress dstHost, QByteArray *datagram);
    void sendDownloadRequest(quint8 protocol, QHostAddress dstHost, QByteArray tth, quint64 offset, quint64 length, quint32 segmentId);
    void sendTransferError(QHostAddress dstHost, quint8 error, QByteArray tth, quint64 offset);
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
    void setSegmentStart(qint64 start);
    void setSegmentEnd(qint64 end);
    qint64 getSegmentStart();
    qint64 getSegmentEnd();
    qint64 getSegmentStartTime();
    QHostAddress getSegmentRemotePeer();
    int getSegmentStatus();
    void setRemoteHost(QHostAddress host);
    virtual void startUploading() = 0;
    virtual void startDownloading() = 0;
    virtual void pauseDownload() = 0;
    virtual void unpauseDownload() = 0;
    void setDownloadBucketTablePointer(QHash<int, QByteArray*> *dbt);
    void setSegmentId(quint32 id);

    virtual qint64 getBytesReceivedNotFlushed();
    virtual qint64 getMaxUploadRequestOffset();

protected:
    void calculateLastBucketParams();
    QByteArray TTH;
    QString filePathName;
    quint64 fileSize;
    QHostAddress remoteHost;
    qint64 segmentStart;
    qint64 segmentLength;
    qint64 segmentEnd;
    qint64 segmentStartTime;
    qint64 maxUploadRequestOffset;
    int status;
    int prev_status;

    int lastBucketNumber;
    int lastBucketSize;

    quint32 segmentId;

    QFile inputFile;
    QHash<int, QByteArray*> *pDownloadBucketTable;

    Transfer *pParent;

    virtual void checkSendDownloadRequest(quint8 protocol, QHostAddress peer, QByteArray TTH,
                                         quint64 requestingOffset, quint64 requestingLength, int status);
    virtual int calculateBucketNumber(quint64 fileOffset);
};

#endif // TRANSFERSEGMENT_H
