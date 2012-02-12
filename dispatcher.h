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

#ifndef DISPATCHER_H
#define DISPATCHER_H

// transfer error definitions
#define TRANSFER_ERROR_NOT_SUPPORTED      1
#define TRANSFER_ERROR_FILE_NOT_FOUND     2
#define TRANSFER_ERROR_IO_ERROR           3
#define TRANSFER_ERROR_RESOURCE_NOT_FOUND 4
#define TRANSFER_ERROR_UNKNOWN_ERROR      5

#include <QObject>
#include <QtNetwork/QUdpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QHash>
#include "networkbootstrap.h"
#include "networktopology.h"
#include "util.h"
#include "protocoldef.h"

class Dispatcher : public QObject
{
    Q_OBJECT

public:
    explicit Dispatcher(QHostAddress dispatchIP, quint16 dispatchPort, QObject *parent = 0);
    void setCID(QByteArray &cid);
    void setDispatchIP(QHostAddress &dispatchIP);
    void setProtocolCapabilityBitmask(char protocols);
    void reconfigureDispatchHostPort(QHostAddress dispatchIP, quint16 dispatchPort);
    ~Dispatcher();

    //Get functions to avoid reconfiguration if no change was made
    QHostAddress getDispatchIP();
    quint16 getDispatchPort();
    int getNumberOfCIDHosts();
    int getNumberOfHosts();
    QByteArray getCID();

signals:
    // Bootstrap signals
    void bootstrapStatusChanged(int);
    void announceReplyArrived(bool isMulticast, QHostAddress &hostAddr, QByteArray &cid, QByteArray &bucket);
    void announceForwardArrived(QHostAddress &hostAddr, QByteArray &cid, QByteArray &bucket);
    void announceArrived(QHostAddress &hostAddr, QByteArray &cid, QByteArray &bucket);
    void requestLastKnownPeers(); 
    void sendLastKnownPeers(QList<QHostAddress> peers);
    void saveLastKnownPeers(QList<QHostAddress> peers);

    // Search signals
    void searchResultsReceived(QHostAddress senderHost, QByteArray senderCID, quint64 searchID, QByteArray searchResult);
    void searchQuestionReceived(QHostAddress senderHost, QByteArray senderCID, quint64 searchID, QByteArray searchQuery);
    void searchForwardReceived();  // for stats
    void TTHSearchResultsReceived(QByteArray tth, QHostAddress peer);
    void TTHSearchQuestionReceived(QByteArray tth, QHostAddress senderHost);

    // P2P network control data arrival signals
    // These signals are intended to be used to generate statistics or graphs on control data throughput
    void multicastPacketReceived();
    void broadcastPacketReceived();
    void unicastPacketReceiced();
    void invalidPacketReceived();

    // Socket write error signals
    void writeUdpUnicastFailed();
    void writeUdpBroadcastFailed();
    void writeUdpMulticastFailed();

    // Bucket exchanges
    void bucketContentsArrived(QByteArray bucket, QHostAddress senderHost);

    // CID
    void CIDReplyArrived(QHostAddress &fromAddr, QByteArray &cid);

    // TTH Tree
    void receivedTTHTree(QByteArray tthRoot, QByteArray tthTree);
    void incomingTTHTreeRequest(QHostAddress fromHost, QByteArray tth, quint32 startOffset, quint32 numberOfBuckets);

    // Transfers
    void incomingProtocolCapabilityResponse(QHostAddress fromHost, char capability);
    void incomingUploadRequest(quint8 protocol, QHostAddress fromHost, QByteArray tth, quint64 offset, quint64 length);
    void incomingDataPacket(quint8 protocolInstruction, QHostAddress senderHost, QByteArray datagram);
    //
    // Debug messages
    void appendChatLine(QString message);

    //GUI user count
    void returnHostCount(int count);

public slots:
    // Bootstrapping
    void sendMulticastAnnounce();
    void sendBroadcastAnnounce();
    void sendUnicastAnnounce(QHostAddress dst);
    void sendMulticastAnnounceReply();
    void sendBroadcastAnnounceReply();
    void sendUnicastAnnounceReply(QHostAddress &dstHost);
    void sendUnicastAnnounceForwardRequest(QHostAddress dstAddr);
    void addNetworkScanRange(quint32 rangeBase, quint32 rangeEnd);
    void removeNetworkScanRange(quint32 rangeBase);

    // Search
    bool initiateSearch(quint64 searchID, QByteArray searchPacket);
    void sendSearchResult(QHostAddress toHost, QByteArray senderCID, quint64 searchID, QByteArray searchResult);
    bool initiateTTHSearch(QByteArray tth);
    void sendTTHSearchResult(QHostAddress toHost, QByteArray tth);

    // Transfers
    void sendProtocolCapabilityQuery(QHostAddress dstHost);
    void sendDownloadRequest(quint8 protocol, QHostAddress dstHost, QByteArray tth, quint64 offset, quint64 length);
    void sendTransferError(QHostAddress dstHost, quint8 error);

