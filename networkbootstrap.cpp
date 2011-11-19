#include "networkbootstrap.h"

NetworkBootstrap::NetworkBootstrap(QObject *parent) :
    QObject(parent)
{
    // Init scanlist
    // TODO: laai lys last known good ip's

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
        case -2:  emit sendMulticastAnnounce();
                 setBootstrapStatus(-1);
                 break;
        case -1: emit sendBroadcastAnnounce();
                 setBootstrapStatus(0);
                 break;
        case  0: setBootstrapStatus(-2);

    }
    bootstrapTimer->start(30000);
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
        networkScanTimer->start(60000);
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
