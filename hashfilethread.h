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
#include "cryptlib/tiger.h"
#include "base32.h"

using namespace CryptoPP;

class HashFileThread : public QObject
{
	Q_OBJECT
public:
	//Constructor
	HashFileThread(QObject *parent = 0);
	~HashFileThread();

public slots:
	void processFile(QString filePath, QString rootDir);

signals:
	//Done hashing the file
	void done(QString filePath, QString fileName, qint64 fileSize, QString tthRoot, QString rootDir, QString modifiedDate, QList<QString> *oneMBList, HashFileThread *hashObj);
	void failed(QString filePath, HashFileThread *hashObj);

private:
	QString base32Encode(byte *input, int inputLength);
};

#endif