#include "networkbootstrap.h"

NetworkBootstrap::NetworkBootstrap(QObject *parent) :
    QObject(parent)
{
    // Init scanlist
    // Load hardcoded hints
    QFile file("nodes.dat");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString line;
        line = file.readLine(64);
        while (line.length() >= 8)
        {
            line = file.readLine(64);
            QHostAddress ip;
            if (!ip.setAddress(line.trimmed()))
                continue;
            lastGoodNodes.append(ip);
        }
    }
    // Read dynamically saved hosts from datastore
    // Note: Must reply with empty list if no nodes are stored, otherwise the entries in nodes.dat won't be queried.
    emit requestLastKnownPeers();

    // Init bootstrap
    setBootstrapStatus(-2);
    bootstrapTimer = new QTimer(this);
    bootstrapTimer->setSingleShot(true);
    connect(bootstrapTimer, SIGNAL(timeout()), this, SLOT(performBootstrap()));

    // Init scanning timer
    networkScanTimer = new QTimer(this);
    networkScanTimer->setSingleShot(true);
    connect(networkScanTimer, SIGNAL(timeout()), this, SLOT(networkScanTimerEvent()));

    // Keepalive timer
    keepaliveTimer = new QTimer(this);
    keepaliveTimer->setSingleShot(false);
    connect(keepaliveTimer, SIGNAL(timeout()), this, SLOT(keepaliveTimerEvent()));
    keepaliveTimer->start(300000);

    // Begin bootstrap proses
    bootstrapTimer->start(1000);
    networkScanTimer->start(1500);

    totalScanHosts = 0;
    networkScanTimeouts = 0;
}

NetworkBootstrap::~NetworkBootstrap()
{
    delete bootstrapTimer;
    delete networkScanTimer;
    delete keepaliveTimer;
}

void NetworkBootstrap::receiveLastKnownPeers(QList<QHostAddress> peers)
{
   lastGoodNodes.append(peers);
   QListIterator<QHostAddress> i(lastGoodNodes);
   while (i.hasNext())
   {
       emit sendRequestAllBuckets(i.next());
   }
}

void NetworkBootstrap::performBootstrap()
{
    // bootstrapStatus
    // -2: Absoluut clueless en nowhere
    // -1: Besig om multicast bootstrap te probeer
    //  0: Besig om broadcast bootstrap te probeer
    //  1: Aanvaar broadcast, geen ander nodes naby, het bucket ID gegenereer.
    //  2: Suksesvol gebootstrap op broadcast
    //  3: Suksesvol gebootstrap op multicast
    // receive functions kan value na 1 of 2 set indien suksesvol.
    // gebruik setBootstrapStatus, hy emit sommer die change signal ook.

    switch(bootstrapStatus)
    {
    case NETWORK_BOOTATTEMPT_NONE:
        emit sendMulticastAnnounce();
        setBootstrapStatus(NETWORK_BOOTATTEMPT_MCAST);
        bootstrapTimer->start(7000);
        break;
    case NETWORK_BOOTATTEMPT_MCAST:
        emit sendBroadcastAnnounce();
        setBootstrapStatus(NETWORK_BOOTATTEMPT_BCAST);
        bootstrapTimer->start(8000);
        break;
    case NETWORK_BOOTATTEMPT_BCAST:
        setBootstrapStatus(NETWORK_BOOTATTEMPT_NONE);
        bootstrapTimer->start(1000);
        break;
    case NETWORK_BCAST_ALONE:
        emit sendMulticastAnnounce();
        emit sendBroadcastAnnounce();
        bootstrapTimer->start(15000);
    }
}

void NetworkBootstrap::setBootstrapStatus(int status)
{
    if (bootstrapStatus != status)
    {
        bootstrapStatus = status;
        emit bootstrapStatusChanged(bootstrapStatus);
    }
}

