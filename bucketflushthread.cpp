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
            
            //Disable MM files due to excessive memory consumption till we figure out how to force release modified memory to disk - to fix lockups
            outf.seek((quint64)bucket * HASH_BUCKET_SIZE);
            outf.write(buf);

            //Map file to memory
            //uchar *f = outf.map((quint64)bucket * HASH_BUCKET_SIZE, buf.size());
            
            //Write to memory if map succeeded
            /*if (f != 0)
                memmove(f, (const uchar *)buf.constData(), buf.size());
            else
                qDebug() << "BucketFlushThread::assembleOutputFile: Big booboo for map! bucket : size" << bucket << buf.size();
                

            /*quint64 fileEnd = (bucket + 1) * HASH_BUCKET_SIZE;
            if (outf.size() < fileEnd)
            {
                outf.seek(fileEnd);
                outf.write("", 0);
            }

            const char *f = (char*)outf.map(bucket * HASH_BUCKET_SIZE, HASH_BUCKET_SIZE);

            // tmp check until mapping is sorted out
            if (outf.size() == bucket * HASH_BUCKET_SIZE)
            {
                QByteArray buf = inf.readAll();
                outf.write(buf);
            }
            QByteArray outputmap(QByteArray::fromRawData(f, HASH_BUCKET_SIZE));
            outputmap = inf.readAll();*/

            inf.close();
            inf.remove();
            //bool res = outf.unmap(f);

        }
        if (bucket == lastbucket)
            outf.rename(outfile);
        bucket++;
    }
    outf.close();
}
