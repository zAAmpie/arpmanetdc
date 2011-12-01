#include "networktopology.h"

NetworkTopology::NetworkTopology(QObject *parent) :
    QObject(parent)
{
    unbootstrapped = true;
    not_multicast = true;
    incomingAnnouncementCount = 0;
    bootstrapTimeoutTimer = new QTimer();
    bootstrapTimeoutTimer->setSingleShot(true);
    connect(bootstrapTimeoutTimer, SIGNAL(timeout()), this, SLOT(bootstrapTimeoutEvent()));
    bootstrapTimeoutTimer->start(32000);
}

NetworkTopology::~NetworkTopology()
{
    delete bootstrapTimeoutTimer;
    if (QDateTime::currentMSecsSinceEpoch() - startupTime > 120000)
    {
        QList<QHostAddress> activeNodes = getForwardingPeers(20);
        QListIterator<QHostAddress> it(activeNodes);
        QFile file("nodes.dat");
        if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        {
            while (it.hasNext())
            {
                QByteArray saveData;
                saveData.append(it.next().toString());
                saveData.append("\n");
                file.write(saveData);
            }
        }
        file.close();
    }
}

// Hosts wat nie gebootstrap is nie reply met 'n empty bucket ID agteraan die packet, en kom dus nie hier uit nie a.g.v. die length check.
// Dispatcher reply op die announce, ons hoef nie hier daaroor te worry nie.
// Ons neem kennis van al die bucket id's wat ons hoor, en gebruik die een wat die meeste voorkom as ons container se id.
void NetworkTopology::announceReplyArrived(bool isMulticast, QHostAddress &hostAddr, QByteArray &cid, QByteArray &bucketId)
{
    // ignoreer onsself
    if (cid == CID)
        return;

    // bootstrap status
    if (unbootstrapped)
    {
        unbootstrapped = false;
        if (isMulticast)
        {
            emit changeBootstrapStatus(NETWORK_MCAST);
            not_multicast = false;
        }
        else
            emit changeBootstrapStatus(NETWORK_BCAST);
    }
    else if (not_multicast && isMulticast)
    {
        emit changeBootstrapStatus(NETWORK_MCAST);
        not_multicast = false;
    }

    if (incomingAnnouncementCount < 20)
        emit requestAllBuckets(hostAddr);
    incomingAnnouncementCount++;

    // determine own bucket id
    if (ownBucketId.contains(bucketId))
    {
        // ek is seker mens kan hierdie meer elegant doen.... iterator trick wil ook nie werk nie.
        int c = ownBucketId.value(bucketId);
        ownBucketId.remove(bucketId);
        ownBucketId.insert(bucketId, c+1);
    }
    else
        ownBucketId.insert(bucketId, 1);

    // save cid-hostaddr relationship
    // add to buckets
    // all happens in announceForwardReplyArrived() - distinction made to not confuse own bucket ID during bootstrapping
    announceForwardReplyArrived(hostAddr, cid, bucketId);
}
// also continues from announceReplyArrived()
void NetworkTopology::announceForwardReplyArrived(QHostAddress &hostAddr, QByteArray &cid, QByteArray &bucket)
{
    // save cid-hostaddr relationship
    if (cid.length() > 0)
        setCIDHostAddress(cid, hostAddr);

    // add to buckets
    updateHostTimestamp(bucket, hostAddr);
}

void NetworkTopology::bucketContentsArrived(QByteArray bucket)
{
    // TODO: vir nou doen ons nog die gullible ding, hier is potensiaal vir serious statistieke
    if (bucket.length() < 28)
        return;

    QByteArray bucketID = bucket.mid(0, 24);
    QByteArray bucketContents = bucket.mid(24);
    while (bucketContents.length() >= 6)
    {
        QHostAddress addr = QHostAddress(bucketContents.mid(0,4).toUInt());
        qint64 age = (qint64)(bucketContents.mid(6,2).toUInt() * 1000);
        if (getHostAge(bucketID, addr) > age)
            updateHostTimestamp(bucketID, addr, age);
        bucketContents.remove(0, 6);
    }
}

void NetworkTopology::initiateBucketRequests()
{
    QList<QHostAddress> selectedPeers = getForwardingPeers(1);
    QListIterator<QHostAddress> it(selectedPeers);
    while (it.hasNext())
    {
        emit requestBucketContents(it.next());
    }
}

QList<QHostAddress> NetworkTopology::getForwardingPeers(int peersPerBucket)
{
    QByteArray ownBucket = getOwnBucketId();
    QList<QHostAddress> forwardingPeers;
    QHashIterator<QByteArray, HostIntPair*> bucketIterator(buckets);
    while (bucketIterator.hasNext())
    {
        bucketIterator.next();
        if (bucketIterator.key() != ownBucket)
        {
            QListIterator<QHostAddress> listIterator(*bucketIterator.value()->first);
            int count = 0;
            while (listIterator.hasNext() && count < peersPerBucket)
            {
                count++;
                forwardingPeers.append(listIterator.next());
            }
        }
    }
    return forwardingPeers;
}

