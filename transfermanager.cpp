#include "transfermanager.h"

TransferManager::TransferManager(QObject *parent) :
    QObject(parent)
{
    currentDownloadCount = 0;
}

TransferManager::~TransferManager()
{
    QHashIterator<QByteArray, Transfer*> it(transferObjectTable);
    while (it.hasNext())
    {
        Transfer *p = it.next().value();
        destroyTransferObject(p);
    }
    QMapIterator<int, QList<DownloadTransferQueueItem>*> i(downloadTransferQueue);
    while (i.hasNext())
        delete i.next().value();
}

// remove the pointer to the transfer object from the transfer object table before deleting the object.
void TransferManager::destroyTransferObject(Transfer* transferObject)
{
    transferObjectTable.remove(*transferObject->getTTH(), transferObject);
    transferObject->deleteLater();
}

// incoming data packets
void TransferManager::incomingDataPacket(quint8 transferPacket, QByteArray datagram)
{
    QByteArray tmp = datagram.mid(2, 8);
    quint64 offset = getQuint64FromByteArray(&tmp);
    QByteArray tth = datagram.mid(10, 24);
    QByteArray data = datagram.mid(34);
    if ((data.length() == datagram.length() - 34) && (transferObjectTable.contains(tth)))
    {
        Transfer *t = getTransferObjectPointer(tth, TRANSFER_TYPE_DOWNLOAD);
        t->incomingDataPacket(transferPacket, offset, data);
    }
}

// incoming requests for files we share
void TransferManager::incomingUploadRequest(quint8 protocol, QHostAddress fromHost, QByteArray tth, quint64 offset, quint64 length)
{
    qDebug() << "TransferManager::incomingUploadRequest(): Data request offset " << offset << " length " << length;
    Transfer *t = getTransferObjectPointer(tth, TRANSFER_TYPE_UPLOAD, fromHost);
    if (t)
    {
        t->setFileOffset(offset);
        t->setSegmentLength(length);
        t->startTransfer();
    }
    else
    {
        UploadTransferQueueItem *i = new UploadTransferQueueItem;
        i->protocol = protocol;
        i->requestingHost = fromHost;
        i->fileOffset = offset;
        i->requestLength = length;
        uploadTransferQueue.insertMulti(tth, i);
        emit filePathNameRequest(tth);
    }
}

// sharing engine replies with file name for tth being requested for download
// this structure assumes only one user requests a specific file during the time it takes to dispatch the request to a transfer object.
// should more than one user try simultaneously, their requests will be deleted off the queue in .remove(tth) and they should try again. oops.
void TransferManager::filePathNameReply(QByteArray tth, QString filename)
{
    if (filename == "" || !uploadTransferQueue.contains(tth))
    {
        uploadTransferQueue.remove(tth);
        return; // TODO: stuur error terug na requesting host
    }
    Transfer *t = new UploadTransfer();
    t->createUploadObject(uploadTransferQueue.value(tth)->protocol);
    connect(t, SIGNAL(abort(Transfer*)), this, SLOT(destroyTransferObject(Transfer*)));
    connect(t, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)), this, SIGNAL(transmitDatagram(QHostAddress,QByteArray*)));
    t->setFileName(filename);
    t->setTTH(tth);
    t->setFileOffset(uploadTransferQueue.value(tth)->fileOffset);
    t->setSegmentLength(uploadTransferQueue.value(tth)->requestLength);
    t->setRemoteHost(uploadTransferQueue.value(tth)->requestingHost);
    uploadTransferQueue.remove(tth);
    transferObjectTable.insertMulti(tth, t);
    t->startTransfer();
}

// incoming requests from user interface for files we want to download
// the higher the priority, the lower the number.
void TransferManager::queueDownload(int priority, QByteArray tth, QString filePathName, quint64 fileSize, QHostAddress fileHost)
{
    if (!downloadTransferQueue.contains(priority))
    {
        QList<DownloadTransferQueueItem> *list = new QList<DownloadTransferQueueItem>;
        downloadTransferQueue.insert(priority, list);
    }

    //Test if item isn't already in the queue
    bool found = false;
    int size = downloadTransferQueue.value(priority)->size();
    foreach (DownloadTransferQueueItem item, *downloadTransferQueue.value(priority))
    {
        if (item.tth == tth)
        {
            found = true;
            break;
        }
    }

    //Only queue the item if it's not already in the queue and not busy downloading
    if (!found && !getTransferObjectPointer(tth, TRANSFER_TYPE_DOWNLOAD))
    {
        DownloadTransferQueueItem i;
        i.filePathName = filePathName;
        i.tth = tth;
        i.fileSize = fileSize;
        i.fileHost = fileHost;
        downloadTransferQueue.value(priority)->append(i);
        // start the next highest priority queued download, unless max downloads active.
        if (currentDownloadCount < maximumSimultaneousDownloads)
            startNextDownload();
    }
}

