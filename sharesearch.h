#ifndef SHARESEARCH_H
#define SHARESEARCH_H

#include <QObject>
#include <QString>
#include <QList>
#include <QTimer>
#include <QDir>
#include <QHostAddress>
#include "hashfilethread.h"
#include "parsedirectorythread.h"
#include "execthread.h"

class ArpmanetDC;

struct ShareStruct
{
	QString fileName;
	QByteArray tthRoot;
	qint64 fileSize;
};

struct FileListStruct
{
	QString fileName;
	QString rootDir;
};

class ShareSearch : public QObject
{
	Q_OBJECT

public:
	ShareSearch(quint32 maxSearchResults, ArpmanetDC *parent);
	~ShareSearch();

	//Gets/Sets the total shares
	quint64 totalShare();
	QString totalShareStr();
	void setTotalShare(quint64);	//TODO: Incorporate total shares into this class to query db directly

	//Blocking function to get shares from database
	QList<QDir> *getShares(); 

	//Get the total share directly from the database
	qint64 getTotalShareFromDB(); //WARNING: Blocking! 10 msecs per 10k files shared
public slots:
	//Query
	void querySearchString(QString searchStr);
	//Return a struct of a file for a given TTH root
	void queryTTH(QByteArray tthRoot);
	//Return the 1MB TTH given a TTH root and file offset
	void query1MBTTH(QByteArray tthRoot, qint64 offset);

	//Slots for transfers
	//Save a source for a particular TTH
	void saveTTHSource(QByteArray *tthRoot, QHostAddress *peerAddress);
	//Load a source from a TTH
	void loadTTHSource(QByteArray *tthRoot);
	//Request filename from a TTH
	void requestFilePath(QByteArray *tthRoot);

	//Sharing - updates shares when shareWidget saves new share structure
	void updateShares(QList<QDir> *dirList); //Takes around 10 secs per 10k files to update including I/O but not hashing

private slots:
	//Hash file thread completed
	void hashFileThreadDone(QString filePath, QString fileName, qint64 fileSize, QString tthRoot, QString rootDir, QString lastModified, QList<QString> *oneMBList, HashFileThread *hashObj);
	//Hash file thread failed
	void hashFileFailed(QString filePath, HashFileThread *hashObj);

	//Parse directory thread completed
	void parseDirectoryThreadDone(QString rootDir, QList<QString> *fileList, ParseDirectoryThread *parseObj);
	//Parse directory thread failed
	void parseDirectoryThreadFailed(QString rootDir, ParseDirectoryThread *parseObj);

	//Database commands
	void commitTransaction(bool startNewTransaction = true);

signals:
	//Signal to return a list of search results
	void returnSearchResults(QList<ShareStruct> *resultList);
	//Signal to return share result for TTH search
	void returnTTHResult(ShareStruct result);
	//Signal to return 1MB TTH
	void return1MBTTH(QByteArray tth1MB);

	//Signals for transfers
	//Filename request reply
	void filePathReply(QByteArray *tthRoot, QString *filePath);
	//TTH source load reply
	void tthSourceLoaded(QByteArray *tthRoot, QHostAddress *peerAddress);

	//Signal to show a file has completed hashing
	void fileHashed(QString fileName);
	//Signal to show a directory has been parsed
	void directoryParsed(QString path);

	//Signal to show all hashing is done
	void hashingDone(int msecs);
	//Signal to show all parsing is done
	void parsingDone();

	void runHashThread(QString filePath, QString rootDir);
	void runParseThread(QString directoryPath);

private:
	//Functions to start threads
	void startHashFileThread(QString filePath, QString rootDir);
	void startParseDirectoryThread(QDir directory);

	//Functions to continue processes
	void startDirectoryParsing();
	void startFileHashing();

	//Checks if a file has been modified since the database has been modified
	bool fileNotModified(QString filePath, QString rootDir);
	
	//Sets all files inactive
	void setAllFilesInactive(); //WARNING: Blocking! 500 msecs per 10k files

	//Objects
	ArpmanetDC *pParent;
	HashFileThread *pHashFileThread;
	ParseDirectoryThread *pParseDirectoryThread;

	quint32 pMaxResults;
	quint64 pTotalShare;

	bool transactionInProgress;
	QTimer *commitTimer;

	//Measure the time taken to update the share db
	QTime *updateTime;

	ExecThread *hashThread;

	QList<FileListStruct> *pFileList;
	QList<QDir> *pDirList;
};

#endif