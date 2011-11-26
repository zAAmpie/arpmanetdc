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
    void reconfigureDispatchHostPort(QHostAddress dispatchIP, quint16 dispatchPort);
    ~Dispatcher();

    //Get functions to avoid reconfiguration if no change was made
    QHostAddress getDispatchIP();
    quint16 getDispatchPort();

signals:
    // Bootstrap signals
    void bootstrapStatusChanged(int);
    void announceReplyArrived(bool isMulticast, QHostAddress &hostAddr, QByteArray &cid, QByteArray &bucket);
    void announceForwardArrived(QHostAddress &hostAddr, QByteArray &cid, QByteArray &bucket);
    void announceArrived(QHostAddress &hostAddr, QByteArray &cid, QByteArray &bucket);

    // Search signals
    void searchResultsReceived(QHostAddress senderHost, QByteArray senderCID, quint64 searchID, QByteArray searchResult);
    void searchQuestionReceived(QHostAddress senderHost, QByteArray senderCID, quint64 searchID, QByteArray searchQuery);
    void searchForwardReceived();  // for stats
    void TTHSearchResultsReceived(QByteArray tth, QHostAddress peer);
    void TTHSearchQuestionReceived(QByteArray &tth, QHostAddress &senderHost);

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
    void bucketContentsArrived(QByteArray bucket);

    // CID
    void CIDReplyArrived(QHostAddress &fromAddr, QByteArray &cid);

    // TTH Tree
    void receivedTTHTree(QByteArray tthRoot, QByteArray tthTree);
    void incomingTTHTreeRequest(QHostAddress &fromHost, QByteArray &datagram);

    // Transfers
    void incomingUploadRequest(QByteArray protocolHint, QHostAddress fromHost, QByteArray tth, quint64 offset, quint64 length);
    void incomingDataPacket(quint8 protocolInstruction, QByteArray &datagram);

public slots:
    // Bootstrapping
    void sendMulticastAnnounce();
    void sendBroadcastAnnounce();
    void sendMulticastAnnounceReply();
    void sendBroadcastAnnounceReply();
    void sendUnicastAnnounceReply(QHostAddress &dstHost);
    void sendUnicastAnnounceForwardRequest(QHostAddress &dstAddr);
    void addNetworkScanRange(quint32 rangeBase, quint32 rangeLength);
    void removeNetworkScanRange(quint32 rangeBase);

    // Search
    bool initiateSearch(quint64 &searchID, QByteArray &searchPacket);
    void sendSearchResult(QHostAddress toHost, QByteArray senderCID, quint64 searchID, QByteArray searchResult);
    bool initiateTTHSearch(QByteArray &tth);
    void sendTTHSearchResult(QHostAddress &toHost, QByteArray &tth);

    // Transfers
    //void sendDownloadRequest(QByteArray protocolHint, QHostAddress &dstHost, QByteArray &tth, quint64 &offset, quint64 &length);
    void sendTransferError(QHostAddress &dstHost, quint8 error);

    // Buckets
    void requestBucketContents(QHostAddress host);
    void requestAllBuckets(QHostAddress host);

    // TTH
    void sendTTHTreeRequest(QHostAddress &host, QByteArray &tthRoot);

    // CID
    void dispatchCIDPing(QByteArray &cid);

    // Misc
    void sendUnicastRawDatagram(QHostAddress &dstAddress, QByteArray &datagram);
    void sendBroadcastRawDatagram(QByteArray &datagram);
    void sendMulticastRawDatagram(QByteArray &datagram);

    // debugging
    QString getDebugBucketsContents();
    QString getDebugCIDHostContents();

private slots:
    void receiveP2PData();
    void changeBootstrapStatus(int);

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
    void parseArrivedSearchResult(QByteArray &datagram);
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
    void handleReceivedAnnounce(quint8 &datagramType, QHostAddress &senderHost, QByteArray &datagram);

    // P2P protocol helper
    void handleProtocolInstruction(quint8 &quint8DatagramType, quint8 &quint8ProtocolInstruction, QByteArray &datagram,
                                   QHostAddress &senderHost);

    // Buckets
    void sendLocalBucket(QHostAddress &host);
    void sendAllBuckets(QHostAddress &host);

    // TTH
    void processIncomingTTHTreeRequest(QHostAddress &host, QByteArray &datagram);

    // Transfers
    void handleIncomingUploadRequest(QHostAddress &fromHost, QByteArray &datagram);

    // Bootstrap object
    NetworkBootstrap *networkBootstrap;

    // Network topology objects for handling buckets
    NetworkTopology *networkTopology;

    // Misc functions
    QByteArray fixedCIDLength(QByteArray);

};

#endif // DISPATCHER_H
