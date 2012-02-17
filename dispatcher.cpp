#include "dispatcher.h"

#ifdef Q_WS_WIN //If windows
#include <winsock2.h>
#include <ws2tcpip.h>
#else //If Q_OS_LINUX
#include <sys/socket.h>
#include <sys/types.h>
#endif

Dispatcher::Dispatcher(QHostAddress ip, quint16 port, QObject *parent) :
    QObject(parent)
{
    // conf
    dispatchIP = ip;
    dispatchPort = port;
    mcastAddress = QHostAddress("239.255.40.12");
    bcastAddress = QHostAddress("255.255.255.255");
    protocolCapabilityBitmask = 0;
    maximumSendBufferSize = 0;

    // Init P2P dispatch socket
    receiverUdpSocket = new QUdpSocket(this);
    receiverUdpSocket->bind(dispatchPort, QUdpSocket::ShareAddress);
#if QT_VERSION >= 0x040800
    receiverUdpSocket->joinMulticastGroup(mcastAddress);
    receiverUdpSocket->setSocketOption(QAbstractSocket::MulticastTtlOption, 16);
    receiverUdpSocket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, false);
#endif
    connect(receiverUdpSocket, SIGNAL(readyRead()), this, SLOT(receiveP2PData()));

    //Set UDP receiving buffer to 10MB
    int size = 10 * 1<<20;
    if (::setsockopt(receiverUdpSocket->socketDescriptor(), SOL_SOCKET, SO_RCVBUF, (char *)&size, sizeof(size)) == -1)
    {
        qDebug() << "Dispatcher::Constructor: Could not set receiving buffer to 10MB";
    }

    senderUdpSocket = new QUdpSocket(this);

    // Bootstrapping
    networkBootstrap = new NetworkBootstrap(this);
    connect(networkBootstrap, SIGNAL(bootstrapStatusChanged(int)), this, SLOT(changeBootstrapStatus(int)));
    connect(networkBootstrap, SIGNAL(sendBroadcastAnnounce()), this, SLOT(sendBroadcastAnnounce()));
    connect(networkBootstrap, SIGNAL(sendMulticastAnnounce()), this, SLOT(sendMulticastAnnounce()));
    connect(networkBootstrap, SIGNAL(sendRequestAllBuckets(QHostAddress)),
            this, SLOT(requestAllBuckets(QHostAddress)));
    connect(networkBootstrap, SIGNAL(requestLastKnownPeers()), this, SIGNAL(requestLastKnownPeers()));
    connect(this, SIGNAL(sendLastKnownPeers(QList<QHostAddress>)), networkBootstrap, SLOT(receiveLastKnownPeers(QList<QHostAddress>)));
    connect(networkBootstrap, SIGNAL(appendChatLine(QString)), this, SIGNAL(appendChatLine(QString)));
        
    // Network topology manager
    networkTopology = new NetworkTopology(this);
    connect(networkTopology, SIGNAL(sendForwardAnnouncement(QHostAddress)), this, SLOT(sendUnicastAnnounceForwardRequest(QHostAddress)));
    connect(this, SIGNAL(announceReplyArrived(bool,QHostAddress&,QByteArray&,QByteArray&)),
            networkTopology, SLOT(announceReplyArrived(bool,QHostAddress&,QByteArray&,QByteArray&)));
    connect(this, SIGNAL(announceForwardArrived(QHostAddress&,QByteArray&,QByteArray&)),
            networkTopology, SLOT(announceForwardReplyArrived(QHostAddress&,QByteArray&,QByteArray&)));
    connect(networkTopology, SIGNAL(sendUnicastAnnounce(QHostAddress)), this, SLOT(sendUnicastAnnounce(QHostAddress)));
    connect(this, SIGNAL(bucketContentsArrived(QByteArray, QHostAddress)), networkTopology, SLOT(bucketContentsArrived(QByteArray, QHostAddress)));
    connect(networkBootstrap, SIGNAL(initiateBucketExchanges()), networkTopology, SLOT(initiateBucketRequests()));
    connect(networkTopology, SIGNAL(requestBucketContents(QHostAddress)), this, SLOT(requestBucketContents(QHostAddress)));
    connect(networkTopology, SIGNAL(requestAllBuckets(QHostAddress)), this, SLOT(requestAllBuckets(QHostAddress)));
    connect(networkTopology, SIGNAL(changeBootstrapStatus(int)), networkBootstrap, SLOT(setBootstrapStatus(int)));
    connect(networkBootstrap, SIGNAL(bootstrapStatusChanged(int)), networkTopology, SLOT(setBootstrapStatus(int)));
    connect(networkTopology, SIGNAL(saveLastKnownPeers(QList<QHostAddress>)), this, SIGNAL(saveLastKnownPeers(QList<QHostAddress>)));

    // Rejoin multicast timer
    rejoinMulticastTimer = new QTimer(this);
    rejoinMulticastTimer->setInterval(300000);
    rejoinMulticastTimer->setSingleShot(false);
    connect(rejoinMulticastTimer, SIGNAL(timeout()), this, SLOT(rejoinMulticastTimeout()));
    rejoinMulticastTimer->start();
}

Dispatcher::~Dispatcher()
{
    networkBootstrap->deleteLater();
#if QT_VERSION >= 0x040800
    receiverUdpSocket->leaveMulticastGroup(mcastAddress);
#endif
    rejoinMulticastTimer->deleteLater();
    networkTopology->deleteLater();
    senderUdpSocket->deleteLater();
    receiverUdpSocket->deleteLater();
}

void Dispatcher::reconfigureDispatchHostPort(QHostAddress ip, quint16 port)
{
    dispatchIP = ip;
    dispatchPort = port;
    disconnect(receiverUdpSocket, SIGNAL(readyRead()));
#if QT_VERSION >= 0x040800
    receiverUdpSocket->leaveMulticastGroup(mcastAddress);
#endif
    receiverUdpSocket->close();
    receiverUdpSocket->bind(dispatchPort, QUdpSocket::ShareAddress);

    //Set UDP receiving buffer to 10MB
    int size = 10 * 1<<20;
    if (::setsockopt(receiverUdpSocket->socketDescriptor(), SOL_SOCKET, SO_RCVBUF, (char *)&size, sizeof(size)) == -1)
    {
        qDebug() << "Dispatcher::Constructor: Could not set receiving buffer to 10MB";
    }

    connect(receiverUdpSocket, SIGNAL(readyRead()), this, SLOT(receiveP2PData()));
#if QT_VERSION >= 0x040800
    receiverUdpSocket->joinMulticastGroup(mcastAddress);
    receiverUdpSocket->setSocketOption(QAbstractSocket::MulticastTtlOption, 16);
    receiverUdpSocket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, false);
