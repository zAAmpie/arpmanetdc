#include "dispatcher.h"

Dispatcher::Dispatcher(QHostAddress &ip,quint16 &port,  QObject *parent) :
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
    //receiverUdpSocket->joinMulticastGroup(mcastAddress);
    // TODO: ondersoek:
    //receiverUdpSocket->setSocketOption(QAbstractSocket::MulticastTtlOption, 16);
    //receiverUdpSocket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, false);
    connect(receiverUdpSocket, SIGNAL(readyRead()), this, SLOT(receiveP2PData()));

    senderUdpSocket = new QUdpSocket(this);

    // Bootstrapping
    networkBootstrap = new NetworkBootstrap(this);
    connect(networkBootstrap, SIGNAL(bootstrapStatusChanged(int)), this, SLOT(changeBootstrapStatus(int)));
    connect(networkBootstrap, SIGNAL(sendBroadcastAnnounce()), this, SLOT(sendBroadcastAnnounce()));
    connect(networkBootstrap, SIGNAL(sendMulticastAnnounce()), this, SLOT(sendMulticastAnnounce()));
    connect(networkBootstrap, SIGNAL(sendUnicastAnnounceForwardRequest(QHostAddress&,quint16&)),
            this, SLOT(sendUnicastAnnounceForwardRequest(QHostAddress&,quint16&)));
    //connect(this, SIGNAL(broadcastAnnounceReplyArrived(QHostAddress&,quint16&,QByteArray&)),
    //        networkBootstrap, SLOT(broadcastAnnounceReplyArrived(QHostAddress&,quint16&,QByteArray&)));
   //connect(this, SIGNAL(multicastAnnounceReplyArrived(QHostAddress&,quint16&,QByteArray&)),
    //        networkBootstrap, SLOT(multicastAnnounceReplyArrived(QHostAddress&,quint16&,QByteArray&)));

    // Network topology manager
    networkTopology = new NetworkTopology(this);
    //connect(networkBootstrap, SIGNAL(announceReplyArrived(QHostAddress&,quint16&,QByteArray&)),
    //        networkTopology, SLOT(announceReplyArrived(QHostAddress&,quint16&,QByteArray&)));
    connect(this, SIGNAL(announceReplyArrived(bool,QHostAddress&,quint16&,QByteArray&,QByteArray&)),
            networkTopology, SLOT(announceReplyArrived(bool,QHostAddress&,quint16&,QByteArray&,QByteArray&)));
    connect(this, SIGNAL(bucketContentsArrived(QByteArray)), networkTopology, SLOT(bucketContentsArrived(QByteArray)));

}

Dispatcher::~Dispatcher()
{
    delete networkBootstrap;
    //receiverUdpSocket->leaveMulticastGroup(mcastAddress);
    delete networkTopology;
    delete senderUdpSocket;
    delete receiverUdpSocket;
}

// ------------------=====================   Initial receive and dispatching   =====================----------------------

void Dispatcher::receiveP2PData()
{
    while (receiverUdpSocket->hasPendingDatagrams())
    {
        QByteArray datagram;
        QHostAddress senderHost;
        quint16 senderPort;
        datagram.resize(receiverUdpSocket->pendingDatagramSize());
        receiverUdpSocket->readDatagram(datagram.data(), datagram.size(), &senderHost, &senderPort);
        QByteArray datagramType(datagram.mid(0,1));
        QByteArray protocolInstruction(datagram.mid(1,1));
        quint8 quint8DatagramType = datagramType.at(0); // hy wil graag mooi gevra wees om by die rou byte uit te kom...
        quint8 quint8ProtocolInstruction = protocolInstruction.at(0);
        switch(quint8DatagramType)
        {
        case DataPacket:
            // TODO: idees: pluk tth af en dispatch na relevante objek in private class scope QList van download objek pointers.
            // emit maybe iets soos invalidDataPacketReceived() indien tth onbekend is vir die stats
            break;

        case MulticastPacket:
            handleProtocolInstruction(quint8DatagramType, quint8ProtocolInstruction, datagram, senderHost, senderPort);
            emit multicastPacketReceived();
            break;

        case BroadcastPacket:
            handleProtocolInstruction(quint8DatagramType, quint8ProtocolInstruction, datagram, senderHost, senderPort);
            emit broadcastPacketReceived();
            break;
        case UnicastPacket:
            handleProtocolInstruction(quint8DatagramType, quint8ProtocolInstruction, datagram, senderHost, senderPort);
            emit unicastPacketReceiced();
            break;

        default:
            emit invalidPacketReceived();
        }
    }
}

