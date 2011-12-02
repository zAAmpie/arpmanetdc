#include "bucketflushthread.h"

BucketFlushThread::BucketFlushThread(QObject *parent) :
    QObject(parent)
{
}

void BucketFlushThread::flushBucket(QString filename, QByteArray *bucket)
{
    QFile file(filename);
    if ((!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) || (file.write(*bucket) == -1))
    {
        // TODO: emit MISTAKE!, pause download
    }
    file.close();
    delete bucket;
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
            QByteArray buf = inf.readAll();
            outf.write(buf);
            inf.close();
            inf.remove();
        }
        if (bucket == lastbucket)
            outf.rename(outfile);
        bucket++;
    }
    outf.close();
}
