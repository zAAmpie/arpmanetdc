#include "bucketflushthread.h"
#include <QDir>

BucketFlushThread::BucketFlushThread(QObject *parent) :
    QObject(parent)
{
}


// IMPORTANT: this function is deprecated.
// If it finds use again, it should be modified to receive TTH and emit bucketFlushed() and bucketFlushFailed().
void BucketFlushThread::flushBucket(QString filename, QByteArray *bucket)
{
    QFile file(filename);
    if (!bucket)
        return;
    if ((!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) || (file.write(*bucket) == -1))
    {
        // TODO: emit MISTAKE!, pause download
    }
    file.close();
    delete bucket;
    bucket = 0;
}

void BucketFlushThread::assembleOutputFile(QString tmpfilebase, QString outfile, int startbucket, int lastbucket)
{
    int bucket = startbucket;
    QFile inf;
    QString outfilepart = outfile + ".part";

    //Go to directory
    QString pathStr = outfile.left(outfile.lastIndexOf("/"));
    QDir path(pathStr);
    if (!path.exists())
        path.mkpath(pathStr);

    QFile outf(outfilepart);
    if (!outf.open(QIODevice::ReadWrite))
    {
        //TODO: report error
        return;
    }

    bool ok = true;
    while (ok)
    {
        QString infile = tmpfilebase + "." + QString::number(bucket);
        inf.setFileName(infile);
        if (!inf.open(QIODevice::ReadOnly))
        {
            ok = false;
            break;
        }
        else
        {
            //Read all data from the temp file (why is there a temp file if assembleOutputFile is called after every bucket flush?)
            QByteArray buf = inf.readAll();
            
            //Ensure enough space is available for the map
            if (outf.size() < (quint64)bucket * HASH_BUCKET_SIZE + buf.size())
                outf.resize((quint64)bucket * HASH_BUCKET_SIZE + buf.size());
            
#ifndef USE_MMAPFILES_FOR_DOWNLOAD
            outf.seek((quint64)bucket * HASH_BUCKET_SIZE);
            outf.write(buf);
#endif

#ifdef USE_MMAPFILES_FOR_DOWNLOAD
            //Map file to memory
            uchar *f = outf.map((quint64)bucket * HASH_BUCKET_SIZE, buf.size());
            
            //Write to memory if map succeeded
            if (f != 0)
                memmove(f, (const uchar *)buf.constData(), buf.size());
            else
                qDebug() << "BucketFlushThread::assembleOutputFile: Big booboo for map! bucket : size" << bucket << buf.size();
                
            if (!outf.unmap(f))
                qDebug() << "BucketFlushThread::assembleOutputFile: Could not unmap file! bucket : size" << bucket << buf.size();
#endif

            inf.close();
            inf.remove();
        }
        bucket++;
    }
    if (outf.isOpen())
        outf.close();
}

void BucketFlushThread::flushBucketDirect(QString filename, int bucketno, QByteArray *bucket, QByteArray tth)
{
    //Go to directory
    QString pathStr = filename.left(filename.lastIndexOf("/"));
    QDir path(pathStr);
    if (!path.exists())
        path.mkpath(pathStr);

    QString incompleteFilename = filename + ".incomplete";
    QFile outf(incompleteFilename);
    if (!outf.open(QIODevice::ReadWrite))
    {
        delete bucket;
        emit bucketFlushFailed(tth, bucketno);
        return;
    }

    //Ensure enough space is available for the map
    if (outf.size() < (quint64)bucketno * HASH_BUCKET_SIZE + bucket->size())
        outf.resize((quint64)bucketno * HASH_BUCKET_SIZE + bucket->size());

#ifndef USE_MMAPFILES_FOR_DOWNLOAD
    outf.seek((quint64)bucketno * HASH_BUCKET_SIZE);
    outf.write(*bucket);
#endif

#ifdef USE_MMAPFILES_FOR_DOWNLOAD
    //Map file to memory
    uchar *f = outf.map((quint64)bucketno * HASH_BUCKET_SIZE, bucket->size());
            
    //Write to memory if map succeeded
    if (f != 0)
        memmove(f, (const uchar *)bucket->constData(), bucket->size());
    else
        qWarning() << "BucketFlushThread::flushBucketDirect: Big booboo for map! Could not map file. bucket : size" << bucketno << bucket->size();

    if (!outf.unmap(f))
        qWarning() << "BucketFlushThread::flushBucketDirect: Big booboo for map! Could not unmap file. bucket : size" << bucketno << bucket->size();
#endif

    if (outf.isOpen())
        outf.close();

    delete bucket;
    emit bucketFlushed(tth, bucketno);
}

void BucketFlushThread::renameIncompleteFile(QString filename)
{
    QString incompleteFilename = filename + ".incomplete";
    QFile f(incompleteFilename);
    if (!f.rename(filename))
        f.remove();
    emit fileAssemblyComplete(filename);
}
