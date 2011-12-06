#ifndef BUCKETFLUSHTHREAD_H
#define BUCKETFLUSHTHREAD_H

#include <QObject>
#include <QFile>
#include "protocoldef.h"

class BucketFlushThread : public QObject
{
    Q_OBJECT
public:
    explicit BucketFlushThread(QObject *parent = 0);

signals:

public slots:
    void flushBucket(QString filename, QByteArray *bucket);
    void assembleOutputFile(QString tmpfilebase, QString outfile, int startbucket, int lastbucket);
};

#endif // BUCKETFLUSHTHREAD_H
