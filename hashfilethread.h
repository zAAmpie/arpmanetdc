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
    void processBucket(QByteArray rootTTH, int bucketNumber, QByteArray *bucket, ReturnEncoding encoding = BinaryEncoded);

signals:
	//Done hashing the file - return the data
	void done(QString filePath, QString fileName, qint64 fileSize, QString tthRoot, QString rootDir, QString modifiedDate, QList<QString> *oneMBList, HashFileThread *hashObj);
	void failed(QString filePath, HashFileThread *hashObj);

    //Done hashing the bucket
    void doneBucket(QByteArray rootTTH, int bucketNumber, QByteArray bucketTTH);

private:
    //QString base32Encode(byte *input, int inputLength);

	ReturnEncoding pEncoding;
};

#endif
