/* This file is part of ArpmanetDC. Copyright (C) 2012
 * Source code can be found at http://code.google.com/p/arpmanetdc/
 * 
 * ArpmanetDC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ArpmanetDC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with ArpmanetDC.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BUCKETFLUSHTHREAD_H
#define BUCKETFLUSHTHREAD_H

#include <QObject>
#include <QFile>
#include "protocoldef.h"
#include <QDebug>

class BucketFlushThread : public QObject
{
    Q_OBJECT
public:
    explicit BucketFlushThread(QObject *parent = 0);

signals:
    void fileAssemblyComplete(QString fileName);
public slots:
    void flushBucket(QString filename, QByteArray *bucket);
    void assembleOutputFile(QString tmpfilebase, QString outfile, int startbucket, int lastbucket);
    void flushBucketDirect(QString filename, int bucketno, QByteArray *bucket);
    void renameIncompleteFile(QString filename);
};

#endif // BUCKETFLUSHTHREAD_H
