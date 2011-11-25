#ifndef NETWORKBOOTSTRAP_H
#define NETWORKBOOTSTRAP_H

#include <QObject>
#include <QTimer>
#include <QHostAddress>
#include <QFile>
#include <QHash>
#include <QList>

// bootstrap status definitions
#define NETWORK_BOOTATTEMPT_NONE -2
#define NETWORK_BOOTATTEMPT_MCAST -1
#define NETWORK_BOOTATTEMPT_BCAST 0
#define NETWORK_BCAST_ALONE 1
#define NETWORK_BCAST 2
#define NETWORK_MCAST 3


class NetworkBootstrap : public QObject
{
    Q_OBJECT
public:
    explicit NetworkBootstrap(QObject *parent = 0);
    ~NetworkBootstrap();
    int getBootstrapStatus();

signals:
    // Bootstrap signals
    void bootstrapStatusChanged(int);
    void sendMulticastAnnounce();
    void sendBroadcastAnnounce();

    // Keepalive signals
    void sendMulticastAnnounceNoReply();
    void sendBroadcastAnnounceNoReply();
    void sendUnicastAnnounceForwardRequest(QHostAddress &toAddr);
    void announceReplyArrived(QHostAddress &hostAddr, QByteArray &cid, QByteArray &bucket);

    // Bucket exchange
    void initiateBucketExchanges();
    void sendRequestAllBuckets(QHostAddress toAddr);

public slots:
    // Bootstrapping
    void performBootstrap();
    void setBootstrapStatus(int status);

    // Network scanning ranges
    void addNetworkScanRange(quint32 rangeBase, quint32 rangeLength);
    void removeNetworkScanRange(quint32 rangeBase);


private slots:
    // Timer events
    void networkScanTimerEvent();
    void keepaliveTimerEvent();

private:
    // Bootstrap
    int bootstrapStatus;
    QList<QHostAddress> lastGoodNodes;

    // Timers
    QTimer *bootstrapTimer;
    QTimer *networkScanTimer;
    QTimer *keepaliveTimer;

    // Network scanning ranges
    QMap<quint32, quint32> networkScanRanges;
    quint32 totalScanHosts;
};

#endif // NETWORKBOOTSTRAP_H
