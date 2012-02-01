#include "networktopology.h"

NetworkTopology::NetworkTopology(QObject *parent) :
    QObject(parent)
{
    unbootstrapped = true;
    not_multicast = true;
    incomingAnnouncementCount = 0;
    bootstrapTimeoutTimer = new QTimer(this);
    bootstrapTimeoutTimer->setSingleShot(true);
    connect(bootstrapTimeoutTimer, SIGNAL(timeout()), this, SLOT(bootstrapTimeoutEvent()));
    bootstrapTimeoutTimer->start(32000);

    garbageCollectTimer = new QTimer(this);
    garbageCollectTimer->setInterval(600000); // 10 min
    garbageCollectTimer->setSingleShot(false);
    connect(garbageCollectTimer, SIGNAL(timeout()), this, SLOT(collectBucketGarbage()));
    garbageCollectTimer->start();

    savePeersTimer = new QTimer(this);
    savePeersTimer->setInterval(300000); // 5 min
    garbageCollectTimer->setSingleShot(false);
    connect(savePeersTimer, SIGNAL(timeout()), this, SLOT(saveActivePeers()));
    savePeersTimer->start();
}

NetworkTopology::~NetworkTopology()
{
    bootstrapTimeoutTimer->deleteLater();
    garbageCollectTimer->deleteLater();
    savePeersTimer->deleteLater();
    //Emitting signal in this destructor will likely not be transmitted since every object is going down along with it
    //::yes that is true, this was put here when we wrote them to nodes.dat, which worked well. this signal is probably useless.
    if (QDateTime::currentMSecsSinceEpoch() - startupTime > 120000) //Don't save hosts unless the program has been online for 2min
    {
        QList<QHostAddress> activeNodes = getForwardingPeers(100);
        emit saveLastKnownPeers(activeNodes);
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

    //if (incomingAnnouncementCount < 20)
        emit requestAllBuckets(hostAddr);
    //incomingAnnouncementCount++;

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

void NetworkTopology::bucketContentsArrived(QByteArray bucket, QHostAddress senderHost)
{
    // TODO: vir nou doen ons nog die gullible ding, hier is potensiaal vir serious statistieke
    if (bucket.length() < 24)
        return;

    QByteArray bucketID = bucket.mid(0, 24);
    bucket.remove(0, 24);
    int iter = 0;
    while (bucket.length() >= 6)
    {
        iter++;
        QHostAddress addr = QHostAddress(getQuint32FromByteArray(&bucket));
        qint64 age = (qint64)(getQuint16FromByteArray(&bucket) * 1000);
        qint64 storedAge = getHostAge(bucketID, addr);
        if ((storedAge > age) || (storedAge == -1))
            updateHostTimestamp(bucketID, addr, age);
    }
    // the replyee is not in his own bucket, since buckets only contain dispatch ip's as seen from the network.
    // we take the address the message came from as his dispatch ip and update the buckets accordingly.
    // update: only do this for empty buckets, i.e. hosts that are alone on a segment, otherwise other buckets are
    // cross-contaminated by interpreting the gossiper as part of it.
    if (iter == 0)
        updateHostTimestamp(bucketID, senderHost);
}

void NetworkTopology::initiateBucketRequests()
{
    QList<QHostAddress> selectedPeers = getForwardingPeers(2);
    QListIterator<QHostAddress> it(selectedPeers);
    while (it.hasNext())
    {
        emit requestBucketContents(it.peekNext());
        emit sendForwardAnnouncement(it.next());
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
    QByteArray bucket = getBucket(ownBucket);
    // I believe this below is wrong, just sending a bucket ID alone is the preferred way when our own bucket is empty.
    //if (bucket.length() == 24)
    //{
    //    bucket.append(toQByteArray(dispatchIP.toIPv4Address()));
    //    bucket.append(toQByteArray((quint16)0));
    //}
    return bucket;
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
    if (host.isNull() || host == QHostAddress(QHostAddress::LocalHost) || host == QHostAddress(QHostAddress::Null) || host == QHostAddress(QHostAddress::Any))
        return;

    if (host.isInSubnet(QHostAddress("169.254.0.0"), 16))
        return;

    qint64 time = QDateTime::currentMSecsSinceEpoch() - age;

    if (buckets.contains(bucket))
    {
        int pos = buckets.value(bucket)->first->indexOf(host);
        if (pos > -1)
        {
            buckets.value(bucket)->first->removeAt(pos);
            buckets.value(bucket)->second->removeAt(pos);
        }
        // this is wrong, the entries must be inserted in the correct order
        //buckets.value(bucket)->first->prepend(host);
        //buckets.value(bucket)->second->prepend(time);

        // optimization: dig the const stuff out of the 24-byte deep reference beforehand
        qint64list *timestampList = buckets.value(bucket)->second;
        int length = timestampList->size();
        int insertPos = length;
        for (int i = 0; i < length; i++)
        {
            if (time >= timestampList->at(i))
            {
                insertPos = i;
                break;
            }
        }

        //qDebug() << timestampList->at(insertPos) << " <= " << time << " inserting at " << insertPos;
        buckets.value(bucket)->first->insert(insertPos, host);
        timestampList->insert(insertPos, time);
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
    qint64 age = -1;
    if (buckets.contains(bucket))
    {
        int pos = buckets.value(bucket)->first->indexOf(host);
        if (pos > -1)
            age = QDateTime::currentMSecsSinceEpoch() - buckets.value(bucket)->second->at(pos);
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
            bucketID.append(toQByteArray((quint64)qrand()*(quint64)qrand()));
        ownBucketId.insert(bucketID, 1);
        emit changeBootstrapStatus(NETWORK_BCAST_ALONE);
    }
}

void NetworkTopology::setCID(QByteArray &cid)
{
    CID = cid;
}

void NetworkTopology::setDispatchIP(QHostAddress ip)
{
    dispatchIP = ip;
}

void NetworkTopology::setBootstrapStatus(int status)
{
    bootstrapStatus = status;
}

void NetworkTopology::collectBucketGarbage()
{
    QMutableHashIterator<QByteArray, HostIntPair*> ib(buckets);

    // Iterate over buckets: shake hosts from small buckets if they also occur in large buckets
    while (ib.hasNext())
    {
        QListIterator<QHostAddress> ilh(*ib.peekNext().value()->first);
        QListIterator<qint64> ili(*ib.peekNext().value()->second);
        QByteArray bucket = ib.next().key();
        while (ilh.hasNext())
        {
            if (ili.hasNext())
                ili.next();
            QHostAddress testForHost = ilh.next();
            int currentBucketSize = buckets.value(bucket)->first->count();
            QHashIterator<QByteArray, HostIntPair*> ib2(buckets);
            while (ib2.hasNext())
            {
                QByteArray testBucket = ib2.next().key();
                int testBucketSize = buckets.value(testBucket)->first->count();
                if ((currentBucketSize < testBucketSize) && (buckets.value(testBucket)->first->contains(testForHost)))
                {
                    int i = buckets.value(bucket)->first->indexOf(testForHost);
                    if (i != -1)
                    {
                        buckets.value(bucket)->first->removeAt(i);
                        buckets.value(bucket)->second->removeAt(i);
                        qDebug() << "NetworkTopology::collectBucketGarbage(): pluck duplicate entry in small bucket: " << testForHost.toString();
                    }
                }
            }
        }
        if (buckets.value(bucket)->first->isEmpty())
        {
            delete buckets.value(bucket)->first;
            delete buckets.value(bucket)->second;
            delete buckets.value(bucket);
            ib.remove();
            qDebug() << "NetworkTopology::collectBucketGarbage(): delete empty bucket pass 1: " << bucket.toBase64();
        }
    }

    // Iterate over buckets: prune stale entries and refresh entries about to become stale
    qint64 cutoffTime = QDateTime::currentMSecsSinceEpoch() - 1800000;  // 30 minutes ago
    qint64 bucketRequestTime = QDateTime::currentMSecsSinceEpoch() - 900000;  // 15 minutes ago
    ib.toFront();
    while (ib.hasNext())
    {
        QListIterator<qint64> il(*ib.peekNext().value()->second);
        QByteArray bucket = ib.next().key();
        if (il.hasNext())
        {
            il.toBack();
            // Purge old entries
            while ((il.hasPrevious()) && (il.previous() < cutoffTime))
            {
                QHostAddress h = QHostAddress(buckets.value(bucket)->first->back());
                emit requestBucketContents(h); // back returns reference, which might be gone by the time the queued connection is dispatched.
                buckets.value(bucket)->first->removeLast();
                buckets.value(bucket)->second->removeLast();
            }
            // Request buckets from semi-old entries
            while ((il.hasPrevious()) && (il.peekPrevious() < bucketRequestTime))
            {
                int p = buckets.value(bucket)->second->indexOf(il.previous());
                emit requestBucketContents(buckets.value(bucket)->first->at(p));
            }
        }
        // Delete empty buckets
        if (buckets.value(bucket)->first->isEmpty())
        {
            delete buckets.value(bucket)->first;
            delete buckets.value(bucket)->second;
            delete buckets.value(bucket);
            ib.remove();
            qDebug() << "NetworkTopology::collectBucketGarbage(): delete empty bucket pass 2: " << bucket.toBase64();
        }
    }
    if (buckets.isEmpty())
        emit changeBootstrapStatus(NETWORK_BCAST_ALONE);
}

void NetworkTopology::saveActivePeers()
{
    //Emit a signal, saving the last active nodes to the database
    QList<QHostAddress> activeNodes = getForwardingPeers(100);
    emit saveLastKnownPeers(activeNodes);
}

// DEBUGGING
QString NetworkTopology::getDebugBucketsContents()
{
    QString rstring;
    QHashIterator<QByteArray, HostIntPair*> bucketIterator(buckets);
    while (bucketIterator.hasNext())
    {
        bucketIterator.next();
        rstring.append("\n<font color=\"red\">");
        rstring.append(bucketIterator.key().toBase64());
        rstring.append("</font>\n");
        QListIterator<QHostAddress> ipIterator(*bucketIterator.value()->first);
        QListIterator<qint64> lastseenIterator(*bucketIterator.value()->second);
        while (ipIterator.hasNext() && lastseenIterator.hasNext())
        {
            rstring.append("<font color=\"green\">");
            rstring.append(QDateTime::fromMSecsSinceEpoch(lastseenIterator.next()).toString("hh:mm:ss"));
            rstring.append("</font> <font color=\"blue\">");
            rstring.append(ipIterator.next().toString());
            rstring.append("</font>\n");
        }
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

int NetworkTopology::getNumberOfCIDHosts()
{
    // These are not garbage collected, and I doubt if they are useful at all.
    // Perhaps we should look into removing the CID host thing later if nothing uses it.
    return CIDHosts.size();
}

int NetworkTopology::getNumberOfHosts()
{
    int count = 0;
    QList<QString> resultList;
    QHashIterator<QByteArray, HostIntPair*> i(buckets);
    while (i.hasNext())
    {
        foreach (QHostAddress addr, *i.next().value()->first)
        {
            QString ip = addr.toString();
            if (!resultList.contains(ip))
                resultList.append(ip);
        }
        //count += i.next().value()->first->count();
    }
    //return count;
    return resultList.count();
}