#endif
}

// ------------------=====================   Initial receive and dispatching   =====================----------------------

void Dispatcher::receiveP2PData()
{
    while (receiverUdpSocket->hasPendingDatagrams())
    {
        QByteArray datagram;
        QHostAddress senderHost;
        quint16 senderPort; // ignoreer
        datagram.resize(receiverUdpSocket->pendingDatagramSize());
        receiverUdpSocket->readDatagram(datagram.data(), datagram.size(), &senderHost, &senderPort);
        //QByteArray datagramType(datagram.left(1));
        //QByteArray protocolInstruction(datagram.mid(1,1));
        //quint8 quint8DatagramType = datagramType.at(0); // hy wil graag mooi gevra wees om by die rou byte uit te kom...
        //quint8 quint8ProtocolInstruction = protocolInstruction.at(0);
        quint8 quint8DatagramType = datagram.at(0);
        quint8 quint8ProtocolInstruction = datagram.at(1);
        switch(quint8DatagramType)
        {
        case DirectDataPacket:
            dispatchDirectDataPacket(datagram);
            break;

        case DataPacket:
            emit incomingDataPacket(quint8ProtocolInstruction, senderHost, datagram);
            break;

        case MulticastPacket:
            handleProtocolInstruction(quint8DatagramType, quint8ProtocolInstruction, datagram, senderHost);
            emit multicastPacketReceived();
            break;

        case BroadcastPacket:
            handleProtocolInstruction(quint8DatagramType, quint8ProtocolInstruction, datagram, senderHost);
            emit broadcastPacketReceived();
            break;

        case UnicastPacket:
            handleProtocolInstruction(quint8DatagramType, quint8ProtocolInstruction, datagram, senderHost);
            emit unicastPacketReceiced();
            break;

        default:
            emit invalidPacketReceived();
        }
    }
}

// ------------------=====================   Protocol splitter and dispatcher   =====================----------------------

void Dispatcher::handleProtocolInstruction(quint8 &quint8DatagramType, quint8 &quint8ProtocolInstruction, QByteArray &datagram,
                                           QHostAddress &senderHost)
{
    // try to sort this from most frequently used to less frequently used
    switch(quint8ProtocolInstruction)
    {
    case TTHSearchRequestPacket:
        handleReceivedTTHSearchQuestion(senderHost, datagram);
        break;

    case TTHSearchResultPacket:
        handleArrivedTTHSearchResult(senderHost, datagram);
        break;

    case TTHSearchForwardRequestPacket:
        handleReceivedTTHSearchForwardRequest(senderHost, datagram);
        break;

    case SearchResultPacket:
        parseArrivedSearchResult(datagram, senderHost);
        break;

    case BucketExchangePacket:
        emit bucketContentsArrived(datagram.mid(2), senderHost);
        break;

    case RequestBucketPacket:
        sendLocalBucket(senderHost);
        break;

    case RequestAllBucketsPacket:
        sendAllBuckets(senderHost);
        break;

    // incoming announcement, send to buckets, reply with AnnounceReply
    case AnnouncePacket:
        switch (quint8DatagramType)
        {
        case MulticastPacket:
            sendMulticastAnnounceReply();
            break;
        case BroadcastPacket:
            sendBroadcastAnnounceReply();
        }
        handleReceivedAnnounce(quint8DatagramType, senderHost, datagram);
        break;

    // same as announce, just from different container, so alleged address and fromaddress may not match
    case AnnounceForwardedPacket:
        sendUnicastAnnounceReply(senderHost);
        break;

    // incoming keepalive ping or responses on container wide announces, just send to buckets
    case AnnounceReplyPacket:
        handleReceivedAnnounce(quint8DatagramType, senderHost, datagram);
        break;

    case SearchRequestPacket:
        handleReceivedSearchQuestion(senderHost, datagram);
        break;

    case DownloadRequestPacket:
        handleIncomingUploadRequest(senderHost, datagram);
        break;

    case SearchForwardRequestPacket:
        handleReceivedSearchForwardRequest(senderHost, datagram);
        break;

    case AnnounceForwardRequestPacket:
        handleReceivedAnnounceForwardRequest(senderHost, datagram);
        break;

    case TransferErrorPacket:
        handleReceivedTransferError(senderHost, datagram);
        break;

    case ProtocolCapabilityQueryPacket:
        handleReceivedProtocolCapabilityQuery(senderHost);
        break;

    case ProtocolCapabilityResponsePacket:
        handleReceivedProtocolCapabilityResponse(senderHost, datagram);
        break;

    case CIDPingPacket:
        handleCIDPingReply(datagram, senderHost);
        break;

    case CIDPingForwardRequestPacket:
        handleReceivedCIDPingForwardRequest(senderHost, datagram);
        break;

    case CIDPingForwardedPacket:
        handleCIDPingForwardedReply(datagram);
        break;

    case CIDPingReplyPacket:
        handleReceivedCIDReply(senderHost, datagram);
        break;

    case TTHTreeRequestPacket:
        handleReceivedTTHTreeRequest(senderHost, datagram);
        break;

    case TTHTreeReplyPacket:
        handleReceivedTTHTree(datagram);
        break;

    // NAT traversal
    case RevConnectPacket:
        break;

    case RevConnectReplyPacket:
        break;

    default:
        emit invalidPacketReceived();

    }
}

void Dispatcher::dispatchDirectDataPacket(QByteArray datagram)
{
    datagram.remove(0, 2);
    quint64 offset = getQuint64FromByteArray(&datagram);
    quint32 segmentId = getQuint32FromByteArray(&datagram);
    emit incomingDirectDataPacket(segmentId, offset, datagram);
}

// ------------------=====================   Network announcement functions   =====================----------------------

void Dispatcher::sendMulticastAnnounce()
{
    QByteArray datagram;
    datagram.reserve(50);
    datagram.append(MulticastPacket);
    datagram.append(AnnouncePacket);
    datagram.append(CID);
    datagram.append(networkTopology->getOwnBucketId());
    sendMulticastRawDatagram(datagram);
}

