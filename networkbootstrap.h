#ifndef NETWORKBOOTSTRAP_H
#define NETWORKBOOTSTRAP_H

#include <QObject>
#include <QTimer>
#include <QHostAddress>

// bootstrap status definitions
#define NETWORK_BCAST 1
#define NETWORK_MCAST 2


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
    void sendMulticastAnnounceNoReply();
    void sendBroadcastAnnounceNoReply();
    void sendUnicastAnnounceForwardRequest(QHostAddress &toAddr, quint16 &toPort);
    void announceReplyArrived(QHostAddress &hostAddr, quint16 &hostPort, QByteArray &cid, QByteArray &bucket);

public slots:
    // Bootstrapping
    void performBootstrap();
    void setBootstrapStatus(int status);
    //void multicastAnnounceReplyArrived(QHostAddress &hostAddr, quint16 &hostPort, QByteArray &packet);
    //void broadcastAnnounceReplyArrived(QHostAddress &hostAddr, quint16 &hostPort, QByteArray &packet);


private slots:
    // Timer events
    void networkScanTimerEvent();
    void keepaliveTimerEvent();

private:
    // Bootstrap status
    int bootstrapStatus;

    // Timers
    QTimer *bootstrapTimer;
    QTimer *networkScanTimer;
    QTimer *keepaliveTimer;

};

#endif // NETWORKBOOTSTRAP_H
