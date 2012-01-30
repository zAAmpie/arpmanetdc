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

#ifndef HASHFILETHREAD_H
#define HASHFILETHREAD_H

#include <QThread>
#include <QObject>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <cryptopp/tiger.h>
#include "util.h"

using namespace CryptoPP;

enum ReturnEncoding {BinaryEncoded=1, Base32Encoded=2, Base64Encoded=4};

class HashFileThread : public QObject
{
	Q_OBJECT
public:
	//Constructor
	HashFileThread(ReturnEncoding encoding = Base64Encoded, QObject *parent = 0);
	~HashFileThread();

public slots:
	//Hashes a particular file
	void processFile(QString filePath, QString rootDir);
    //Hashes a bucket
    void processBucket(QByteArray rootTTH, int bucketNumber, QByteArray bucket, ReturnEncoding encoding = BinaryEncoded);
    //Hashes a file and return just the root TTH
    void hashFile(QString filePath);

    //Stops hashing
    void stopHashing();

signals:
	//Done hashing the file - return the data
	void done(QString filePath, QString fileName, qint64 fileSize, QString tthRoot, QString rootDir, QString modifiedDate, QList<QString> *oneMBList, HashFileThread *hashObj);
	void failed(QString filePath, HashFileThread *hashObj);

    //Done calculating hash
    void doneFile(QString filePath, QByteArray tthRoot, quint64 fileSize);

    //Done hashing the bucket
    void doneBucket(QByteArray rootTTH, int bucketNumber, QByteArray bucketTTH);

private:
    //QString base32Encode(byte *input, int inputLength);

    bool pStopHashing;

	ReturnEncoding pEncoding;
};

#endif