// ------------------=====================   Protocol splitter and dispatcher   =====================----------------------

void Dispatcher::handleProtocolInstruction(quint8 &quint8DatagramType, quint8 &quint8ProtocolInstruction, QByteArray &datagram,
                                           QHostAddress &senderHost, quint16 &senderPort)
{
    // try to sort this from most frequently used to less frequently used

    switch(quint8ProtocolInstruction)
    {
    case SearchResultPacket:
        parseArrivedSearchResult(datagram);
        break;

    case BucketExchangePacket:
        emit bucketContentsArrived(datagram.mid(2));
        break;

    case RequestBucketPacket:
        sendLocalBucket(senderHost, senderPort);
        break;

    case AnnouncePacket:
        // TODO: pass to buckets implementation
        break;

    case AnnounceNoReplyPacket:
        // TODO: send to buckets
        break;

    case AnnounceReplyPacket:
        handleReceivedAnnounceReply(quint8DatagramType, senderHost, senderPort, datagram);
        break;

    case SearchRequestPacket:
        // TODO: pass to sharing implementation
        break;

    case DownloadProtocolARequestPacket:
        // TODO: pass to upload transfer implementation
        break;

    case DownloadProtocolBRequestPacket:
        sendTransferError(senderHost, senderPort, TRANSFER_ERROR_NOT_SUPPORTED);
        break;

    case DownloadProtocolCRequestPacket:
        sendTransferError(senderHost, senderPort, TRANSFER_ERROR_NOT_SUPPORTED);
        break;

    case DownloadProtocolDRequestPacket:
        sendTransferError(senderHost, senderPort, TRANSFER_ERROR_NOT_SUPPORTED);
        break;

    case SearchForwardRequestPacket:
        handleReceivedSearchForwardRequest(datagram);
        break;

    case AnnounceForwardRequestPacket:
        handleReceivedAnnounceForwardRequest(datagram);
        break;

    case TransferErrorPacket:
        // TODO: pass aan
        break;

    case CIDPingPacket:
        handleCIDPingReply(datagram, senderHost, senderPort);
        break;

    case CIDPingForwardRequestPacket:
        handleReceivedCIDPingForwardRequest(datagram);
        break;

    case CIDPingReplyPacket:
        handleReceivedCIDReply(senderHost, senderPort, datagram);
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
    datagram.append(toQByteArray(dispatchIP.toIPv4Address()));
    datagram.append(toQByteArray(dispatchPort));
    datagram.append(CID);
    sendMulticastRawDatagram(datagram);
}

void Dispatcher::sendBroadcastAnnounce()
{
    QByteArray datagram;
    datagram.append(BroadcastPacket);
    datagram.append(AnnouncePacket);
    datagram.append(toQByteArray(dispatchIP.toIPv4Address()));
    datagram.append(toQByteArray(dispatchPort));
    datagram.append(CID);
    sendBroadcastRawDatagram(datagram);
}

void Dispatcher::sendMulticastAnnounceNoReply()
{
    QByteArray datagram;
    datagram.append(MulticastPacket);
    datagram.append(AnnounceNoReplyPacket);
    datagram.append(CID);
    sendMulticastRawDatagram(datagram);
}

void Dispatcher::sendBroadcastAnnounceNoReply()
{
    QByteArray datagram;
    datagram.append(BroadcastPacket);
    datagram.append(AnnounceNoReplyPacket);
    datagram.append(CID);
    sendBroadcastRawDatagram(datagram);
}

void Dispatcher::sendUnicastAnnounceForwardRequest(QHostAddress &toAddr, quint16 &toPort)
{
    QByteArray datagram;
    datagram.append(UnicastPacket);
    datagram.append(AnnounceForwardRequestPacket);
    datagram.append(toQByteArray(dispatchIP.toIPv4Address()));
    datagram.append(toQByteArray(dispatchPort));
    datagram.append(CID);
    // TODO: piggyback bucket contents here
    sendUnicastRawDatagram(toAddr, toPort, datagram);
}

void Dispatcher::handleReceivedAnnounceForwardRequest(QByteArray &datagram)
{
    // TODO check ook fromaddress en adres in pakkie
}

void Dispatcher::handleReceivedAnnounceReply(quint8 &datagramType, QHostAddress &senderHost, quint16 &senderPort, QByteArray &datagram)
{
    QByteArray packet = datagram.mid(2);
    if ((datagram.length() == 56) && (senderHost == QHostAddress(datagram.mid(2,4).toUInt())) && (senderPort == datagram.mid(6,2).toUInt()))
    {
        QByteArray cid = packet.mid(8, 24);
        QByteArray bucket = packet.mid(32);
        bool isMulticast = false;
        if (datagramType == MulticastPacket)
            isMulticast = true;

        emit announceReplyArrived(isMulticast, senderHost, senderPort, cid, bucket);
    }

}

// ------------------=====================   Search functions   =====================----------------------

bool Dispatcher::initiateSearch(quint64 &searchId, QString &searchString)
{
    QByteArray datagram;
    if (networkBootstrap->getBootstrapStatus() == NETWORK_MCAST)
        datagram.append(MulticastPacket);
    else if (networkBootstrap->getBootstrapStatus() == NETWORK_BCAST)
        datagram.append(BroadcastPacket);
    else
        return false;

    datagram.append(assembleSearchPacket(dispatchIP, dispatchPort, searchId, searchString));
    if (networkBootstrap->getBootstrapStatus() == NETWORK_MCAST)
        sendMulticastRawDatagram(datagram);
    else if (networkBootstrap->getBootstrapStatus() == NETWORK_BCAST)
        sendBroadcastRawDatagram(datagram);

    datagram.remove(0, 1);
    datagram.prepend(UnicastPacket);
    // TODO:
    // foreach broadcastcontainer: 3-5 lastseen hosts
    //     sendSearchForwardRequest(host, searchId, searchString);
    return true;
}

QByteArray Dispatcher::assembleSearchPacket(QHostAddress &searchingHost, quint16 &searchingPort, quint64 &searchID, QString &searchString)
{
    QByteArray datagram;
    datagram.append(toQByteArray(searchingHost.toIPv4Address()));
    datagram.append(toQByteArray(searchingPort));
    datagram.append(toQByteArray(searchID));
    datagram.append(fixedCIDLength(CID));  // moet seker wees lengte is reg, anders bevark hy die indekse wat kom
    datagram.append(toQByteArray((quint16)searchString.length()));
    datagram.append(searchString);
    // TODO: piggyback bucket contents here
    // datagram.append(localBucketContents);
    return datagram;
}

void Dispatcher::parseArrivedSearchResult(QByteArray &datagram)
{
    if (datagram.length() <= 42)
    {
        emit invalidPacketReceived();
        return;
    }
    QHostAddress senderHost = QHostAddress(datagram.mid(2,4).toUInt());
    quint16 senderPort = (quint16)datagram.mid(6,2).toInt();
    quint64 searchID = datagram.mid(8, 8).toULongLong();
    QByteArray senderCID = datagram.mid(16, 24);
    int searchResultLength = datagram.mid(40, 2).toInt();
    QString searchResult = QString(datagram.mid(42, searchResultLength));
    QByteArray bucket = datagram.mid(42 + searchResultLength);

    if (searchResult.length() > 0)
        emit searchResultsReceived(senderHost, senderPort, senderCID, searchID, searchResult);

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

void Dispatcher::sendSearchForwardRequest(QHostAddress &forwardingNode, quint16 &forwardingPort, QByteArray &searchPacket)
{
    QByteArray datagram;
    datagram.append(UnicastPacket);
    datagram.append(SearchForwardRequestPacket);
    datagram.append(searchPacket);
    sendUnicastRawDatagram(forwardingNode, forwardingPort, datagram);
}

void Dispatcher::handleReceivedSearchForwardRequest(QByteArray &datagram)
{
    QByteArray searchToForward;
    if (networkBootstrap->getBootstrapStatus() == NETWORK_MCAST)
        searchToForward.append(MulticastPacket);
    else if (networkBootstrap->getBootstrapStatus() == NETWORK_BCAST)
        searchToForward.append(BroadcastPacket);

    searchToForward.append(SearchRequestPacket);
    // TODO:
    // ons kan later hier iewers check of die fromAddress en die beweerde adres in die pakkie dieselfde is
    // om cheap-and-easy DDoS floods met 'n rogue client te voorkom :)
    searchToForward.append(datagram.mid(2));
    if (networkBootstrap->getBootstrapStatus() == NETWORK_MCAST)
        sendMulticastRawDatagram(searchToForward);
    else if (networkBootstrap->getBootstrapStatus() == NETWORK_BCAST)
        sendBroadcastRawDatagram(searchToForward);
    // else drop silently

    emit searchForwardReceived();
}

// ------------------=====================   Data transfer functions   =====================----------------------

bool Dispatcher::initiateDownload(QByteArray &tth, QString &fileName)
{
    // TODO: fire 'n objek op wat die inkomende data kan vang onder sy TTH
    // TODO: die objek kan self vir sendDownloadProtocolARequest of sy broers trigger met signals soos nodig.
    // Preferred protokol metode kan in die settings gestel word
	return true;
}

void Dispatcher::sendDownloadProtocolARequest(QHostAddress &dstHost, QByteArray &tth, quint64 &offset, quint64 &length)
{
    // TODO
}

void Dispatcher::sendDownloadProtocolBRequest(QHostAddress &dstHost, QByteArray &tth, quint64 &offset, quint64 &length)
{
    // not implemented yet, probably libutp based transfers
}

void Dispatcher::sendDownloadProtocolCRequest(QHostAddress &dstHost, QByteArray &tth, quint64 &offset, quint64 &length)
{
    // not implemented yet, probably tcp transfers
}

void Dispatcher::sendDownloadProtocolDRequest(QHostAddress &dstHost, QByteArray &tth, quint64 &offset, quint64 &length)
{
    // not implemented yet, probably fountain transfers
}
void Dispatcher::sendTransferError(QHostAddress &dstHost, quint16 &dstPort, quint8 error)
{
    QByteArray sendDatagram;
    sendDatagram.append(UnicastPacket);
    sendDatagram.append(TransferErrorPacket);
    sendDatagram.append(error);
    sendUnicastRawDatagram(dstHost, dstPort, sendDatagram);
}

// ------------------=====================   CID functions   =====================----------------------

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

void Dispatcher::sendCIDPingForwardRequest(QHostAddress &forwardingNode, quint16 &forwardingPort, QByteArray &cid)
{
    QByteArray datagram;
    datagram.append(UnicastPacket);
    datagram.append(CIDPingForwardRequestPacket);
    datagram.append(toQByteArray(dispatchIP.toIPv4Address()));
    datagram.append(toQByteArray(dispatchPort));
    datagram.append(cid);
    sendUnicastRawDatagram(forwardingNode, forwardingPort, datagram);
}

void Dispatcher::handleCIDPingReply(QByteArray &data, QHostAddress &dstHost, quint16 &dstPort)
{
    if (CID == data.mid(2))
    {
        QByteArray datagram;
        datagram.append(UnicastPacket);
        datagram.append(CIDPingReplyPacket);
        datagram.append(CID);
        sendUnicastRawDatagram(dstHost, dstPort, datagram);
    }
}

void Dispatcher::handleReceivedCIDPingForwardRequest(QByteArray &datagram)
{
    // TODO check ook fromaddress en adres in pakkie
}

void Dispatcher::handleReceivedCIDReply(QHostAddress &fromAddr, quint16 &fromPort, QByteArray &datagram)
{
    QByteArray cid = datagram.mid(2);
    if (cid.length() > 0)
         emit CIDReplyArrived(fromAddr, fromPort, cid);
}

// ------------------=====================   Bucket functions   =====================----------------------

void Dispatcher::sendLocalBucket(QHostAddress &host, quint16 &port)
{
    QByteArray bucket = networkTopology->getOwnBucket();
    if (bucket.isEmpty())
        return;

    QByteArray datagram;
    datagram.append(UnicastPacket);
    datagram.append(BucketExchangePacket);
    datagram.append(bucket);
    sendUnicastRawDatagram(host, port, datagram);
}

// ------------------=====================   Raw transmission functions   =====================----------------------

void Dispatcher::sendUnicastRawDatagram(QHostAddress &dstAddress, quint16 &dstPort, QByteArray &datagram)
{
    // moontlike aborsie wat wag hier:
    //Warning: Calling this function on a connected UDP socket may result in an error and no packet being sent.
    //If you are using a connected socket, use write() to send datagrams.
    // ...maar write() vat nie dstAddress en dstPort nie...
    // split hom toe maar in 'n bound socket vir read en 'n unbound socket vir write.
    // hoop dit werk.

    if (senderUdpSocket->writeDatagram(datagram, dstAddress, dstPort) == -1)
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
    CID.append(cid);
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

