#ifndef NETWORKTOPOLOGY_H
#define NETWORKTOPOLOGY_H

#include <QObject>
#include <QHostAddress>
#include <QList>
#include <QHash>
#include <QDateTime>
#include "util.h"

typedef QList<QHostAddress> QHostAddressList;
typedef QList<qint64> qint64list;
typedef QPair<QHostAddressList*, qint64list*> HostIntPair;

class NetworkTopology : public QObject
{
    Q_OBJECT
public:
    explicit NetworkTopology(QObject *parent = 0);
    ~NetworkTopology();
    QHash<QHostAddress, quint16> getForwardingPeers(int peersPerBucket);
    QByteArray getOwnBucketId();
    QByteArray getOwnBucket();

signals:
    void bootstrapStatus(int status);

public slots:
    void announceReplyArrived(bool isMulticast, QHostAddress &hostAddr, quint16 &hostPort, QByteArray &cid, QByteArray &bucket);
    void bucketContentsArrived(QByteArray);

private slots:

private:
    QHash<QHostAddress, quint16> dispatchPorts;
    QHash<QByteArray, QHostAddress> CIDHosts;

    quint16 getDispatchPort(QHostAddress &h);
    void setDispatchPort(QHostAddress &h, quint16 &p);

    // moet hierdie twee lyste se indekse manually in sync hou.
    // stupid manier, maar gee beste sorted value lookup performance.
    //QHash<QByteArray, QPair<QList<QHostAddress>*, QList<qint64>* >* > buckets; // typedef'd below
    QHash<QByteArray, HostIntPair*> buckets;
    // helper funcs om die mess hier bo in orde te hou
    void updateHostTimestamp(QByteArray &bucket, QHostAddress &host);

    QHash<QByteArray, int> ownBucketId;
    bool unbootstrapped;

};

#endif // NETWORKTOPOLOGY_H