void Dispatcher::sendBroadcastAnnounce()
{
    QByteArray datagram;
    datagram.reserve(50);
    datagram.append(BroadcastPacket);
    datagram.append(AnnouncePacket);
    datagram.append(CID);
    datagram.append(networkTopology->getOwnBucketId());
    sendBroadcastRawDatagram(datagram);
}

void Dispatcher::sendUnicastAnnounce(QHostAddress dst)
{
    // Unicast announces should be treated as forwarded, so that they do not confuse other bucket IDs elsewhere
    QByteArray *datagram = new QByteArray;
    datagram->reserve(54);
    datagram->append(UnicastPacket);
    datagram->append(AnnounceForwardedPacket);
    datagram->append(quint32ToByteArray(dispatchIP.toIPv4Address()));
    datagram->append(CID);
    datagram->append(networkTopology->getOwnBucketId());
    sendUnicastRawDatagram(dst, datagram);
}

void Dispatcher::sendMulticastAnnounceReply()
{
    QByteArray datagram;
    datagram.reserve(50);
    datagram.append(MulticastPacket);
    datagram.append(AnnounceReplyPacket);
    datagram.append(CID);
    datagram.append(networkTopology->getOwnBucketId());
    sendMulticastRawDatagram(datagram);
}

void Dispatcher::sendBroadcastAnnounceReply()
{
    QByteArray datagram;
    datagram.reserve(50);
    datagram.append(BroadcastPacket);
    datagram.append(AnnounceReplyPacket);
    datagram.append(CID);
    datagram.append(networkTopology->getOwnBucketId());
    sendBroadcastRawDatagram(datagram);
}

void Dispatcher::sendUnicastAnnounceReply(QHostAddress &dstHost)
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 cutoffTime = currentTime - 60000; // 1 minute
    qint64 age = announceForwardToHostTimestamps.value(dstHost);
    if (age != 0)
    {
        if (age >= cutoffTime)
        {
            return;
        }
    }

    announceForwardToHostTimestamps[dstHost] = currentTime;
    
    QByteArray *datagram = new QByteArray;
    datagram->reserve(50);
    datagram->append(UnicastPacket);
    datagram->append(AnnounceReplyPacket);
    datagram->append(CID);
    datagram->append(networkTopology->getOwnBucketId());
    sendUnicastRawDatagram(dstHost, datagram);
}

void Dispatcher::sendUnicastAnnounceForwardRequest(QHostAddress toAddr)
{
    QByteArray *datagram = new QByteArray;
    datagram->reserve(54);
    datagram->append(UnicastPacket);
    datagram->append(AnnounceForwardRequestPacket);
    datagram->append(toQByteArray(dispatchIP.toIPv4Address()));
    datagram->append(CID);
    datagram->append(networkTopology->getOwnBucketId());
    sendUnicastRawDatagram(toAddr, datagram);
}

void Dispatcher::handleReceivedAnnounceForwardRequest(QHostAddress &fromHost, QByteArray &datagram)
{
    QByteArray tmp = datagram.mid(2, 4);
    QHostAddress allegedFromHost = QHostAddress(getQuint32FromByteArray(&tmp));
    if (fromHost != allegedFromHost)
        return;

    QByteArray sendData;
    sendData.reserve(54);
    switch (networkBootstrap->getBootstrapStatus())
    {
    case NETWORK_MCAST:
        sendData.append(MulticastPacket);
        sendData.append(AnnounceForwardedPacket);
        sendData.append(datagram.mid(2));
        sendMulticastRawDatagram(sendData);
        break;
    case NETWORK_BCAST:
        sendData.append(BroadcastPacket);
        sendData.append(AnnounceForwardedPacket);
        sendData.append(datagram.mid(2));
        sendBroadcastRawDatagram(sendData);
    }
}

void Dispatcher::handleReceivedForwardedAnnounce(QByteArray &datagram)
{
    QByteArray tmp = datagram.mid(2, 4);
    QHostAddress announcingHost(getQuint32FromByteArray(&tmp));
    datagram.remove(2, 4);
    handleReceivedAnnounce(UnicastPacket, announcingHost, datagram);
}

void Dispatcher::handleReceivedAnnounce(quint8 datagramType, QHostAddress &senderHost, QByteArray &datagram)
{
    if (datagram.length() == 50)
    {
        QByteArray cid = datagram.mid(2, 24);
        QByteArray bucket = datagram.mid(26);

        switch(datagramType)
        {
        case MulticastPacket:
            emit announceReplyArrived(true, senderHost, cid, bucket);
            break;
        case BroadcastPacket:
            emit announceReplyArrived(false, senderHost, cid, bucket);
            break;
        case UnicastPacket:
            emit announceForwardArrived(senderHost, cid, bucket);
            break;
        default:
            emit invalidPacketReceived();
        }
    }
}

// ------------------=====================   Search functions   =====================----------------------

bool Dispatcher::initiateSearch(quint64 searchId, QByteArray searchData)
{
    QByteArray datagram;
    datagram.reserve(400);
    if (networkBootstrap->getBootstrapStatus() == NETWORK_MCAST)
        datagram.append(MulticastPacket);
    else if (networkBootstrap->getBootstrapStatus() == NETWORK_BCAST)
        datagram.append(BroadcastPacket);
    datagram.append(SearchRequestPacket);

    QByteArray searchPacket = assembleSearchPacket(dispatchIP, searchId, searchData);
    datagram.append(searchPacket);
    if (networkBootstrap->getBootstrapStatus() == NETWORK_MCAST)
        sendMulticastRawDatagram(datagram);
    else if (networkBootstrap->getBootstrapStatus() == NETWORK_BCAST)
        sendBroadcastRawDatagram(datagram);

    QList<QHostAddress> forwardingPeers = networkTopology->getForwardingPeers(3);
    QListIterator<QHostAddress> it(forwardingPeers);
    while(it.hasNext())
    {        
        QHostAddress h = it.next();
        sendSearchForwardRequest(h, searchPacket);
    }
    return true;
}

void Dispatcher::sendSearchResult(QHostAddress toHost, QByteArray senderCID, quint64 searchID, QByteArray searchResult)
{
    // TODO: we can lose senderCID in the function call since it is our own CID we are sending here.
    QByteArray *datagram = new QByteArray;
    datagram->reserve(300);
    datagram->append(UnicastPacket);
    datagram->append(SearchResultPacket);
    datagram->append(assembleSearchPacket(dispatchIP, searchID, searchResult, false));  // TODO: add bucket on first result per host
    sendUnicastRawDatagram(toHost, datagram);
}

