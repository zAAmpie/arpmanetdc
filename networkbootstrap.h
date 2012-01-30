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

#ifndef NETWORKBOOTSTRAP_H
#define NETWORKBOOTSTRAP_H

#include <QObject>
#include <QTimer>
#include <QHostAddress>
#include <QFile>
#include <QHash>
#include <QList>
#include <QDateTime>

// bootstrap status definitions
#define NETWORK_BOOTATTEMPT_NONE -2
#define NETWORK_BOOTATTEMPT_MCAST -1
#define NETWORK_BOOTATTEMPT_BCAST 0
#define NETWORK_BCAST_ALONE 1
#define NETWORK_BCAST 2
#define NETWORK_MCAST 3


class NetworkBootstrap : public QObject
{
    Q_OBJECT
public:
    explicit NetworkBootstrap(QObject *parent = 0);
    ~NetworkBootstrap();
    int getBootstrapStatus();

signals:
    // Bootstrap signals
    void bootstrapStatusChanged(int);
    void sendMulticastAnnounce();
    void sendBroadcastAnnounce();

    // Keepalive signals
    void sendMulticastAnnounceNoReply();
    void sendBroadcastAnnounceNoReply();
    void announceReplyArrived(QHostAddress &hostAddr, QByteArray &cid, QByteArray &bucket);

    // Bucket exchange
    void initiateBucketExchanges();
    void sendRequestAllBuckets(QHostAddress toAddr);

    // Initialize scanlist
    void requestLastKnownPeers();

    // Debug messages
    void appendChatLine(QString message);

public slots:
    // Bootstrapping
    void performBootstrap();
    void setBootstrapStatus(int status);

    // Network scanning ranges
    void addNetworkScanRange(quint32 rangeBase, quint32 rangeLength);
    void removeNetworkScanRange(quint32 rangeBase);

    // Initialize scanlist
    void receiveLastKnownPeers(QList<QHostAddress> peers);

    // Do linear scan
    void initiateLinscan();
    void killLinscan();

private slots:
    // Timer events
    void networkScanTimerEvent();
    void keepaliveTimerEvent();
    void linscanTimerEvent();

private:
    // Bootstrap
    int bootstrapStatus;
    QList<QHostAddress> lastGoodNodes;

    // Timers
    QTimer *bootstrapTimer;
    QTimer *networkScanTimer;
    QTimer *keepaliveTimer;
    QTimer *linscanTimer;

    // Network scanning ranges
    QMap<quint32, quint32> networkScanRanges;
    quint32 totalScanHosts;
    int networkScanTimeouts;

    // Linear scan iterator
    // A bit hackish, but it does the job
    QMap<quint32, quint32>::iterator linscanIterator;
};

#endif // NETWORKBOOTSTRAP_H