// get next item to be downloaded off the queue
DownloadTransferQueueItem TransferManager::getNextQueuedDownload()
{
    QMapIterator<int, QList<DownloadTransferQueueItem>* > i(downloadTransferQueue);
    while (i.hasNext())
    {
        QList<DownloadTransferQueueItem>* l = i.next().value();
        if (!l->isEmpty())
        {
            DownloadTransferQueueItem nextItem;
            nextItem = l->first();
            l->removeFirst();
            return nextItem;
        }
    }
    DownloadTransferQueueItem noItem;
    noItem.filePathName = "";
    return noItem;
}

void TransferManager::startNextDownload()
{
    DownloadTransferQueueItem i = getNextQueuedDownload();
    if (i.filePathName == "")
        return;

    currentDownloadCount++;
    Transfer *t = new DownloadTransfer();
    connect(t, SIGNAL(abort(Transfer*)), this, SLOT(destroyTransferObject(Transfer*)));
    connect(t, SIGNAL(hashBucketRequest(QByteArray,int,QByteArray*)), this, SIGNAL(hashBucketRequest(QByteArray,int,QByteArray*)));
    connect(t, SIGNAL(TTHTreeRequest(QHostAddress,QByteArray)), this, SIGNAL(TTHTreeRequest(QHostAddress,QByteArray)));
    connect(t, SIGNAL(searchTTHAlternateSources(QByteArray)), this, SIGNAL(searchTTHAlternateSources(QByteArray)));
    connect(t, SIGNAL(loadTTHSourcesFromDatabase(QByteArray)), this, SIGNAL(loadTTHSourcesFromDatabase(QByteArray)));
    connect(t, SIGNAL(sendDownloadRequest(quint8,QHostAddress,QByteArray,quint64,quint64)),
            this, SIGNAL(sendDownloadRequest(quint8,QHostAddress,QByteArray,quint64,quint64)));
    connect(t, SIGNAL(flushBucket(QString,QByteArray*)), this, SIGNAL(flushBucket(QString,QByteArray*)));
    connect(t, SIGNAL(assembleOutputFile(QString,QString,int,int)), this, SIGNAL(assembleOutputFile(QString,QString,int,int)));
    t->setFileName(i.filePathName);
    t->setTTH(i.tth);
    t->setFileSize(i.fileSize);
    t->addPeer(i.fileHost);
    transferObjectTable.insertMulti(i.tth, t);
    emit loadTTHSourcesFromDatabase(i.tth);
    emit searchTTHAlternateSources(i.tth);
    t->startTransfer();

    emit downloadStarted(i.tth);
}

void TransferManager::changeQueuedDownloadPriority(int oldPriority, int newPriority, QByteArray tth)
{
    DownloadTransferQueueItem item;
    if (downloadTransferQueue.contains(oldPriority))
    {
        QListIterator<DownloadTransferQueueItem> i(*downloadTransferQueue.value(oldPriority));
        while (i.hasNext())
        {
            if (i.peekNext().tth == tth)
            {
                item = i.next();
                break;
            }
            i.next();
        }
    }
    if (item.filePathName != "")
    {
        if (!downloadTransferQueue.contains(newPriority))
        {
            QList<DownloadTransferQueueItem> *list = new QList<DownloadTransferQueueItem>;
            downloadTransferQueue.insert(newPriority, list);
        }
        if (oldPriority > newPriority)
            downloadTransferQueue.value(newPriority)->append(item);
        else
            downloadTransferQueue.value(newPriority)->prepend(item);
    }
}

// downloads that are still queued can just be lifted off the queue, their transfer objects do not exist yet.
void TransferManager::removeQueuedDownload(int priority, QByteArray tth)
{
    if (downloadTransferQueue.contains(priority))
    {
        int pos = -1;
        int foundPos = -1;
        QListIterator<DownloadTransferQueueItem> i(*downloadTransferQueue.value(priority));
        while (i.hasNext())
        {
            pos++;
            if (i.next().tth == tth)
            {
                foundPos = pos;
                break;
            }
        }
        if (foundPos > -1)
        {   // As far as I understand, a list's index will always start at 0 and follow sequentially. Touch wood.
            downloadTransferQueue.value(priority)->removeAt(foundPos);
        }
    }

    //Stop the transfer if it's not in the queue (it might've already started)
    stopTransfer(tth, TRANSFER_TYPE_DOWNLOAD, QHostAddress());
}

