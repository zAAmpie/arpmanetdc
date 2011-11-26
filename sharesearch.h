#ifndef SHARESEARCH_H
#define SHARESEARCH_H

#include <QObject>
#include <QString>
#include <QList>
#include <QTimer>
#include <QDir>
#include <QRegExp>
#include <QHostAddress>
#include "hashfilethread.h"
#include "parsedirectorythread.h"
#include "execthread.h"
#include "downloadqueuewidget.h"
#include "downloadfinishedwidget.h"
#include "util.h"

class ArpmanetDC;

#define MAX_TTHTREE_PACKET_SIZE 1000 //This should be at least sizeOf(TTH + quint16 + quint32) less than the MTU (For TTH this is 24+2+4=30 bytes less than MTU)

struct SearchStruct //Used to return search results
{
	QByteArray tthRoot;
    QString fileName;
	QString relativePath;
	qint64 fileSize;
    qint16 majorVersion;
    qint16 minorVersion;
};

struct FileListStruct //Used to store fileList for hashing
{
	QString fileName;
	QString rootDir;
};

struct VersionStruct //Used to store/return major and minor version of a file
{
    qint16 majorVersion;
    qint16 minorVersion;
};



class ShareSearch : public QObject
{
	Q_OBJECT

public:
	ShareSearch(quint32 maxSearchResults, ArpmanetDC *parent);
	~ShareSearch();

	//Gets/Sets the total shares
	quint64 totalShare(bool fromDB = false);
	QString totalShareStr(bool fromDB = false);
	void setTotalShare(quint64);

	//Blocking function to get shares from database
	QList<QDir> *getShares(); 
public slots:
	//----------========== SHARE QUERIES (DISPATCHER) ==========----------

	//String query
	void querySearchString(QHostAddress senderHost, QByteArray cid, quint64 id, QByteArray searchPacket);
	
    //Return a struct of a file for a given TTH root
	void queryTTH(QByteArray tthRoot);
	
    //Return the 1MB TTH given a TTH root and file offset
	void query1MBTTH(QByteArray tthRoot, qint64 offset);

	//----------========== TTH SOURCES FOR TRANSFERS (TRANSFER MANAGER) ==========----------
	
    //Save a source for a particular TTH
	void saveTTHSource(QByteArray tthRoot, QHostAddress peerAddress);
	
    //Load a source from a TTH
	void loadTTHSource(QByteArray tthRoot);
	
    //Request filename from a TTH
	void requestFilePath(QByteArray tthRoot);
	
    //Release all sources for a particular TTH
	void deleteTTHSources(QByteArray tthRoot);

    //----------========== TTH REQUESTS FOR ALTERNATE SEARCHING ==========----------

    //Request whether a file is being shared
    void TTHSearchQuestionReceived(QByteArray tth, QHostAddress host);

    //----------========== TTH TREE REQUEST FOR TRANSFERS ==========----------

    //Request a tth tree for a file
    void incomingTTHTreeRequest(QHostAddress host, QByteArray tth);

	//----------========== UPDATE SHARES (GUI) ==========----------
	
    //Sharing - updates shares when shareWidget saves new share structure
	void updateShares(QList<QDir> *dirList); //Takes around 10 secs per 10k files to update including I/O but not hashing
	
    //Convenience function to update existing shares
	void updateShares();

	//----------========== DOWNLOAD QUEUE (DOWNLOAD QUEUE WIDGET) ==========----------
	
    //Save a new entry
	void saveQueuedDownload(QueueStruct file);
	
    //Remove an entry
	void removeQueuedDownload(QByteArray tthRoot);
	
    //Sets the priority of a queued download
	void setQueuedDownloadPriority(QByteArray tthRoot, QueuePriority priority);
	
    //Request queued download list
	void requestQueueList();

	//----------========== FINISHED DOWNLOADS (FINISHED DOWNLOADS WIDGET) ==========----------
	
    //Save an entry
	void saveFinishedDownload(FinishedDownloadStruct file);
	
    //Clear all entries
	void clearFinishedDownloads();
	
    //Request list
	void requestFinishedList();

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
	//----------========== SEARCH QUERIES (DISPATCHER) ==========----------
	
    //Signal to return a search result
	void returnSearchResult(QHostAddress host, QByteArray cid, quint64 id, QByteArray result);
	
    //Signal to return share result for TTH search
	void returnTTHResult(SearchStruct result);
	
    //Signal to return 1MB TTH
	void return1MBTTH(QByteArray tth1MB);

	//----------========== TRANSFERS (TRANSFER MANAGER) ==========----------
	
    //Filename request reply
	void filePathReply(QByteArray tthRoot, QString filePath);
	
    //TTH source load reply
	void tthSourceLoaded(QByteArray tthRoot, QHostAddress peerAddress);

    //----------========== TTH REQUESTS FOR ALTERNATE SEARCHING ==========----------

    //If this signal is emitted, it means the tth is shared!
    void sendTTHSearchResult(QHostAddress host, QByteArray tth);

    //----------========== TTH TREE REQUEST FOR TRANSFERS ==========----------

    //Reply with the TTH Tree packet for the file
    void sendTTHTreeReply(QHostAddress host, QByteArray tthTreePacket);

	//----------========== QUEUED DOWNLOADS (DOWNLOAD QUEUE WIDGET) ==========----------
	
    //Signals incoming queued list
	void returnQueueList(QHash<QByteArray, QueueStruct> *list);
	
    //Signals that an entry has been added
	void queuedDownloadAdded(QueueStruct file);

	//----------========== FINISHED DOWNLOADS (FINISHED DOWNLOAD WIDGET) ==========----------
	
    //Signals incoming finished downloads list
	void returnFinishedList(QList<FinishedDownloadStruct> *list);
	
    //Signals that an entry has been added
	void finishedDownloadAdded(FinishedDownloadStruct file);

	//----------========== PUBLIC HASHING / PARSING (GUI) ==========----------
	
    //Signal to show a file has completed hashing
	void fileHashed(QString fileName);
	
    //Signal to show a directory has been parsed
	void directoryParsed(QString path);

	//Signal to show all hashing is done
	void hashingDone(int msecs, int numFiles);
	//Signal to show all parsing is done
	void parsingDone();

    //----------========== PRIVATE SIGNALS ==========----------

	//Signals to interface with hashing thread objects
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
	
	//Get the total share directly from the database
	qint64 getTotalShareFromDB(); //WARNING: Blocking! 10 msecs per 10k files shared

    //Get the major and minor versions for a specific fileName
    VersionStruct getMajorMinorVersions(QString fileName);

    //Get relative path to root dir
    QString getRelativePath(QString absoluteRootDir, QString absoluteFilePath);

	//Objects
	ArpmanetDC *pParent;
	HashFileThread *pHashFileThread;
	ParseDirectoryThread *pParseDirectoryThread;

	quint32 pMaxResults;
	quint64 pTotalShare;
    int numberOfFilesShared;

	bool transactionInProgress;
	QTimer *commitTimer; //TODO: Cannot use QTimers in a thread other than the GUI thread... Will need to move it somehow.

	//Measure the time taken to update the share db
	QTime *updateTime;
    

	ExecThread *hashThread;

	QList<FileListStruct> *pFileList;
	QList<QDir> *pDirList;
};

#endif