QByteArray NetworkTopology::getOwnBucket()
{
    QByteArray ownBucket = getOwnBucketId();
    return getBucket(ownBucket);
}

QList<QByteArray> NetworkTopology::getAllBuckets()
{
    QList<QByteArray> bucketPackets;
    QHashIterator<QByteArray, HostIntPair*> it(buckets);
    while (it.hasNext())
        bucketPackets.append(getBucket(it.next().key()));
    return bucketPackets;
}

// helper for getOwnBucket() and getAllBuckets()
QByteArray NetworkTopology::getBucket(QByteArray bucketid)
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    QByteArray bucket;
    // send at least the bucket ID for empty buckets (typically our own) in case somebody scans us.
    // this is to aid detection of clients that are sitting alone in a container.
    bucket.append(bucketid);
    if (!buckets.contains(bucketid))
        return bucket;

    int count = buckets.value(bucketid)->first->count();
    if (count > 20)
        count = 20;

    for (int i = 0; i < count; i++)
    {
        QHostAddress host = buckets.value(bucketid)->first->at(i);
        quint64 tmp = (currentTime - buckets.value(bucketid)->second->at(i)) / 1000;
        quint16 age;
        if (tmp < 65536)
            age = (quint16)tmp;
        else
            age = 65535;

        bucket.append(toQByteArray(host.toIPv4Address()));
        bucket.append(toQByteArray(age));
    }
    return bucket;
}

QHostAddress NetworkTopology::getCIDHostAddress(QByteArray &cid)
{
    return CIDHosts.value(cid);
}

void NetworkTopology::setCIDHostAddress(QByteArray &cid, QHostAddress &host)
{
    CIDHosts.insert(cid, host);
}

void NetworkTopology::updateHostTimestamp(QByteArray &bucket, QHostAddress &host, qint64 age)
{
    qint64 time = QDateTime::currentMSecsSinceEpoch() - age;

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

qint64 NetworkTopology::getHostAge(QByteArray &bucket, QHostAddress &host)
{
    qint64 time = QDateTime::currentMSecsSinceEpoch();
    qint64 age = -1;
    if (buckets.contains(bucket))
    {
        int pos = buckets.value(bucket)->first->indexOf(host);
        if (pos > -1)
            age = time - buckets.value(bucket)->second->at(pos);
    }
    return age;
}

QByteArray NetworkTopology::getOwnBucketId()
{
    QByteArray bucketId;
    int maxcount = 0;
    QHashIterator<QByteArray, int> it(ownBucketId);
    while (it.hasNext())
    {
        if (it.next().value() > maxcount)
        {
            // QByteArray is COW, hierdie assignment behels net 'n pointer, so dis OK.
            bucketId = it.key();
            maxcount = it.value();
        }
    }
    return bucketId;
}

void NetworkTopology::bootstrapTimeoutEvent()
{
    if (bootstrapStatus <= 0)
    {
        quint64 time = QDateTime::currentMSecsSinceEpoch();
        qsrand(time);
        QByteArray bucketID;
        for (int i = 0; i < 3; i++)
            bucketID.append(toQByteArray((quint64)qrand()*qrand()));
        ownBucketId.insert(bucketID, 2);
        emit changeBootstrapStatus(NETWORK_BCAST_ALONE);
    }
}

void NetworkTopology::setCID(QByteArray &cid)
{
    CID = cid;
}

void NetworkTopology::setBootstrapStatus(int status)
{
    bootstrapStatus = status;
}

// DEBUGGING
QString NetworkTopology::getDebugBucketsContents()
{
    QString rstring;
    QHashIterator<QByteArray, HostIntPair*> bucketIterator(buckets);
    while (bucketIterator.hasNext())
    {
        bucketIterator.next();
        rstring.append(bucketIterator.key().toBase64());
        rstring.append("\n");
        QListIterator<QHostAddress> ipIterator(*bucketIterator.value()->first);
        QListIterator<qint64> lastseenIterator(*bucketIterator.value()->second);
        while (ipIterator.hasNext() && lastseenIterator.hasNext())
        {
            rstring.append(ipIterator.next().toString());
            rstring.append(" ");
            rstring.append(QString::number(lastseenIterator.next()));
            rstring.append("\n");
        }
        rstring.append("\n");
    }
    return rstring;
}

QString NetworkTopology::getDebugCIDHostContents()
{
    QString rstring;
    QHashIterator<QByteArray, QHostAddress> it(CIDHosts);
    while (it.hasNext())
    {
        it.next();
        rstring.append(it.key().toBase64());
        rstring.append(" ");
        rstring.append(it.value().toString());
        rstring.append("\n");
    }
    return rstring;
}
