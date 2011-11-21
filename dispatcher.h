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

class Dispatcher : public QObject
{
    Q_OBJECT

    enum MajorPacketType
    {
        DataPacket=0xaa,
        UnicastPacket=0x55,
        BroadcastPacket=0x5a,
        MulticastPacket=0xa5
    };

    enum MinorPacketType
    {
        SearchRequestPacket=0x11,
        SearchForwardRequestPacket=0x12,
        SearchResultPacket=0x13,
        TTHSearchRequestPacket=0x14,
        TTHSearchForwardRequestPacket=0x15,
        TTHSearchResultPacket=0x16,
        TransferErrorPacket=0x20,
        DownloadProtocolARequestPacket=0x21,
        DownloadProtocolBRequestPacket=0x22,
        DownloadProtocolCRequestPacket=0x23,
        DownloadProtocolDRequestPacket=0x24,
        DataPacketA=0x31,
        DataPacketB=0x32,
        DataPacketC=0x33,
        DataPacketD=0x34,
        TTHTreeRequestPacket=0x41,
        TTHTreeReplyPacket=0x42,
        AnnouncePacket=0x71,
        AnnounceForwardRequestPacket=0x72,
        AnnounceForwardedPacket=0x73,
        AnnounceReplyPacket=0x74,
        RequestBucketPacket=0x81,
        RequestAllBucketsPacket=0x82,
        BucketExchangePacket=0x83,
        CIDPingPacket=0x91,
        CIDPingForwardRequestPacket=0x92,
        CIDPingForwardedPacket=0x93,
        CIDPingReplyPacket=0x94,
        RevConnectPacket=0xa1,
        RevConnectReplyPacket=0xa2
    };

public:
    explicit Dispatcher(QHostAddress dispatchIP, quint16 dispatchPort, QObject *parent = 0);
    void setCID(QByteArray &cid);
    void setDispatchIP(QHostAddress &dispatchIP);
    void reconfigureDispatchHostPort(QHostAddress dispatchIP, quint16 dispatchPort);
    ~Dispatcher();


signals:
    // Bootstrap signals
    void bootstrapStatusChanged(int);
    void announceReplyArrived(bool isMulticast, QHostAddress &hostAddr, QByteArray &cid, QByteArray &bucket);
    void announceForwardArrived(QHostAddress &hostAddr, QByteArray &cid, QByteArray &bucket);
    void announceArrived(QHostAddress &hostAddr, QByteArray &cid, QByteArray &bucket);

    // Search signals
    void searchResultsReceived(QHostAddress &senderHost, QByteArray &senderCID, quint64 &searchID, QByteArray &searchResult);
    void searchQuestionReceived(QHostAddress &senderHost, quint64 &searchID, QByteArray &cid, QByteArray &searchData);
    void searchForwardReceived();  // for stats
    void TTHSearchResultsReceived(QByteArray &tth, QHostAddress &peer);
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
    void incomingTTHTreeDatagram(QByteArray &datagram);
    void incomingTTHTreeRequest(QHostAddress &fromHost, QByteArray &datagram);

    // Transfers
    void incomingUploadRequest(quint8 protocolInstruction, QHostAddress &fromHost, QByteArray &tth, quint64 &offset, quint64 &length);
    void incomingDataPacket(quint8 protocolInstruction, QByteArray &datagram);

public slots:
    // Bootstrapping
    void sendMulticastAnnounce();
    void sendBroadcastAnnounce();
    void sendMulticastAnnounceReply();
    void sendBroadcastAnnounceReply();
    void sendUnicastAnnounceReply(QHostAddress &dstHost);
    void sendUnicastAnnounceForwardRequest(QHostAddress &dstAddr);

    // Search
    bool initiateSearch(quint64 &searchID, QByteArray &searchPacket);
    void sendSearchResult(QHostAddress &toHost, QByteArray searchResult);
    bool initiateTTHSearch(QByteArray &tth);
    void sendTTHSearchResult(QHostAddress &toHost, QByteArray &tth);

    // Transfers
    void sendDownloadRequest(quint8 protocol, QHostAddress &dstHost, QByteArray &tth, quint64 &offset, quint64 &length);
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
    QByteArray assembleSearchPacket(QHostAddress &searchingHost, quint64 &searchID, QByteArray &searchData);
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
    void handleIncomingUploadRequest(quint8 protocolInstruction, QHostAddress &fromHost, QByteArray &datagram);

    // Bootstrap object
    NetworkBootstrap *networkBootstrap;

    // Network topology objects for handling buckets
    NetworkTopology *networkTopology;

    // Misc functions
    QByteArray fixedCIDLength(QByteArray);

};

#endif // DISPATCHER_H
