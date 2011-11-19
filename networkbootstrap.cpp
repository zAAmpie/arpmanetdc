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
    //  1: Suksesvol gebootstrap op broadcast
    //  2: Suksesvol gebootstrap op multicast
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
    case  NETWORK_BOOTATTEMPT_BCAST:
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
    // TODO: slim scanning om stuff op ander broadcast domains raak te skiet


    if (bootstrapStatus <= 0)
        networkScanTimer->start(2500);
    else
    {
        networkScanTimer->start(60000);
        emit initiateBucketExchanges();
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