QByteArray Dispatcher::assembleSearchPacket(QHostAddress &searchingHost, quint64 &searchID, QByteArray &searchData, bool appendBucket)
{
    QByteArray datagram;
    datagram.reserve(300);
    datagram.append(toQByteArray(searchingHost.toIPv4Address()));
    datagram.append(toQByteArray(searchID));
    datagram.append(fixedCIDLength(CID));  // moet seker wees lengte is reg, anders bevark hy die indekse wat kom
    datagram.append(toQByteArray((quint16)searchData.length()));
    datagram.append(searchData);
    if (appendBucket)
        datagram.append(networkTopology->getOwnBucket());
    return datagram;
}

void Dispatcher::parseArrivedSearchResult(QByteArray &datagram, QHostAddress senderHost)
{
    if (datagram.length() <= 40)
    {
        emit invalidPacketReceived();
        return;
    }
    datagram.remove(0, 2);
    QHostAddress allegedSenderHost(getQuint32FromByteArray(&datagram));
    if (senderHost != allegedSenderHost)
        return;

    quint64 searchID = getQuint64FromByteArray(&datagram);
    QByteArray senderCID = datagram.mid(0, 24);
    datagram.remove(0, 24);
    int searchResultLength = getQuint16FromByteArray(&datagram);
    QByteArray searchResult = datagram.mid(0, searchResultLength);
    QByteArray bucket = datagram.mid(searchResultLength);

    if (searchResult.length() > 0)
        emit searchResultsReceived(senderHost, senderCID, searchID, searchResult);

    if (bucket.length() > 0)
        emit bucketContentsArrived(bucket, senderHost);
}

void Dispatcher::sendSearchBroadcast(QByteArray &searchPacket)
{
    QByteArray datagram;
    datagram.reserve(300);
    datagram.append(BroadcastPacket);
    datagram.append(SearchRequestPacket);
    datagram.append(searchPacket);
    sendBroadcastRawDatagram(datagram);
}

void Dispatcher::sendSearchMulticast(QByteArray &searchPacket)
{
    QByteArray datagram;
    datagram.reserve(300);
    datagram.append(MulticastPacket);
    datagram.append(SearchRequestPacket);
    datagram.append(searchPacket);
    sendMulticastRawDatagram(datagram);
}

void Dispatcher::sendSearchForwardRequest(QHostAddress &forwardingNode, QByteArray &searchPacket)
{
    QByteArray *datagram = new QByteArray;
    datagram->reserve(300);
    datagram->append(UnicastPacket);
    datagram->append(SearchForwardRequestPacket);
    datagram->append(searchPacket);
    sendUnicastRawDatagram(forwardingNode, datagram);
}

void Dispatcher::handleReceivedSearchForwardRequest(QHostAddress &fromAddr, QByteArray &datagram)
{
    QByteArray tmp = datagram.mid(2, 4);
    QHostAddress allegedFromHost = QHostAddress(getQuint32FromByteArray(&tmp));
    if (fromAddr != allegedFromHost)
        return;

    QByteArray searchToForward;
    searchToForward.reserve(200);
    if (networkBootstrap->getBootstrapStatus() == NETWORK_MCAST)
        searchToForward.append(MulticastPacket);
    else if (networkBootstrap->getBootstrapStatus() == NETWORK_BCAST)
        searchToForward.append(BroadcastPacket);

    searchToForward.append(SearchRequestPacket);
    searchToForward.append(datagram.mid(2));
    if (networkBootstrap->getBootstrapStatus() == NETWORK_MCAST)
        sendMulticastRawDatagram(searchToForward);
    else if (networkBootstrap->getBootstrapStatus() == NETWORK_BCAST)
        sendBroadcastRawDatagram(searchToForward);
    // else drop silently

    // handle search question while here, in case we don't hear ourselves broadcast it.
    // TODO: add duplicate search killer with moving window somewhere in this mess.
    handleReceivedSearchQuestion(fromAddr, datagram);

    emit searchForwardReceived(); // stats
}

// kan later besluit of ons host adres uit pakkie uit wil parse of van socket af wil kry of wat.
// update: kry uit pakkie sodat forwarded searches na searcher gaan en nie na forwarder nie
//       : TODO: dink oor 'n manier om die DDoS moontlikheid wat hierin skuil aan te spreek :)
void Dispatcher::handleReceivedSearchQuestion(QHostAddress &fromHost, QByteArray &datagram)
{
    datagram.remove(0, 2);
    QHostAddress sendToHost = QHostAddress(getQuint32FromByteArray(&datagram));
    QString q = sendToHost.toString();
    quint64 searchID = getQuint64FromByteArray(&datagram);
    QByteArray clientCID = datagram.left(24);
    datagram.remove(0, 24);
    int searchLength = getQuint16FromByteArray(&datagram);
    QByteArray searchData = datagram.left(searchLength);
    QByteArray bucket = datagram.right(datagram.length() - searchLength);

    if (searchData.length() > 0)
        emit searchQuestionReceived(sendToHost, clientCID, searchID, searchData);

    if (bucket.length() > 0)
        emit bucketContentsArrived(bucket, fromHost);

}

bool Dispatcher::initiateTTHSearch(QByteArray tth)
{
    QByteArray datagram;
    datagram.reserve(26);
    if (networkBootstrap->getBootstrapStatus() == NETWORK_MCAST)
        datagram.append(MulticastPacket);
    else if (networkBootstrap->getBootstrapStatus() == NETWORK_BCAST)
        datagram.append(BroadcastPacket);
    else
        return false;
    datagram.append(TTHSearchRequestPacket);

    datagram.append(tth);
    if (networkBootstrap->getBootstrapStatus() == NETWORK_MCAST)
        sendMulticastRawDatagram(datagram);
    else if (networkBootstrap->getBootstrapStatus() == NETWORK_BCAST)
        sendBroadcastRawDatagram(datagram);

    QList<QHostAddress> forwardingPeers = networkTopology->getForwardingPeers(3);
    QListIterator<QHostAddress> it(forwardingPeers);
    while(it.hasNext())
    {
        QHostAddress h = it.next();
        sendTTHSearchForwardRequest(h, tth);
    }
    return true;
}

