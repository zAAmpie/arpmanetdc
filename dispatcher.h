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
        TransferErrorPacket=0x20,
        DownloadProtocolARequestPacket=0x21,
        DownloadProtocolBRequestPacket=0x22,
        DownloadProtocolCRequestPacket=0x23,
        DownloadProtocolDRequestPacket=0x24,
        DataPacketA=0x31,
        DataPacketB=0x32,
        DataPacketC=0x33,
        DataPacketD=0x34,
        AnnouncePacket=0x71,
        AnnounceForwardRequestPacket=0x72,
        AnnounceReplyPacket=0x73,
        AnnounceNoReplyPacket=0x74,
        RequestBucketPacket=0x81,
        BucketExchangePacket=0x82,
        CIDPingPacket=0x91,
        CIDPingForwardRequestPacket=0x92,
        CIDPingReplyPacket=0x93
    };

public:
    explicit Dispatcher(QHostAddress &dispatchIP, quint16 &dispatchPort, QObject *parent = 0);
    void setCID(QByteArray &cid);
    void setDispatchIP(QHostAddress &dispatchIP);
    ~Dispatcher();


signals:
    // Bootstrap signals
    void bootstrapStatusChanged(int);
    void announceReplyArrived(bool isMulticast, QHostAddress &hostAddr, quint16 &hostPort, QByteArray &cid, QByteArray &bucket);
    //void multicastAnnounceReplyArrived(QHostAddress &senderHost, quint16 &senderPort, QByteArray &cid);
    //void broadcastAnnounceReplyArrived(QHostAddress &senderHost, quint16 &senderPort, QByteArray &cid);

    // Search signals
    void searchResultsReceived(QHostAddress &senderHost, quint16 &senderPort, QByteArray &senderCID, quint64 &searchID, QString &searchResult);
    void searchForwardReceived();  // for stats

    // P2P network control data arrival signals (for stats/graphs)
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
    //void bucketContentsRequested(QHostAddress &senderHost, quint16 &senderPort);

    // CID
    void CIDReplyArrived(QHostAddress &fromAddr, quint16 &fromPort, QByteArray &cid);

public slots:
    // Bootstrapping
    void sendMulticastAnnounce();
    void sendBroadcastAnnounce();
    void sendMulticastAnnounceNoReply();
    void sendBroadcastAnnounceNoReply();
    void sendUnicastAnnounceForwardRequest(QHostAddress &toAddr, quint16 &toPort);

    // Search
    bool initiateSearch(quint64&, QString&);

    // Download
    bool initiateDownload(QByteArray&, QString&);
    void sendDownloadProtocolARequest(QHostAddress &dstHost, QByteArray &tth, quint64 &offset, quint64 &length);
    void sendDownloadProtocolBRequest(QHostAddress &dstHost, QByteArray &tth, quint64 &offset, quint64 &length);
    void sendDownloadProtocolCRequest(QHostAddress &dstHost, QByteArray &tth, quint64 &offset, quint64 &length);
    void sendDownloadProtocolDRequest(QHostAddress &dstHost, QByteArray &tth, quint64 &offset, quint64 &length);
    void sendTransferError(QHostAddress &dstHost, quint16 &dstPort, quint8 error);

    // Misc
    void sendUnicastRawDatagram(QHostAddress &dstAddress, quint16 &dstPort, QByteArray &datagram);
    void sendBroadcastRawDatagram(QByteArray &datagram);
    void sendMulticastRawDatagram(QByteArray &datagram);

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
    QByteArray assembleSearchPacket(QHostAddress &searchingHost, quint16 &searchingPort, quint64 &searchID, QString &searchString);
    void sendSearchBroadcast(QByteArray &searchPacket);
    void sendSearchMulticast(QByteArray &searchPacket);
    void sendSearchForwardRequest(QHostAddress &forwardingNode, quint16 &forwardingPort, QByteArray &searchPacket);
    void handleReceivedSearchForwardRequest(QByteArray &datagram);
    void parseArrivedSearchResult(QByteArray &datagram);

    // CID related network functions
    void sendBroadcastCIDPing(QByteArray &cid);
    void sendMulticastCIDPing(QByteArray &cid);
    void sendCIDPingForwardRequest(QHostAddress &forwardingNode, quint16 &forwardingPort, QByteArray &cid);
    void handleCIDPingReply(QByteArray &data, QHostAddress &dstHost, quint16 &dstPort);
    void handleReceivedCIDPingForwardRequest(QByteArray &datagram);
    void handleReceivedCIDReply(QHostAddress &fromAddr, quint16 &fromPort, QByteArray &datagram);

    // P2P announcements
    void handleReceivedAnnounceForwardRequest(QByteArray &datagram);
    void handleReceivedAnnounceReply(quint8 &datagramType, QHostAddress &senderHost, quint16 &senderPort, QByteArray &datagram);

    // P2P protocol helper
    void handleProtocolInstruction(quint8 &quint8DatagramType, quint8 &quint8ProtocolInstruction, QByteArray &datagram,
                                   QHostAddress &senderHost, quint16 &senderPort);

    // Buckets
    void sendLocalBucket(QHostAddress &host, quint16 &port);

    // Bootstrap object
    NetworkBootstrap *networkBootstrap;

    // Network topology objects for handling buckets
    NetworkTopology *networkTopology;

    // Misc functions
    QByteArray fixedCIDLength(QByteArray);

};

#endif // DISPATCHER_H
