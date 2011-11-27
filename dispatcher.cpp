#include "dispatcher.h"

#ifdef Q_WS_WIN
#include <winsock2.h>
#else //If Q_WS_X11
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
        qDebug("Could not set receiving buffer to 10MB");
    }

    senderUdpSocket = new QUdpSocket(this);

    // Bootstrapping
    networkBootstrap = new NetworkBootstrap(this);
    connect(networkBootstrap, SIGNAL(bootstrapStatusChanged(int)), this, SLOT(changeBootstrapStatus(int)));
    connect(networkBootstrap, SIGNAL(sendBroadcastAnnounce()), this, SLOT(sendBroadcastAnnounce()));
    connect(networkBootstrap, SIGNAL(sendMulticastAnnounce()), this, SLOT(sendMulticastAnnounce()));
    connect(networkBootstrap, SIGNAL(sendUnicastAnnounceForwardRequest(QHostAddress&)),
            this, SLOT(sendUnicastAnnounceForwardRequest(QHostAddress&)));
    connect(networkBootstrap, SIGNAL(sendRequestAllBuckets(QHostAddress)),
            this, SLOT(requestAllBuckets(QHostAddress)));

    // Network topology manager
    networkTopology = new NetworkTopology(this);
    connect(this, SIGNAL(announceReplyArrived(bool,QHostAddress&,QByteArray&,QByteArray&)),
            networkTopology, SLOT(announceReplyArrived(bool,QHostAddress&,QByteArray&,QByteArray&)));
    connect(this, SIGNAL(announceForwardArrived(QHostAddress&,QByteArray&,QByteArray&)),
            networkTopology, SLOT(announceForwardReplyArrived(QHostAddress&,QByteArray&,QByteArray&)));
    connect(this, SIGNAL(bucketContentsArrived(QByteArray)), networkTopology, SLOT(bucketContentsArrived(QByteArray)));
    connect(networkBootstrap, SIGNAL(initiateBucketExchanges()), networkTopology, SLOT(initiateBucketRequests()));
    connect(networkTopology, SIGNAL(requestBucketContents(QHostAddress)), this, SLOT(requestBucketContents(QHostAddress)));
    connect(networkTopology, SIGNAL(requestAllBuckets(QHostAddress)), this, SLOT(requestAllBuckets(QHostAddress)));
    connect(networkTopology, SIGNAL(changeBootstrapStatus(int)), networkBootstrap, SLOT(setBootstrapStatus(int)));
    connect(networkBootstrap, SIGNAL(bootstrapStatusChanged(int)), networkTopology, SLOT(setBootstrapStatus(int)));
}