void Dispatcher::sendTTHSearchResult(QHostAddress toHost, QByteArray tth)
{
    QByteArray *datagram = new QByteArray;
    datagram->reserve(30);
    datagram->append(UnicastPacket);
    datagram->append(TTHSearchResultPacket);
    datagram->append(toQByteArray(dispatchIP.toIPv4Address()));
    datagram->append(tth);
    sendUnicastRawDatagram(toHost, datagram);
}

void Dispatcher::sendTTHSearchBroadcast(QByteArray &tth)
{
    QByteArray datagram;
    datagram.reserve(30);
    datagram.append(BroadcastPacket);
    datagram.append(TTHSearchRequestPacket);
    datagram.append(toQByteArray(dispatchIP.toIPv4Address()));
    datagram.append(tth);
    sendBroadcastRawDatagram(datagram);
}

void Dispatcher::sendTTHSearchMulticast(QByteArray &tth)
{
    QByteArray datagram;
    datagram.reserve(30);
    datagram.append(MulticastPacket);
    datagram.append(TTHSearchRequestPacket);
    datagram.append(toQByteArray(dispatchIP.toIPv4Address()));
    datagram.append(tth);
    sendMulticastRawDatagram(datagram);
}

void Dispatcher::sendTTHSearchForwardRequest(QHostAddress &forwardingNode, QByteArray &tth)
{
    QByteArray *datagram = new QByteArray;
    datagram->reserve(30);
    datagram->append(UnicastPacket);
    datagram->append(TTHSearchForwardRequestPacket);
    datagram->append(toQByteArray(dispatchIP.toIPv4Address()));
    datagram->append(tth);
    sendUnicastRawDatagram(forwardingNode, datagram);
}

void Dispatcher::handleReceivedTTHSearchForwardRequest(QHostAddress &fromAddr, QByteArray &datagram)
{
    QByteArray tmp = datagram.mid(2, 4);
    QHostAddress allegedFromHost = QHostAddress(getQuint32FromByteArray(&tmp));
    if (fromAddr != allegedFromHost)
        return;

    QByteArray searchToForward;
    searchToForward.reserve(30);
    if (networkBootstrap->getBootstrapStatus() == NETWORK_MCAST)
        searchToForward.append(MulticastPacket);
    else if (networkBootstrap->getBootstrapStatus() == NETWORK_BCAST)
        searchToForward.append(BroadcastPacket);

    searchToForward.append(TTHSearchRequestPacket);
    searchToForward.append(datagram.mid(2));
    if (networkBootstrap->getBootstrapStatus() == NETWORK_MCAST)
        sendMulticastRawDatagram(searchToForward);
    else if (networkBootstrap->getBootstrapStatus() == NETWORK_BCAST)
        sendBroadcastRawDatagram(searchToForward);
    // else drop silently

    handleReceivedTTHSearchQuestion(fromAddr, datagram);

    emit searchForwardReceived(); // stats
}

void Dispatcher::handleArrivedTTHSearchResult(QHostAddress &fromAddr, QByteArray &datagram)
{
    QByteArray tmp = datagram.mid(2, 4);
    QHostAddress allegedFromAddr = QHostAddress(getQuint32FromByteArray(&tmp));
    if (fromAddr != allegedFromAddr) // mainly to catch misconfigured nodes behind NAT
        return;

    QByteArray tth(datagram.mid(6));
    emit TTHSearchResultsReceived(tth, fromAddr);
}

void Dispatcher::handleReceivedTTHSearchQuestion(QHostAddress &fromAddr, QByteArray &datagram)
{
    //QByteArray tmp = datagram.mid(2, 4);
    //QHostAddress allegedFromAddr = QHostAddress(getQuint32FromByteArray(&tmp));
    //if (fromAddr != allegedFromAddr) // mainly to catch misconfigured nodes behind NAT
    //    return;

    //QByteArray tth(datagram.mid(6));
    //emit TTHSearchQuestionReceived(tth, fromAddr);

    // update: results must go to address specified in packet, otherwise they end up at the forwarding node.
    datagram.remove(0, 2);
    QHostAddress sendToHost = QHostAddress(getQuint32FromByteArray(&datagram));
    emit TTHSearchQuestionReceived(datagram, sendToHost);
}

// ------------------=====================   Data transfer functions   =====================----------------------

void Dispatcher::handleIncomingUploadRequest(QHostAddress &fromHost, QByteArray &datagram)
{
    //quint8 protocol = datagram.at(2);
    //QByteArray tth = datagram.mid(3, 24);
    //QByteArray tmp;
    //tmp = datagram.mid(27, 8);
    //quint64 offset = getQuint64FromByteArray(&tmp);
    //tmp = datagram.mid(35, 8);
    //quint64 length = getQuint64FromByteArray(&tmp);

    datagram.remove(0, 2);
    quint8 protocol = getQuint8FromByteArray(&datagram);
    QByteArray tth = datagram.left(24);
    datagram.remove(0, 24);
    qint64 offset = (qint64)getQuint64FromByteArray(&datagram);
    qint64 length = (qint64)getQuint64FromByteArray(&datagram);
    quint32 segmentId = 0;
    if (datagram.length() >= 4)
        segmentId = getQuint32FromByteArray(&datagram);

    emit incomingUploadRequest(protocol, fromHost, tth, offset, length, segmentId);
}

void Dispatcher::sendDownloadRequest(quint8 protocol, QHostAddress dstHost, QByteArray tth, qint64 offset, qint64 length, quint32 segmentId)
{
    QByteArray *datagram = new QByteArray;
    datagram->reserve(43);
    datagram->append(UnicastPacket);
    datagram->append(DownloadRequestPacket);
    datagram->append(protocol);
    datagram->append(tth);
    datagram->append(quint64ToByteArray((quint64)offset));
    datagram->append(quint64ToByteArray((quint64)length));
    datagram->append(quint32ToByteArray(segmentId));
    sendUnicastRawDatagram(dstHost, datagram);
}

void Dispatcher::sendTransferError(QHostAddress dstHost, quint8 error, QByteArray tth, quint64 offset)
{
    QByteArray *datagram = new QByteArray;
    datagram->reserve(35);
    datagram->append(UnicastPacket);
    datagram->append(TransferErrorPacket);
    datagram->append(error);
    datagram->append(tth);
    datagram->append(quint64ToByteArray(offset));
    sendUnicastRawDatagram(dstHost, datagram);
}

