#include "networktopology.h"

NetworkTopology::NetworkTopology(QObject *parent) :
    QObject(parent)
{
    unbootstrapped = true;
}

NetworkTopology::~NetworkTopology()
{
    // TODO: save last known good hosts in ander buckets
}


// Hosts wat nie gebootstrap is nie reply met 'n empty bucket ID agteraan die packet, en kom dus nie hier uit nie a.g.v. die length check.
// Ons neem kennis van al die bucket id's wat ons hoor, en gebruik die een wat die meeste voorkom as ons container se id.
void NetworkTopology::announceReplyArrived(bool isMulticast, QHostAddress &hostAddr, quint16 &hostPort, QByteArray &cid, QByteArray &bucketId)
{
    if (unbootstrapped)
    {
        if (isMulticast)
            emit bootstrapStatus(2);
        else
            emit bootstrapStatus(1);
    }
    if (ownBucketId.contains(bucketId))
    {
        // ek is seker mens kan hierdie meer elegant doen.... iterator trick wil ook nie werk nie.
        int c = ownBucketId.value(bucketId);
        ownBucketId.remove(bucketId);
        ownBucketId.insert(bucketId, c+1);
    }
    else
        ownBucketId.insert(bucketId, 1);
}

void NetworkTopology::bucketContentsArrived(QByteArray bucket)
{
    // TODO
}

QHash<QHostAddress, quint16> NetworkTopology::getForwardingPeers(int peersPerBucket)
{
    QByteArray ownBucket = getOwnBucketId();
    QHash<QHostAddress, quint16> forwardingPeersList;
    QHashIterator<QByteArray, HostIntPair*> bucketIterator(buckets);
    while (bucketIterator.hasNext())
    {
        if (bucketIterator.key() != ownBucket)
        {
            QListIterator<QHostAddress> listIterator(*bucketIterator.value()->first);
            int count = 0;
            while (listIterator.hasNext() && count < peersPerBucket)
            {
                count++;
                QHostAddress addr = listIterator.next();
                forwardingPeersList.insert(addr, getDispatchPort(addr));
            }
        }
    }
    return forwardingPeersList;
}

QByteArray NetworkTopology::getOwnBucket()
{
    QByteArray ownBucket = getOwnBucketId();
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    int count = buckets.value(ownBucket)->first->count();
    if (count > 20)
        count = 20;

    QByteArray bucket;
    bucket.append(ownBucket);
    for (int i = 0; i < count; i++)
    {
        QHostAddress host = buckets.value(ownBucket)->first->at(i);
        quint64 tmp = currentTime - buckets.value(ownBucket)->second->at(i);
        quint16 age;
        if (tmp < 65536)
            age = (quint16)tmp;
        else
            age = 65535;

        bucket.append(toQByteArray(host.toIPv4Address()));
        bucket.append(toQByteArray(getDispatchPort(host)));
        bucket.append(toQByteArray(age));
    }
    return bucket;
}

quint16 NetworkTopology::getDispatchPort(QHostAddress &h)
{
    return dispatchPorts.value(h);
}

void NetworkTopology::setDispatchPort(QHostAddress &h, quint16 &p)
{
    dispatchPorts.insert(h, p);
}

void NetworkTopology::updateHostTimestamp(QByteArray &bucket, QHostAddress &host)
{
    qint64 time = QDateTime::currentMSecsSinceEpoch();
    if (buckets.contains(bucket))
    {
        int pos = buckets.value(bucket)->first->indexOf(host);
        if (pos > -1)
        {
            buckets.value(bucket)->first->removeAt(pos);
            buckets.value(bucket)->second->removeAt(pos);
        }
        buckets.value(bucket)->first->prepend(host);
        buckets.value(bucket)->second->prepend(time);
    }
    else
    {
        QHostAddressList *a = new QHostAddressList;
        qint64list *b = new qint64list;
        a->prepend(host);
        b->prepend(time);
        HostIntPair *p = new HostIntPair;
        p->first = a;
        p->second = b;
        buckets.insert(bucket, p);
    }
}

QByteArray NetworkTopology::getOwnBucketId()
{
    QByteArray bucketId;
    int maxcount = 0;
    QHashIterator<QByteArray, int> it(ownBucketId);
    while (it.hasNext())
    {
        if (it.value() > maxcount)
        {
            bucketId = it.key();
            maxcount = it.value();
        }
    }
    return bucketId;
}