Dispatcher::~Dispatcher()
{
    delete networkBootstrap;
#if QT_VERSION >= 0x040800
    receiverUdpSocket->leaveMulticastGroup(mcastAddress);
#endif
    delete networkTopology;
    delete senderUdpSocket;
    delete receiverUdpSocket;
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
        QByteArray datagramType(datagram.mid(0,1));
        QByteArray protocolInstruction(datagram.mid(1,1));
        quint8 quint8DatagramType = datagramType.at(0); // hy wil graag mooi gevra wees om by die rou byte uit te kom...
        quint8 quint8ProtocolInstruction = protocolInstruction.at(0);
        switch(quint8DatagramType)
        {
        case DataPacket:
            emit incomingDataPacket(quint8ProtocolInstruction, datagram);
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
        parseArrivedSearchResult(datagram);
        break;

    case BucketExchangePacket:
        emit bucketContentsArrived(datagram.mid(2));
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
        // TODO: pass aan
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
        emit incomingTTHTreeRequest(senderHost, datagram);
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


// ------------------=====================   Network announcement functions   =====================----------------------

void Dispatcher::sendMulticastAnnounce()
{
    QByteArray datagram;
    datagram.append(MulticastPacket);
    datagram.append(AnnouncePacket);
    datagram.append(CID);
    datagram.append(networkTopology->getOwnBucketId());
    sendMulticastRawDatagram(datagram);
}

void Dispatcher::sendBroadcastAnnounce()
{
    QByteArray datagram;
    datagram.append(BroadcastPacket);
    datagram.append(AnnouncePacket);
    datagram.append(CID);
    datagram.append(networkTopology->getOwnBucketId());
    sendBroadcastRawDatagram(datagram);
}

void Dispatcher::sendMulticastAnnounceReply()
{
    QByteArray datagram;
    datagram.append(MulticastPacket);
    datagram.append(AnnounceReplyPacket);
    datagram.append(CID);
    datagram.append(networkTopology->getOwnBucketId());
    sendMulticastRawDatagram(datagram);
}

void Dispatcher::sendBroadcastAnnounceReply()
{
    QByteArray datagram;
    datagram.append(BroadcastPacket);
    datagram.append(AnnounceReplyPacket);
    datagram.append(CID);
    datagram.append(networkTopology->getOwnBucketId());
    sendBroadcastRawDatagram(datagram);
}

void Dispatcher::sendUnicastAnnounceReply(QHostAddress &dstHost)
{
    QByteArray datagram;
    datagram.append(UnicastPacket);
    datagram.append(AnnounceReplyPacket);
    datagram.append(CID);
    datagram.append(networkTopology->getOwnBucketId());
    sendUnicastRawDatagram(dstHost, datagram);
}

void Dispatcher::sendUnicastAnnounceForwardRequest(QHostAddress &toAddr)
{
    QByteArray datagram;
    datagram.append(UnicastPacket);
    datagram.append(AnnounceForwardRequestPacket);
    datagram.append(toQByteArray(dispatchIP.toIPv4Address()));
    datagram.append(CID);
    datagram.append(networkTopology->getOwnBucketId());
    sendUnicastRawDatagram(toAddr, datagram);
}

void Dispatcher::handleReceivedAnnounceForwardRequest(QHostAddress &fromHost, QByteArray &datagram)
{
    QByteArray tmp = datagram.mid(2, 4);
    QHostAddress allegedFromHost = QHostAddress(getQuint32FromByteArray(&tmp));
    if (fromHost != allegedFromHost)
        return;

    QByteArray sendData;
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

void Dispatcher::handleReceivedAnnounce(quint8 &datagramType, QHostAddress &senderHost, QByteArray &datagram)
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

bool Dispatcher::initiateSearch(quint64 &searchId, QByteArray &searchData)
{
    QByteArray datagram;
    if (networkBootstrap->getBootstrapStatus() == NETWORK_MCAST)
        datagram.append(MulticastPacket);
    else if (networkBootstrap->getBootstrapStatus() == NETWORK_BCAST)
        datagram.append(BroadcastPacket);
    else
        return false;
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
    QByteArray datagram;
    datagram.append(UnicastPacket);
    datagram.append(SearchResultPacket);
    datagram.append(assembleSearchPacket(dispatchIP, searchID, searchResult, false));  // TODO: add bucket on first result per host
    sendUnicastRawDatagram(toHost, datagram);
}

QByteArray Dispatcher::assembleSearchPacket(QHostAddress &searchingHost, quint64 &searchID, QByteArray &searchData, bool appendBucket)
{
    QByteArray datagram;
    datagram.append(toQByteArray(searchingHost.toIPv4Address()));
    datagram.append(toQByteArray(searchID));
    datagram.append(fixedCIDLength(CID));  // moet seker wees lengte is reg, anders bevark hy die indekse wat kom
    datagram.append(toQByteArray((quint16)searchData.length()));
    datagram.append(searchData);
    if (appendBucket)
        datagram.append(networkTopology->getOwnBucket());
    return datagram;
}

void Dispatcher::parseArrivedSearchResult(QByteArray &datagram)
{
    if (datagram.length() <= 40)
    {
        emit invalidPacketReceived();
        return;
    }
    QByteArray tmp = datagram.mid(2,4);
    QHostAddress senderHost = QHostAddress(getQuint32FromByteArray(&tmp));
    tmp = datagram.mid(6, 8);
    quint64 searchID = getQuint64FromByteArray(&tmp);
    QByteArray senderCID = datagram.mid(14, 24);
    tmp = datagram.mid(38, 2);
    int searchResultLength = getQuint16FromByteArray(&tmp);
    QByteArray searchResult = datagram.mid(40, searchResultLength);
    QByteArray bucket = datagram.mid(40 + searchResultLength);

    if (searchResult.length() > 0)
        emit searchResultsReceived(senderHost, senderCID, searchID, searchResult);

    if (bucket.length() > 0)
        emit bucketContentsArrived(bucket);
}

void Dispatcher::sendSearchBroadcast(QByteArray &searchPacket)
{
    QByteArray datagram;
    datagram.append(BroadcastPacket);
    datagram.append(SearchRequestPacket);
    datagram.append(searchPacket);
    sendBroadcastRawDatagram(datagram);
}

void Dispatcher::sendSearchMulticast(QByteArray &searchPacket)
{
    QByteArray datagram;
    datagram.append(MulticastPacket);
    datagram.append(SearchRequestPacket);
    datagram.append(searchPacket);
    sendMulticastRawDatagram(datagram);
}

void Dispatcher::sendSearchForwardRequest(QHostAddress &forwardingNode, QByteArray &searchPacket)
{
    QByteArray datagram;
    datagram.append(UnicastPacket);
    datagram.append(SearchForwardRequestPacket);
    datagram.append(searchPacket);
    sendUnicastRawDatagram(forwardingNode, datagram);
}

void Dispatcher::handleReceivedSearchForwardRequest(QHostAddress &fromAddr, QByteArray &datagram)
{
    QByteArray tmp = datagram.mid(2, 4);
    QHostAddress allegedFromHost = QHostAddress(getQuint32FromByteArray(&tmp));
    if (fromAddr != allegedFromHost)
        return;

    QByteArray searchToForward;
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

    emit searchForwardReceived(); // stats
}

// kan later besluit of ons host adres uit pakkie uit wil parse of van socket af wil kry of wat.
void Dispatcher::handleReceivedSearchQuestion(QHostAddress &fromHost, QByteArray &datagram)
{
    QByteArray tmp = datagram.mid(6, 8);
    quint64 searchID = getQuint64FromByteArray(&tmp);
    QByteArray clientCID = datagram.mid(14, 24);
    tmp = datagram.mid(38, 2);
    int searchLength = getQuint16FromByteArray(&tmp);
    QByteArray searchData = datagram.mid(40, searchLength);
    QByteArray bucket = datagram.mid(40 + searchLength);

    if (searchData.length() > 0)
        emit searchQuestionReceived(fromHost, clientCID, searchID, searchData);

    if (bucket.length() > 0)
        emit bucketContentsArrived(bucket);

}

bool Dispatcher::initiateTTHSearch(QByteArray &tth)
{
    QByteArray datagram;
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
    QByteArray datagram;
    datagram.append(UnicastPacket);
    datagram.append(TTHSearchResultPacket);
    datagram.append(toQByteArray(dispatchIP.toIPv4Address()));
    datagram.append(tth);
    sendUnicastRawDatagram(toHost, datagram);
}

void Dispatcher::sendTTHSearchBroadcast(QByteArray &tth)
{
    QByteArray datagram;
    datagram.append(BroadcastPacket);
    datagram.append(TTHSearchRequestPacket);
    datagram.append(toQByteArray(dispatchIP.toIPv4Address()));
    datagram.append(tth);
    sendBroadcastRawDatagram(datagram);
}

void Dispatcher::sendTTHSearchMulticast(QByteArray &tth)
{
    QByteArray datagram;
    datagram.append(MulticastPacket);
    datagram.append(TTHSearchRequestPacket);
    datagram.append(toQByteArray(dispatchIP.toIPv4Address()));
    datagram.append(tth);
    sendMulticastRawDatagram(datagram);
}

void Dispatcher::sendTTHSearchForwardRequest(QHostAddress &forwardingNode, QByteArray &tth)
{
    QByteArray datagram;
    datagram.append(UnicastPacket);
    datagram.append(TTHSearchForwardRequestPacket);
    datagram.append(toQByteArray(dispatchIP.toIPv4Address()));
    datagram.append(tth);
    sendUnicastRawDatagram(forwardingNode, datagram);
}

void Dispatcher::handleReceivedTTHSearchForwardRequest(QHostAddress &fromAddr, QByteArray &datagram)
{
    QByteArray tmp = datagram.mid(2, 4);
    QHostAddress allegedFromHost = QHostAddress(getQuint32FromByteArray(&tmp));
    if (fromAddr != allegedFromHost)
        return;

    QByteArray searchToForward;
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
    QByteArray tmp = datagram.mid(2, 4);
    QHostAddress allegedFromAddr = QHostAddress(getQuint32FromByteArray(&tmp));
    if (fromAddr != allegedFromAddr) // mainly to catch misconfigured nodes behind NAT
        return;

    QByteArray tth(datagram.mid(6));
    emit TTHSearchQuestionReceived(tth, fromAddr);
}

// ------------------=====================   Data transfer functions   =====================----------------------

void Dispatcher::handleIncomingUploadRequest(QHostAddress &fromHost, QByteArray &datagram)
{
    QByteArray tth = datagram.mid(2, 24);
    QByteArray tmp;
    tmp = datagram.mid(26, 2);
    quint64 offset = getQuint16FromByteArray(&tmp);
    tmp = datagram.mid(28, 2);
    quint64 length = getQuint16FromByteArray(&tmp);
    QByteArray protocolHint = datagram.mid(30);
    emit incomingUploadRequest(protocolHint, fromHost, tth, offset, length);
}


void Dispatcher::sendDownloadRequest(QByteArray &protocolPreference, QHostAddress &dstHost, QByteArray &tth, quint64 &offset, quint64 &length)
{
    QByteArray datagram;
    datagram.append(UnicastPacket);
    datagram.append(DownloadRequestPacket);
    datagram.append(tth);
    datagram.append(toQByteArray(offset));
    datagram.append(toQByteArray(length));
    datagram.append(protocolPreference);
    sendUnicastRawDatagram(dstHost, datagram);
}

void Dispatcher::sendTransferError(QHostAddress &dstHost, quint8 error)
{
    QByteArray sendDatagram;
    sendDatagram.append(UnicastPacket);
    sendDatagram.append(TransferErrorPacket);
    sendDatagram.append(error);
    sendUnicastRawDatagram(dstHost, sendDatagram);
}

void Dispatcher::sendTTHTreeRequest(QHostAddress host, QByteArray tthRoot)
{
    QByteArray datagram;
    datagram.append(UnicastPacket);
    datagram.append(TTHTreeRequestPacket);
    datagram.append(tthRoot);
    qDebug() << "sendUnicastRawDatagram() " << host << datagram;
    sendUnicastRawDatagram(host, datagram);
}

void Dispatcher::sendTTHTreeReply(QHostAddress host, QByteArray tthTreePacket)
{
    QByteArray datagram;
    datagram.append(UnicastPacket);
    datagram.append(TTHTreeReplyPacket);
    datagram.append(tthTreePacket);
    sendUnicastRawDatagram(host, datagram);
}

void Dispatcher::handleReceivedTTHTree(QByteArray &datagram)
{
    QByteArray tth = datagram.mid(0, 24);
    QByteArray tree = datagram.mid(24);
    emit receivedTTHTree(tth, tree);
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
    datagram.append(BroadcastPacket);
    datagram.append(CIDPingPacket);
    datagram.append(cid);
    sendBroadcastRawDatagram(datagram);
}

void Dispatcher::sendMulticastCIDPing(QByteArray &cid)
{
    QByteArray datagram;
    datagram.append(MulticastPacket);
    datagram.append(CIDPingPacket);
    datagram.append(cid);
    sendMulticastRawDatagram(datagram);
}

void Dispatcher::sendCIDPingForwardRequest(QHostAddress &forwardingNode, QByteArray &cid)
{
    QByteArray datagram;
    datagram.append(UnicastPacket);
    datagram.append(CIDPingForwardRequestPacket);
    datagram.append(toQByteArray(dispatchIP.toIPv4Address()));
    datagram.append(cid);
    sendUnicastRawDatagram(forwardingNode, datagram);
}

// reply to CID pings forwarded by peers
void Dispatcher::handleCIDPingForwardedReply(QByteArray &data)
{
    if (data.length() < 8)
        return;

    if (CID == data.mid(6))
    {
        QByteArray datagram;
        QByteArray tmp = data.mid(2, 4);
        QHostAddress dst = QHostAddress(getQuint32FromByteArray(&tmp));
        datagram.append(UnicastPacket);
        datagram.append(CIDPingReplyPacket);
        datagram.append(CID);
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
        QByteArray datagram;
        datagram.append(UnicastPacket);
        datagram.append(CIDPingReplyPacket);
        datagram.append(CID);
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

    QByteArray datagram;
    datagram.append(UnicastPacket);
    datagram.append(BucketExchangePacket);
    datagram.append(bucket);
    sendUnicastRawDatagram(host, datagram);
}

void Dispatcher::sendAllBuckets(QHostAddress &host)
{
    QList<QByteArray> bucketList = networkTopology->getAllBuckets();
    QListIterator<QByteArray> it(bucketList);
    while (it.hasNext())
    {
        QByteArray datagram;
        datagram.append(UnicastPacket);
        datagram.append(BucketExchangePacket);
        datagram.append(it.next());
        sendUnicastRawDatagram(host, datagram);
    }
}

void Dispatcher::requestBucketContents(QHostAddress host)
{
    QByteArray datagram;
    datagram.append(UnicastPacket);
    datagram.append(RequestBucketPacket);
    sendUnicastRawDatagram(host, datagram);
}

void Dispatcher::requestAllBuckets(QHostAddress host)
{
    QByteArray datagram;
    datagram.append(UnicastPacket);
    datagram.append(RequestAllBucketsPacket);
    sendUnicastRawDatagram(host, datagram);
}

// ------------------=====================   Set network scanning ranges   =====================----------------------

void Dispatcher::addNetworkScanRange(quint32 rangeBase, quint32 rangeLength)
{
    networkBootstrap->addNetworkScanRange(rangeBase, rangeLength);
}

void Dispatcher::removeNetworkScanRange(quint32 rangeBase)
{
    networkBootstrap->removeNetworkScanRange(rangeBase);
}

// ------------------=====================   Raw transmission functions   =====================----------------------

void Dispatcher::sendUnicastRawDatagram(QHostAddress &dstAddress, QByteArray &datagram)
{
    // moontlike aborsie wat wag hier:
    //Warning: Calling this function on a connected UDP socket may result in an error and no packet being sent.
    //If you are using a connected socket, use write() to send datagrams.
    // ...maar write() vat nie dstAddress en dstPort nie...
    // split hom toe maar in 'n bound socket vir read en 'n unbound socket vir write.
    // hoop dit werk.

    //quint32 dst = dstAddress.toIPv4Address();  // watch in debugger
    if (senderUdpSocket->writeDatagram(datagram, dstAddress, dispatchPort) == -1)
        emit writeUdpUnicastFailed();
}

void Dispatcher::sendBroadcastRawDatagram(QByteArray &datagram)
{
    if (senderUdpSocket->writeDatagram(datagram, bcastAddress, dispatchPort) == -1)
        emit writeUdpBroadcastFailed();
}

void Dispatcher::sendMulticastRawDatagram(QByteArray &datagram)
{
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
    /*if (CID.length() < 24)
    {
        for (int i = 0; i < 24 - CID.length(); i++)
            CID.append((char)0x00);
    }
    else if (CID.length() > 24)
        CID.truncate(24);
*/
    return CID.leftJustified(24, (char)0x00, true);
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

// ------------------=====================   DEBUGGING   =====================----------------------

QString Dispatcher::getDebugBucketsContents()
{
    return networkTopology->getDebugBucketsContents();
}

QString Dispatcher::getDebugCIDHostContents()
{
    return networkTopology->getDebugCIDHostContents();
}

