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

#ifndef NETWORKTOPOLOGY_H
#define NETWORKTOPOLOGY_H

#include <QObject>
#include <QHostAddress>
#include <QList>
#include <QHash>
#include <QDateTime>
#include <QTimer>
#include <QFile>
#include "util.h"

typedef QList<QHostAddress> QHostAddressList;
typedef QList<qint64> qint64list;
typedef QPair<QHostAddressList*, qint64list*> HostIntPair;

// bootstrap status definitions
#define NETWORK_BOOTATTEMPT_NONE -2
#define NETWORK_BOOTATTEMPT_MCAST -1
#define NETWORK_BOOTATTEMPT_BCAST 0
#define NETWORK_BCAST_ALONE 1
#define NETWORK_BCAST 2
#define NETWORK_MCAST 3

class NetworkTopology : public QObject
{
    Q_OBJECT
public:
    explicit NetworkTopology(QObject *parent = 0);
    ~NetworkTopology();
    QList<QHostAddress> getForwardingPeers(int peersPerBucket);
    QByteArray getOwnBucketId();
    QByteArray getOwnBucket();
    QList<QByteArray> getAllBuckets();

    QHostAddress getCIDHostAddress(QByteArray &cid);
    void setCID(QByteArray &CID);
    void setDispatchIP(QHostAddress ip);

    int getNumberOfCIDHosts();
    int getNumberOfHosts();

signals:
    void changeBootstrapStatus(int status);
    void requestBucketContents(QHostAddress host);
    void sendForwardAnnouncement(QHostAddress host);
    void sendUnicastAnnounce(QHostAddress host);
    void requestAllBuckets(QHostAddress host);
    void saveLastKnownPeers(QList<QHostAddress> peers);

public slots:
    // ons onderskei tussen hierdie twee sodat foreign buckets nie ons begrip van ons eie bucket id affekteer nie
    void announceReplyArrived(bool isMulticast, QHostAddress &hostAddr, QByteArray &cid, QByteArray &bucket);
    void announceForwardReplyArrived(QHostAddress &hostAddr, QByteArray &cid, QByteArray &bucket);

    void bucketContentsArrived(QByteArray bucket, QHostAddress senderHost);
    void initiateBucketRequests();

    void setBootstrapStatus(int status);

    // debugging
    QString getDebugBucketsContents();
    QString getDebugCIDHostContents();

private slots:
    void bootstrapTimeoutEvent();
    void collectBucketGarbage();
    void saveActivePeers();

private:
    QHash<QByteArray, QHostAddress> CIDHosts;
    // moet hierdie twee lyste se indekse manually in sync hou.
    // stupid manier, maar gee beste sorted value lookup performance.
    QHash<QByteArray, HostIntPair*> buckets;
    // helper func om die mess hier bo in orde te hou
    void updateHostTimestamp(QByteArray &bucket, QHostAddress &host, qint64 age = 0);

    QTimer *bootstrapTimeoutTimer;
    QTimer *savePeersTimer;
    QTimer *garbageCollectTimer;

    void setCIDHostAddress(QByteArray &cid, QHostAddress &host);

    qint64 getHostAge(QByteArray &bucket, QHostAddress &host);
    QByteArray getBucket(QByteArray bucketID);

    QHash<QByteArray, int> ownBucketId;
    bool unbootstrapped;
    bool not_multicast;
    quint64 startupTime;
    int incomingAnnouncementCount;
    QByteArray CID;
    QHostAddress dispatchIP;
    int bootstrapStatus;
    QHash<QHostAddress, qint64> announceToHostTimestamps;
};

#endif // NETWORKTOPOLOGY_H