void Dispatcher::sendTTHTreeRequest(QHostAddress host, QByteArray tthRoot, quint32 startOffset, quint32 numberOfBuckets)
{
    QByteArray *datagram = new QByteArray;
    datagram->reserve(34);
    datagram->append(UnicastPacket);
    datagram->append(TTHTreeRequestPacket);
    datagram->append(tthRoot);
    datagram->append(toQByteArray(startOffset));
    datagram->append(toQByteArray(numberOfBuckets));
    sendUnicastRawDatagram(host, datagram);
}

void Dispatcher::sendTTHTreeReply(QHostAddress host, QByteArray tthTreePacket)
{
    QByteArray *datagram = new QByteArray;
    datagram->reserve(1400);
    datagram->append(UnicastPacket);
    datagram->append(TTHTreeReplyPacket);
    datagram->append(tthTreePacket);
    sendUnicastRawDatagram(host, datagram);
}

void Dispatcher::handleReceivedTTHTreeRequest(QHostAddress &senderHost, QByteArray &datagram)
{
    QByteArray tth = datagram.mid(2, 24);
    datagram.remove(0, 26);
    quint32 startOffset = getQuint32FromByteArray(&datagram);
    quint32 numberOfBuckets = getQuint32FromByteArray(&datagram);
    emit incomingTTHTreeRequest(senderHost, tth, startOffset, numberOfBuckets);
    qDebug() << "Dispatcher::handleReceivedTTHTreeRequest: Tree request TTH:offset:number" << tth.toBase64() << startOffset << numberOfBuckets;
}

void Dispatcher::handleReceivedTTHTree(QByteArray &datagram)
{
    QByteArray tth = datagram.mid(2, 24);
    QByteArray tree = datagram.mid(26);
    emit receivedTTHTree(tth, tree);
}

void Dispatcher::handleReceivedProtocolCapabilityQuery(QHostAddress host)
{
    QByteArray *datagram = new QByteArray;
    datagram->reserve(3);
    datagram->append(UnicastPacket);
    datagram->append(ProtocolCapabilityResponsePacket);
    datagram->append(protocolCapabilityBitmask);
    sendUnicastRawDatagram(host, datagram);
}

void Dispatcher::handleReceivedProtocolCapabilityResponse(QHostAddress fromHost, QByteArray &datagram)
{
    char capability = datagram.at(2);
    emit incomingProtocolCapabilityResponse(fromHost, capability);
}

void Dispatcher::sendProtocolCapabilityQuery(QHostAddress dstHost)
{
    QByteArray *datagram = new QByteArray;
    datagram->reserve(2);
    datagram->append(UnicastPacket);
    datagram->append(ProtocolCapabilityQueryPacket);
    sendUnicastRawDatagram(dstHost, datagram);
}

void Dispatcher::handleReceivedTransferError(QHostAddress fromHost, QByteArray datagram)
{
    datagram.remove(0, 2);
    quint8 error = getQuint8FromByteArray(&datagram);
    QByteArray tth = datagram.left(24);
    datagram.remove(0, 24);
    quint64 offset = getQuint64FromByteArray(&datagram);
    emit incomingTransferError(fromHost, tth, offset, error);
}

// ------------------=====================   CID functions   =====================----------------------

void Dispatcher::dispatchCIDPing(QByteArray &cid)
{
    switch (networkBootstrap->getBootstrapStatus())
    {
    case NETWORK_MCAST:
        sendMulticastCIDPing(cid);
        break;
    case NETWORK_BCAST:
        sendBroadcastCIDPing(cid);
    }
    QList<QHostAddress> forwardingPeers = networkTopology->getForwardingPeers(3);
    QListIterator<QHostAddress> it(forwardingPeers);
    while(it.hasNext())
    {
        QHostAddress h = it.next();
        sendCIDPingForwardRequest(h, cid);
    }
}

void Dispatcher::sendBroadcastCIDPing(QByteArray &cid)
{
    QByteArray datagram;
    datagram.reserve(26);
    datagram.append(BroadcastPacket);
    datagram.append(CIDPingPacket);
    datagram.append(cid);
    sendBroadcastRawDatagram(datagram);
}

void Dispatcher::sendMulticastCIDPing(QByteArray &cid)
{
    QByteArray datagram;
    datagram.reserve(26);
    datagram.append(MulticastPacket);
    datagram.append(CIDPingPacket);
    datagram.append(cid);
    sendMulticastRawDatagram(datagram);
}

void Dispatcher::sendCIDPingForwardRequest(QHostAddress &forwardingNode, QByteArray &cid)
{
    QByteArray *datagram = new QByteArray;
    datagram->reserve(30);
    datagram->append(UnicastPacket);
    datagram->append(CIDPingForwardRequestPacket);
    datagram->append(toQByteArray(dispatchIP.toIPv4Address()));
    datagram->append(cid);
    sendUnicastRawDatagram(forwardingNode, datagram);
}

// reply to CID pings forwarded by peers
void Dispatcher::handleCIDPingForwardedReply(QByteArray &data)
{
    if (data.length() < 8)
        return;

    if (CID == data.mid(6))
    {
        QByteArray *datagram = new QByteArray;
        QByteArray tmp = data.mid(2, 4);
        QHostAddress dst = QHostAddress(getQuint32FromByteArray(&tmp));
        datagram->append(UnicastPacket);
        datagram->append(CIDPingReplyPacket);
        datagram->append(CID);
        sendUnicastRawDatagram(dst, datagram);
    }
}

// reply to CID pings broadcasted or multicasted directly
void Dispatcher::handleCIDPingReply(QByteArray &data, QHostAddress &dstHost)
{
    if (data.length() < 4)
        return;

    if (CID == data.mid(2))
    {
        QByteArray *datagram = new QByteArray;
        datagram->reserve(26);
        datagram->append(UnicastPacket);
        datagram->append(CIDPingReplyPacket);
        datagram->append(CID);
        sendUnicastRawDatagram(dstHost, datagram);
    }
}

