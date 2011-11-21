#include "transfermanager.h"

TransferManager::TransferManager(QObject *parent) :
    QObject(parent)
{
}

TransferManager::~TransferManager()
{
    QHashIterator<QByteArray, Transfer*> it(transferObjectTable);
    while (it.hasNext())
    {
        Transfer *p = it.next().value();
        destroyTransferObject(p);
    }
}

// remove the pointer to the transfer object from the transfer object table before deleting the object.
void TransferManager::destroyTransferObject(Transfer* transferObject)
{
    transferObjectTable.remove(*transferObject->getTTH(), transferObject);
    delete transferObject;
}

// incoming data packets
void TransferManager::incomingDataPacket(quint8 transferProtocolVersion, QByteArray &datagram)
{
    if (transferProtocolVersion == ProtocolAPacket)
    {
        quint64 offset = datagram.mid(2, 8).toULongLong();
        QByteArray tth = datagram.mid(10, 24);
        QByteArray data = datagram.mid(34);
        if ((data.length() == datagram.length() - 34) && (transferObjectTable.contains(tth)))
        {
            QListIterator<Transfer*> it(transferObjectTable.values(tth));
            while (it.hasNext())
            {
                Transfer *t = it.next();
                // we can potentially be uploading segments of the file to multiple peers, but there can be only one download instance
                if (t->getTransferType() == TRANSFER_TYPE_DOWNLOAD)
                    t->incomingDataPacket(transferProtocolVersion, offset, data);
            }
        }
    }
}

// incoming requests for files we share
void TransferManager::incomingUploadRequest(quint8 protocolInstruction, QHostAddress &fromHost, QByteArray &tth, quint64 &offset, quint64 &length)
{
    Transfer *t = getTransferObjectPointer(tth, TRANSFER_TYPE_UPLOAD, fromHost);
    if (t)
    {
        t->setFileOffset(offset);
        t->setSegmentLength(length);
        t->startTransfer();
    }
    else
    {
        UploadTransferQueueItem *i;
        i->protocol = protocolInstruction;
        i->requestingHost = fromHost;
        i->fileOffset = offset;
        i->requestLength = length;
        uploadTransferQueue.insertMulti(tth, i);
        emit filePathNameRequest(tth);
    }
}

// incoming requests from user interface for files we want to download
void TransferManager::incomingDownloadRequest(quint8 protocolVersion, QString &filePathName, QByteArray &tth)
{
    Transfer *t = new DownloadTransfer();
    connect(t, SIGNAL(abort(Transfer*)), this, SLOT(destroyTransferObject(Transfer*)));
    connect(t, SIGNAL(hashBucketRequest(QByteArray&,int&,QByteArray*)), this, SLOT(proxyHashBucketRequest(QByteArray&,int&,QByteArray*)));
    t->setFileName(filePathName);
    t->setTTH(tth);
    t->setTransferProtocol(protocolVersion);
    transferObjectTable.insertMulti(tth, t);
    t->startTransfer();
    emit loadTTHSourcesFromDatabase(tth);
    emit searchTTHAlternateSources(tth);
}

// look for pointer to Transfer object matching tth, transfer type and host address
Transfer* TransferManager::getTransferObjectPointer(QByteArray &tth, int transferType, QHostAddress hostAddr)
{
    if (transferObjectTable.contains(tth))
    {
        QListIterator<Transfer*> it(transferObjectTable.values(tth));
        while (it.hasNext());
        {
            Transfer* p = it.next();
            if ((p->getTransferType() == transferType) && (*p->getRemoteHost() == hostAddr))
                return p;
        }
    }
    return 0;
}

// sharing engine replies with file name for tth being requested for download
// this structure assumes only one user requests a specific file during the time it takes to dispatch the request to a transfer object.
// should more than one user try simultaneously, their requests will be deleted off the queue in .remove(tth) and they should try again. oops.
void TransferManager::filePathNameReply(QByteArray &tth, QString &filename)
{
    if (filename == "")
    {
        uploadTransferQueue.remove(tth);
        return; // TODO: stuur error terug
    }
    Transfer *t = new UploadTransfer();
    connect(t, SIGNAL(abort(Transfer*)), this, SLOT(destroyTransferObject(Transfer*)));
    t->setFileName(filename);
    t->setTTH(tth);
    t->setFileOffset(uploadTransferQueue.value(tth)->fileOffset);
    t->setSegmentLength(uploadTransferQueue.value(tth)->requestLength);
    t->setRemoteHost(uploadTransferQueue.value(tth)->requestingHost);
    t->setTransferProtocol(uploadTransferQueue.value(tth)->protocol);
    uploadTransferQueue.remove(tth);
    transferObjectTable.insertMulti(tth, t);
    t->startTransfer();
}

// return a list of structs that contain the status of all the current transfers
QList<TransferItemStatus> TransferManager::getGlobalTransferStatus()
{
    QList<TransferItemStatus> status;
    QHashIterator<QByteArray, Transfer*> it(transferObjectTable);
    while (it.hasNext())
    {
        TransferItemStatus tis;
        tis.TTH = it.peekNext().value()->getTTH();
        tis.filePathName = it.peekNext().value()->getFileName();
        tis.transferType = it.peekNext().value()->getTransferType();
        tis.transferStatus = it.peekNext().value()->getTransferStatus();
        tis.transferProgress = it.peekNext().value()->getTransferProgress();
        tis.transferRate = it.next().value()->getTransferRate();
        status.append(tis);
    }
    return status;
}

// signals from db restore and incoming tth search both route here
void TransferManager::incomingTTHSource(QByteArray &tth, QHostAddress &sourcePeer)
{
    if (transferObjectTable.contains(tth))
        transferObjectTable.value(tth)->addPeer(sourcePeer);
}

void TransferManager::proxyHashBucketRequest(QByteArray &rootTTH, int &bucketNumber, QByteArray *bucket)
{
    emit hashBucketRequest(rootTTH, bucketNumber, bucket);
}

void TransferManager::hashBucketReply(QByteArray &rootTTH, int &bucketNumber, QByteArray &bucketTTH)
{
    Transfer *t = getTransferObjectPointer(rootTTH, TRANSFER_TYPE_DOWNLOAD);
    if (t)
        t->hashBucketReply(rootTTH, bucketNumber, bucketTTH);
    // should be no else, if the download object mysteriously disappeared somewhere, we can just silently drop the message here.
}