    // Buckets
    void requestBucketContents(QHostAddress host);
    void requestAllBuckets(QHostAddress host);
    void initiateLinscan();

    // TTH
    void sendTTHTreeRequest(QHostAddress host, QByteArray tthRoot, quint32 startOffset, quint32 numberOfBuckets);
    void sendTTHTreeReply(QHostAddress host, QByteArray tthTreePacket);

    // CID
    void dispatchCIDPing(QByteArray &cid);

    // Misc
    void sendUnicastRawDatagram(QHostAddress dstAddress, QByteArray *datagram);
    void sendBroadcastRawDatagram(QByteArray &datagram);
    void sendMulticastRawDatagram(QByteArray &datagram);

    //GUI user count
    void getHostCount();

    // debugging
    QString getDebugBucketsContents();
    QString getDebugCIDHostContents();

private slots:
    void receiveP2PData();
    void changeBootstrapStatus(int);
    void rejoinMulticastTimeout();

private:
    // CID
    QByteArray CID;

    // Addresses, ports and sockets
    QHostAddress mcastAddress;
    QHostAddress bcastAddress;
    QUdpSocket *receiverUdpSocket;
    QUdpSocket *senderUdpSocket;
    quint16 dispatchPort;
    QHostAddress dispatchIP;

    // Search network functions
    QByteArray assembleSearchPacket(QHostAddress &searchingHost, quint64 &searchID, QByteArray &searchData, bool appendBucket=true);
    void sendSearchBroadcast(QByteArray &searchPacket);
    void sendSearchMulticast(QByteArray &searchPacket);
    void sendSearchForwardRequest(QHostAddress &forwardingNode, QByteArray &searchPacket);
    void handleReceivedSearchForwardRequest(QHostAddress &fromAddr, QByteArray &datagram);
    void parseArrivedSearchResult(QByteArray &datagram, QHostAddress senderHost);
    void handleReceivedSearchQuestion(QHostAddress &fromHost, QByteArray &datagram);
    void sendTTHSearchBroadcast(QByteArray &tth);
    void sendTTHSearchMulticast(QByteArray &tth);
    void sendTTHSearchForwardRequest(QHostAddress &forwardingNode, QByteArray &tth);
    void handleReceivedTTHSearchForwardRequest(QHostAddress &fromAddr, QByteArray &datagram);
    void handleArrivedTTHSearchResult(QHostAddress &fromAddr, QByteArray &datagram);
    void handleReceivedTTHSearchQuestion(QHostAddress &fromHost, QByteArray &datagram);
    void handleReceivedTTHTree(QByteArray &datagram);

    // CID related network functions
    void sendBroadcastCIDPing(QByteArray &cid);
    void sendMulticastCIDPing(QByteArray &cid);
    void sendCIDPingForwardRequest(QHostAddress &forwardingNode, QByteArray &cid);
    void handleCIDPingReply(QByteArray &data, QHostAddress &dstHost);
    void handleCIDPingForwardedReply(QByteArray &data);
    void handleReceivedCIDPingForwardRequest(QHostAddress &fromAddr, QByteArray &datagram);
    void handleReceivedCIDReply(QHostAddress &fromAddr, QByteArray &datagram);

    // P2P announcements
    void handleReceivedAnnounceForwardRequest(QHostAddress &fromHost, QByteArray &datagram);
    void handleReceivedAnnounce(quint8 datagramType, QHostAddress &senderHost, QByteArray &datagram);
    void handleReceivedForwardedAnnounce(QByteArray &datagram);

    // P2P protocol helper
    void handleProtocolInstruction(quint8 &quint8DatagramType, quint8 &quint8ProtocolInstruction, QByteArray &datagram,
                                   QHostAddress &senderHost);

    // Buckets
    void sendLocalBucket(QHostAddress &host);
    void sendAllBuckets(QHostAddress &host);

    // TTH
    void handleReceivedTTHTreeRequest(QHostAddress &host, QByteArray &datagram);

    // Transfers
    void handleReceivedProtocolCapabilityQuery(QHostAddress fromHost);
    void handleReceivedProtocolCapabilityResponse(QHostAddress fromHost, QByteArray &datagram);
    void handleIncomingUploadRequest(QHostAddress &fromHost, QByteArray &datagram);

    // Bootstrap object
    NetworkBootstrap *networkBootstrap;

    // Network topology objects for handling buckets
    NetworkTopology *networkTopology;

    // Protocol capability bitmask
    char protocolCapabilityBitmask;

    // Misc functions
    QByteArray fixedCIDLength(QByteArray);
    int getMaximumSendBufferSize();
    int maximumSendBufferSize;

    // Multicast rejoin timer
    QTimer *rejoinMulticastTimer;

    QHash<QHostAddress, qint64> announceForwardToHostTimestamps;
};

#endif // DISPATCHER_H