// forward the CID ping to own bucket on request
void Dispatcher::handleReceivedCIDPingForwardRequest(QHostAddress &fromAddr, QByteArray &data)
{
    QByteArray tmp = data.mid(2, 4);
    QHostAddress allegedFromHost = QHostAddress(getQuint32FromByteArray(&tmp));
    if (fromAddr != allegedFromHost)
        return;

    QByteArray datagram;
    datagram.reserve(30);
    switch(networkBootstrap->getBootstrapStatus())
    {
    case NETWORK_MCAST:
        datagram.append(MulticastPacket);
        datagram.append(CIDPingForwardedPacket);
        datagram.append(data.mid(2));
        sendMulticastRawDatagram(datagram);
        break;
    case NETWORK_BCAST:
        datagram.append(BroadcastPacket);
        datagram.append(CIDPingForwardedPacket);
        datagram.append(data.mid(2));
        sendBroadcastRawDatagram(datagram);
        break;
    }
}

void Dispatcher::handleReceivedCIDReply(QHostAddress &fromAddr, QByteArray &datagram)
{
    if (datagram.length() < 4)
        return;

    QByteArray cid = datagram.mid(2);
    emit CIDReplyArrived(fromAddr, cid);
}

// ------------------=====================   Bucket functions   =====================----------------------

void Dispatcher::sendLocalBucket(QHostAddress &host)
{
    QByteArray bucket = networkTopology->getOwnBucket();
    if (bucket.isEmpty())
        return;

    QByteArray *datagram = new QByteArray;
    datagram->reserve(200);
    datagram->append(UnicastPacket);
    datagram->append(BucketExchangePacket);
    datagram->append(bucket);
    sendUnicastRawDatagram(host, datagram);
}

void Dispatcher::sendAllBuckets(QHostAddress &host)
{
    QList<QByteArray> bucketList = networkTopology->getAllBuckets();
    QListIterator<QByteArray> it(bucketList);
    while (it.hasNext())
    {
        QByteArray *datagram = new QByteArray;
        datagram->reserve(200);
        datagram->append(UnicastPacket);
        datagram->append(BucketExchangePacket);
        datagram->append(it.next());
        sendUnicastRawDatagram(host, datagram);
    }
}

void Dispatcher::requestBucketContents(QHostAddress host)
{
    QByteArray *datagram = new QByteArray;
    datagram->reserve(2);
    datagram->append(UnicastPacket);
    datagram->append(RequestBucketPacket);
    sendUnicastRawDatagram(host, datagram);
}

void Dispatcher::requestAllBuckets(QHostAddress host)
{
    QByteArray *datagram = new QByteArray;
    datagram->reserve(2);
    datagram->append(UnicastPacket);
    datagram->append(RequestAllBucketsPacket);
    sendUnicastRawDatagram(host, datagram);
}

// ------------------=====================   Set network scanning ranges   =====================----------------------

void Dispatcher::addNetworkScanRange(quint32 rangeBase, quint32 rangeEnd)
{
    quint32 rangeLength = rangeEnd - rangeBase;
    networkBootstrap->addNetworkScanRange(rangeBase, rangeLength);
}

void Dispatcher::removeNetworkScanRange(quint32 rangeBase)
{
    networkBootstrap->removeNetworkScanRange(rangeBase);
}

void Dispatcher::initiateLinscan()
{
    networkBootstrap->initiateLinscan();
}

// ------------------=====================   Raw transmission functions   =====================----------------------

void Dispatcher::sendUnicastRawDatagram(QHostAddress dstAddress, QByteArray *datagram)
{
    // moontlike aborsie wat wag hier:
    //Warning: Calling this function on a connected UDP socket may result in an error and no packet being sent.
    //If you are using a connected socket, use write() to send datagrams.
    // ...maar write() vat nie dstAddress en dstPort nie...
    // split hom toe maar in 'n bound socket vir read en 'n unbound socket vir write.
    // hoop dit werk.

    //quint32 dst = dstAddress.toIPv4Address();  // watch in debugger

    //if (senderUdpSocket->writeDatagram(*datagram, dstAddress, dispatchPort) == -1)
    //    emit writeUdpUnicastFailed();


    // **************************************
    /* man 7 socket:
       SO_SNDBUF
              Sets or gets the maximum socket send buffer in bytes.  The  ker‐
              nel doubles this value (to allow space for bookkeeping overhead)
              when it is set using setsockopt(2), and this  doubled  value  is
              returned  by  getsockopt(2).   The  default  value is set by the
              /proc/sys/net/core/wmem_default file  and  the  maximum  allowed
              value is set by the /proc/sys/net/core/wmem_max file.  The mini‐
              mum (doubled) value for this option is 2048.
    */
    if (dstAddress.isNull())
    {
        delete datagram;
        return;
    }

    /*if (senderUdpSocket->peerAddress() != dstAddress)
    {
        senderUdpSocket->disconnectFromHost();
        senderUdpSocket->connectToHost(dstAddress, dispatchPort);
    }*/

    QAbstractSocket::SocketState state = senderUdpSocket->state();
    int size = 0;
    int maxSize = getMaximumSendBufferSize();
    socklen_t *s = new socklen_t(sizeof(size));
    if (::getsockopt(senderUdpSocket->socketDescriptor(), SOL_SOCKET, SO_SNDBUF, (char *)&size, s) != -1) //successfully read
    {
        if (size != maxSize) //set if not already set
        {
            size = maxSize;
            if (::setsockopt(senderUdpSocket->socketDescriptor(), SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof(size)) == -1) //couldn't write
            {
                qDebug() << "Dispatcher::sendUnicastRawDatagram: Could not set sending buffer size";
            }
#ifndef Q_OS_LINUX
            else
            {
                //verify if set correctly
                if (::getsockopt(senderUdpSocket->socketDescriptor(), SOL_SOCKET, SO_SNDBUF, (char *)&size, s) != -1) //successfully read
                {
                    if (size != maxSize)
                        qDebug() << "Dispatcher::sendUnicastRawDatagram: Value returned inconsistent with value set " << size << maxSize;
                }
            }
#endif
        }
    }
    delete s;

    int res;
    //if ((res = senderUdpSocket->write(*datagram)) == -1)
    //    emit writeUdpUnicastFailed();

    if (res = senderUdpSocket->writeDatagram(*datagram, dstAddress, dispatchPort) == -1)
        emit writeUdpUnicastFailed();

    delete datagram;
}