//Stop a transfer already running
void TransferManager::stopTransfer(QByteArray tth, int transferType, QHostAddress hostAddr)
{
    Transfer *t;

    if (transferType == TRANSFER_TYPE_DOWNLOAD)
        t = getTransferObjectPointer(tth, transferType);
    else
        t = getTransferObjectPointer(tth, transferType, hostAddr);
    if (t)
    {
        //Abort transfer before deletion
        t->abortTransfer();
        t->blockSignals(true);
        t->deleteLater();
        transferObjectTable.remove(tth, t);

        //Decrease download count
        currentDownloadCount--;

        //Start next transfer
        startNextDownload();
    }
}

// look for pointer to Transfer object matching tth, transfer type and host address
Transfer* TransferManager::getTransferObjectPointer(QByteArray &tth, int transferType, QHostAddress hostAddr)
{
    if (transferObjectTable.contains(tth))
    {
        QListIterator<Transfer*> it(transferObjectTable.values(tth));
        while (it.hasNext())
        {
            Transfer* p = it.next();
            //if ((p->getTransferType() == transferType) && (p->getRemoteHost()->toString() == hostAddr.toString()))
            if ((p->getTransferType() == transferType) && (*p->getRemoteHost() == hostAddr))
                return p;
        }
    }
    return 0;
}

// return a list of structs that contain the status of all the current transfers
QList<TransferItemStatus> TransferManager::getGlobalTransferStatus()
{
    QList<TransferItemStatus> status;
    QHashIterator<QByteArray, Transfer*> it(transferObjectTable);
    while (it.hasNext())
    {
        TransferItemStatus tis;
        tis.TTH = *it.peekNext().value()->getTTH();
        tis.filePathName = *it.peekNext().value()->getFileName();
        tis.transferType = it.peekNext().value()->getTransferType();
        tis.transferStatus = it.peekNext().value()->getTransferStatus();
        tis.transferProgress = it.peekNext().value()->getTransferProgress();
        tis.transferRate = it.peekNext().value()->getTransferRate();
        tis.host = *it.next().value()->getRemoteHost();
        status.append(tis);
    }
    return status;
}

// signals from db restore and incoming tth search both route here
void TransferManager::incomingTTHSource(QByteArray tth, QHostAddress sourcePeer)
{
    if (transferObjectTable.contains(tth))
        transferObjectTable.value(tth)->addPeer(sourcePeer);
}

// packet containing part of a tree
// qint32 bucket number followed by 24-byte QByteArray of bucket 1MBTTH
// therefore, 2.25 petabyte max file size, should suffice.
void TransferManager::incomingTTHTree(QByteArray tth, QByteArray tree)
{
    Transfer *t = getTransferObjectPointer(tth, TRANSFER_TYPE_DOWNLOAD);
    if (t)
        t->TTHTreeReply(tree);
}

void TransferManager::hashBucketReply(QByteArray rootTTH, int bucketNumber, QByteArray bucketTTH)
{
    Transfer *t = getTransferObjectPointer(rootTTH, TRANSFER_TYPE_DOWNLOAD);
    if (t)
        t->hashBucketReply(bucketNumber, bucketTTH);
    // should be no else, if the download object mysteriously disappeared somewhere, we can just silently drop the message here.
}

void TransferManager::incomingProtocolCapabilityResponse(QHostAddress peer, char protocols)
{
    peerProtocolCapabilities.insert(peer, protocols);
    if (peerProtocolDiscoveryWaitingPool.contains(peer) && peerProtocolDiscoveryWaitingPool.value(peer))
    {
        peerProtocolDiscoveryWaitingPool.value(peer)->setPeerProtocolCapability(peer, protocols);
        peerProtocolDiscoveryWaitingPool.remove(peer);
    }
}

void TransferManager::requestPeerProtocolCapability(QHostAddress peer, Transfer *transferObject)
{
    if (peerProtocolCapabilities.contains(peer))
        transferObject->setPeerProtocolCapability(peer, peerProtocolCapabilities.value(peer));
    else
    {
        peerProtocolDiscoveryWaitingPool.insert(peer, transferObject);
        emit requestProtocolCapability(peer);
    }
}

void TransferManager::setMaximumSimultaneousDownloads(int n)
{
    maximumSimultaneousDownloads = n;
}