void NetworkBootstrap::networkScanTimerEvent()
{
    networkScanTimeouts++;
    if (networkScanTimeouts == 3)
        networkScanTimeouts = 0;

    if (bootstrapStatus <= 0)
        networkScanTimer->start(1500);
    else if (bootstrapStatus == 1)
        networkScanTimer->start(10000);
    else
    {
        networkScanTimer->start(30000);
        if (networkScanTimeouts == 0)
            emit initiateBucketExchanges();
    }

    if (totalScanHosts == 0)
        return;

    qsrand((quint32)(QDateTime::currentMSecsSinceEpoch()&0xFFFFFFFF));
    for (int i = 0; i < 3; i++)
    {
        quint32 scanHostOffset = qrand() % totalScanHosts;
        QMapIterator<quint32, quint32> it(networkScanRanges);
        quint32 currentOffset = 0;
        QHostAddress scanHost;
        while (it.hasNext())
        {
            currentOffset += it.peekNext().value();
            if (scanHostOffset <= currentOffset)
            {
                scanHost = QHostAddress(it.peekNext().key() + scanHostOffset % it.peekNext().value());
                break;
            }
            else
                it.next();
        }
        qDebug() << "NetworkBootstrap::networkScanTimerEvent(): Scanning host:" << scanHost.toString();
        emit sendRequestAllBuckets(scanHost);
    }
}

void NetworkBootstrap::keepaliveTimerEvent()
{
    if (bootstrapStatus == NETWORK_MCAST)
        emit sendMulticastAnnounceNoReply();
    else if (bootstrapStatus == NETWORK_BCAST)
        emit sendBroadcastAnnounceNoReply();
    // else wait patiently
}

int NetworkBootstrap::getBootstrapStatus()
{
    return bootstrapStatus;
}

void NetworkBootstrap::addNetworkScanRange(quint32 rangeBase, quint32 rangeLength)
{
    if (!networkScanRanges.contains(rangeBase))
    {
        networkScanRanges.insert(rangeBase, rangeLength);
        totalScanHosts += rangeLength;
    }
}

void NetworkBootstrap::removeNetworkScanRange(quint32 rangeBase)
{
    if (networkScanRanges.contains(rangeBase))
    {
        totalScanHosts -= networkScanRanges.value(rangeBase);
        networkScanRanges.remove(rangeBase);
    }
}

void NetworkBootstrap::initiateLinscan()
{
    // TODO: Check whether Timer is already running
    // Stop random network scanner
    networkScanTimer->stop();

    // Init linear scanning timer
    linscanTimer = new QTimer(this);
    connect(linscanTimer, SIGNAL(timeout()), this, SLOT(linscanTimerEvent()));

    // Initialize iterator to networkScanRanges
    linscanIterator = networkScanRanges.begin();

    // Start timer!
    linscanTimer->start(5000);

    qDebug() << "NetworkBootstrap::initiateLinscan(): Starting linear scan";
}


void NetworkBootstrap::killLinscan()
{
    linscanTimer->stop();
    delete linscanTimer;
}

void NetworkBootstrap::linscanTimerEvent()
{
    // TODO: Show more debugging output
    // TODO: Replace unicast with multicast
    quint32 rangeBase = linscanIterator.key();
    quint32 rangeLength = linscanIterator.value();
    qDebug() << "NetworkBootstrap::linscanTimerEvent(): Scanning range " << QHostAddress(rangeBase).toString() << " - " << QHostAddress(rangeBase + rangeLength).toString();
    QString msg = "Scanning range " + QHostAddress(rangeBase).toString() + " - " + QHostAddress(rangeBase + rangeLength).toString();
    emit appendChatLine(msg);

    for (quint32 host = rangeBase; host <= rangeBase + rangeLength; ++host)
    {
        QHostAddress scanHost = QHostAddress(host);
        emit sendRequestAllBuckets(scanHost);
    }

    ++linscanIterator;
    if (linscanIterator==networkScanRanges.end())
        killLinscan();

}