void Dispatcher::sendBroadcastRawDatagram(QByteArray &datagram)
{
    //if (senderUdpSocket->writeDatagram(datagram, bcastAddress, dispatchPort) == -1)
    //    emit writeUdpBroadcastFailed();

    //if ((senderUdpSocket->state() != QAbstractSocket::ConnectingState && senderUdpSocket->state() != QAbstractSocket::ConnectedState) && senderUdpSocket->peerAddress() != bcastAddress)
    /*if (senderUdpSocket->peerAddress() != bcastAddress)
    {
        senderUdpSocket->disconnectFromHost();
        senderUdpSocket->connectToHost(bcastAddress, dispatchPort);
    }*/

    int size = 0;
    int maxSize = getMaximumSendBufferSize();
    socklen_t *s = new socklen_t(sizeof(size));
    if (::getsockopt(senderUdpSocket->socketDescriptor(), SOL_SOCKET, SO_SNDBUF, (char *)&size, s) != -1) //successfully read
    {
        if (size != maxSize) //set if not already set
        {
            size = maxSize;
            if (::setsockopt(senderUdpSocket->socketDescriptor(), SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof(size)) == -1) //couldn't write
            {
                qDebug() << "Dispatcher::sendBroadcastRawDatagram: Could not set sending buffer size";
            }
#ifndef Q_OS_LINUX
            else
            {
                //verify if set correctly
                if (::getsockopt(senderUdpSocket->socketDescriptor(), SOL_SOCKET, SO_SNDBUF, (char *)&size, s) != -1) //successfully read
                {
                    if (size != maxSize)
                        qDebug() << "Dispatcher::sendBroadcastRawDatagram: Value returned inconsistent with value set " << size << maxSize;
                }
            }
#endif
        }
    }
    delete s;

    int res;
    //if ((res = senderUdpSocket->write(datagram)) == -1)
    //    emit writeUdpBroadcastFailed();
    if (senderUdpSocket->writeDatagram(datagram, bcastAddress, dispatchPort) == -1)
        emit writeUdpBroadcastFailed();
}

void Dispatcher::sendMulticastRawDatagram(QByteArray &datagram)
{
    //if (senderUdpSocket->writeDatagram(datagram, mcastAddress, dispatchPort) == -1)
    //    emit writeUdpMulticastFailed();

    //if ((senderUdpSocket->state() != QAbstractSocket::ConnectingState && senderUdpSocket->state() != QAbstractSocket::ConnectedState) && senderUdpSocket->peerAddress() != mcastAddress)
    /*if (senderUdpSocket->peerAddress() != mcastAddress)
    {
        senderUdpSocket->disconnectFromHost();
        senderUdpSocket->connectToHost(mcastAddress, dispatchPort);
    }*/

    int size = 0;
    int maxSize = getMaximumSendBufferSize();
    socklen_t *s = new socklen_t(sizeof(size));
    if (::getsockopt(senderUdpSocket->socketDescriptor(), SOL_SOCKET, SO_SNDBUF, (char *)&size, s) != -1) //successfully read
    {
        if (size != maxSize) //set if not already set
        {
            size = maxSize;
            if (::setsockopt(senderUdpSocket->socketDescriptor(), SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof(size)) == -1) //couldn't write
            {
                qDebug() << "Dispatcher::sendMulticastRawDatagram: Could not set sending buffer size";
            }
#ifndef Q_OS_LINUX
            else
            {
                //verify if set correctly
                if (::getsockopt(senderUdpSocket->socketDescriptor(), SOL_SOCKET, SO_SNDBUF, (char *)&size, s) != -1) //successfully read
                {
                    if (size != maxSize)
                        qDebug() << "Dispatcher::sendMulticastRawDatagram: Value returned inconsistent with value set " << size << maxSize;
                }
            }
#endif
        }
    }
    delete s;

    int res;
    //if ((res = senderUdpSocket->write(datagram)) == -1)
    //    emit writeUdpMulticastFailed();
    if (senderUdpSocket->writeDatagram(datagram, mcastAddress, dispatchPort) == -1)
        emit writeUdpMulticastFailed();
}

// ------------------=====================   Misc functions   =====================----------------------

void Dispatcher::setCID(QByteArray &cid)
{
    CID.clear();
    CID.append(fixedCIDLength(cid));
    networkTopology->setCID(CID);
}

void Dispatcher::changeBootstrapStatus(int status)
{
    emit bootstrapStatusChanged(status);
}

QByteArray Dispatcher::fixedCIDLength(QByteArray CID)
{
    return CID.leftJustified(24, (char)0x00, true);
}

void Dispatcher::setProtocolCapabilityBitmask(char protocols)
{
    protocolCapabilityBitmask = protocols;
}

void Dispatcher::rejoinMulticastTimeout()
{
#if QT_VERSION >= 0x040800
    receiverUdpSocket->joinMulticastGroup(mcastAddress);
#endif
}

int Dispatcher::getMaximumSendBufferSize()
{
    if (maximumSendBufferSize == 0)
    {
        int size = 10*(1<<20);
#ifdef Q_OS_LINUX
        QFile file("/proc/sys/net/core/wmem_max");
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QString line = file.readLine();
            size = line.toLong();
            qDebug() << "wmem_max:" << size;
        }
        if (size < 10240)
            size = 10240;
        else if (size > (10*(1<<20)))
            size = 10*(1<<20);
#endif
        maximumSendBufferSize = size;
    }
    return maximumSendBufferSize;
}

// ------------------=====================   GET FUNCTIONS   =====================----------------------

//Get functions to avoid reconfiguration if no change was made
QHostAddress Dispatcher::getDispatchIP()
{
    return dispatchIP;
}

quint16 Dispatcher::getDispatchPort()
{
    return dispatchPort;
}

int Dispatcher::getNumberOfCIDHosts()
{
    return networkTopology->getNumberOfCIDHosts();
}

int Dispatcher::getNumberOfHosts()
{
    return networkTopology->getNumberOfHosts();
}

QByteArray Dispatcher::getCID()
{
    return CID;
}

//GUI user count
void Dispatcher::getHostCount()
{
    emit returnHostCount(networkTopology->getNumberOfHosts());
}

// ------------------=====================   DEBUGGING   =====================----------------------

QString Dispatcher::getDebugBucketsContents()
{
    return networkTopology->getDebugBucketsContents();
}

QString Dispatcher::getDebugCIDHostContents()
{
    return networkTopology->getDebugCIDHostContents();
}

