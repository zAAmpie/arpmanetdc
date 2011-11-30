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
    delete transferObject;
}

// incoming data packets
void TransferManager::incomingDataPacket(quint8 transferPacket, QByteArray &datagram)
{
    if (transferPacket == ProtocolADataPacket)
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
    else if (transferPacket == ProtocolAControlPacket)
    {

    }
}

// incoming requests for files we share
void TransferManager::incomingUploadRequest(QByteArray transferProtocolHint, QHostAddress fromHost, QByteArray tth, quint64 offset, quint64 length)
{
    qDebug() << "Data request offset " << offset << " length " << length;
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
        i->transferProtocolHint = transferProtocolHint;
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
    connect(t, SIGNAL(abort(Transfer*)), this, SLOT(destroyTransferObject(Transfer*)));
    connect(t, SIGNAL(transmitDatagram(QHostAddress&,QByteArray&)), this, SIGNAL(transmitDatagram(QHostAddress&,QByteArray&)));
    t->setFileName(filename);
    t->setTTH(tth);
    t->setFileOffset(uploadTransferQueue.value(tth)->fileOffset);
    t->setSegmentLength(uploadTransferQueue.value(tth)->requestLength);
    t->setRemoteHost(uploadTransferQueue.value(tth)->requestingHost);
    t->setTransferProtocolHint(uploadTransferQueue.value(tth)->transferProtocolHint);
    uploadTransferQueue.remove(tth);
    transferObjectTable.insertMulti(tth, t);
    t->startTransfer();
}

// incoming requests from user interface for files we want to download
// the higher the priority, the lower the number.
void TransferManager::queueDownload(int priority, QByteArray &tth, QString &filePathName, quint64 fileSize, QHostAddress fileHost)
{
    if (!downloadTransferQueue.contains(priority))
    {
        QList<DownloadTransferQueueItem> *list = new QList<DownloadTransferQueueItem>;
        downloadTransferQueue.insert(priority, list);
    }
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
    connect(t, SIGNAL(searchTTHAlternateSources(QByteArray&)), this, SIGNAL(searchTTHAlternateSources(QByteArray&)));
    connect(t, SIGNAL(loadTTHSourcesFromDatabase(QByteArray)), this, SIGNAL(loadTTHSourcesFromDatabase(QByteArray)));
    connect(t, SIGNAL(sendDownloadRequest(QByteArray&,QHostAddress&,QByteArray&,quint64&,quint64&)),
            this, SIGNAL(sendDownloadRequest(QByteArray&,QHostAddress&,QByteArray&,quint64&,quint64&)));
    t->setFileName(i.filePathName);
    t->setTTH(i.tth);
    t->setFileSize(i.fileSize);
    t->addPeer(i.fileHost);
    //t->setTransferProtocol(protocolVersion);
    transferObjectTable.insertMulti(i.tth, t);
    emit loadTTHSourcesFromDatabase(i.tth);
    emit searchTTHAlternateSources(i.tth);
    t->startTransfer();

    emit downloadStarted(i.tth);
}

void TransferManager::changeQueuedDownloadPriority(int oldPriority, int newPriority, QByteArray &tth)
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
void TransferManager::removeQueuedDownload(int priority, QByteArray &tth)
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
        tis.transferRate = it.next().value()->getTransferRate();
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

void TransferManager::setMaximumSimultaneousDownloads(int n)
{
    maximumSimultaneousDownloads = n;
}
