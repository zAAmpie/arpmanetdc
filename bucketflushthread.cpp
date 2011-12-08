#include "bucketflushthread.h"

BucketFlushThread::BucketFlushThread(QObject *parent) :
    QObject(parent)
{
}

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
    QFile outf(outfilepart);
    if (!outf.open(QIODevice::WriteOnly | QIODevice::Append))
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
            const char *f = (char*)outf.map(bucket * HASH_BUCKET_SIZE, HASH_BUCKET_SIZE);
            //QByteArray buf = inf.readAll();
            //outf.write(buf);
            QByteArray outputmap(QByteArray::fromRawData(f, HASH_BUCKET_SIZE));
            outputmap = inf.readAll();
            inf.close();
            inf.remove();
            outf.unmap((unsigned char *)f);
        }
        if (bucket == lastbucket)
            outf.rename(outfile);
        bucket++;
    }
    outf.close();
}
