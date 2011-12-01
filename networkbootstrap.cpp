#include "networkbootstrap.h"

NetworkBootstrap::NetworkBootstrap(QObject *parent) :
    QObject(parent)
{
    // Init scanlist: Laai lys last known good ip's
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
    networkScanTimer->start(2500);

    totalScanHosts = 0;
}

NetworkBootstrap::~NetworkBootstrap()
{
    delete bootstrapTimer;
    delete networkScanTimer;
    delete keepaliveTimer;
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
        bootstrapTimer->start(5000);
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
    if (bootstrapStatus <= 0)
        networkScanTimer->start(2500);
    else
    {
        networkScanTimer->start(60000);
        emit initiateBucketExchanges();
    }

    if (totalScanHosts == 0)
        return;

    for (int i = 0; i < 3; i++)
    {
        quint32 scanHostOffset = qrand() % totalScanHosts;
        QMapIterator<quint32, quint32> it(networkScanRanges);
        int currentOffset = 0;
        QHostAddress scanHost;
        while (it.hasNext())
        {
            currentOffset += it.peekNext().value();
            if (scanHostOffset <= currentOffset)
            {
                scanHost = QHostAddress(it.peekNext().key() + scanHostOffset % it.peekNext().value());
                qDebug() << "Scanning host:" << scanHost.toString();
                break;
            }
            else
                it.next();
        }
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
