#include "sharesearch.h"
#include "arpmanetdc.h"
#include "parsedirectorythread.h"

ShareSearch::ShareSearch(quint32 maxSearchResults, ArpmanetDC *parent)
{
    //Constructor
    qRegisterMetaType<ReturnEncoding>("ReturnEncoding");

    pParent = parent;
    pMaxResults = maxSearchResults;

    pFileList = new QList<FileListStruct>();
    pTotalShare = 0;

    pStopHashing = false;
    pStopParsing = false;

    //Commit transactions every minute
    commitTimer = new QTimer(this);
    connect(commitTimer, SIGNAL(timeout()), this, SLOT(commitTransaction()));
    commitTimer->setInterval(60000);

    searchHistoryTimer = new QTimer(this);
    connect(searchHistoryTimer, SIGNAL(timeout()), this, SLOT(garbageCollectSearchHistory()));
    searchHistoryTimer->start(120000); //Every 2min

    updateTime = new QTime();

    //Create a new thread
    hashThread = new ExecThread();
    
    pHashFileThread = new HashFileThread();
    connect(pHashFileThread, SIGNAL(done(QString, QString, qint64, QString, QString, QString, QList<QString> *, HashFileThread *)),
        this, SLOT(hashFileThreadDone(QString, QString, qint64, QString, QString, QString, QList<QString> *, HashFileThread *)), Qt::QueuedConnection);
    connect(pHashFileThread, SIGNAL(failed(QString, HashFileThread *)), this, SLOT(hashFileFailed(QString, HashFileThread *)), Qt::QueuedConnection);
    //connect(pHashFileThread, SIGNAL(doneBucket(QByteArray, int, QByteArray)), this, SLOT(hashBucketDone(QByteArray, int, QByteArray)), Qt::QueuedConnection);
    connect(pHashFileThread, SIGNAL(doneFile(QString, QByteArray, quint64)), this, SIGNAL(returnTTHFromPath(QString, QByteArray, quint64)), Qt::QueuedConnection);
    connect(this, SIGNAL(runHashThread(QString, QString)), pHashFileThread, SLOT(processFile(QString, QString)), Qt::QueuedConnection);
    //connect(this, SIGNAL(runHashBucket(QByteArray, int, QByteArray, ReturnEncoding)), pHashFileThread, SLOT(processBucket(QByteArray, int, QByteArray, ReturnEncoding)), Qt::QueuedConnection);
    connect(this, SIGNAL(stopHashingThread()), pHashFileThread, SLOT(stopHashing()));
    connect(this, SIGNAL(calculateTTHFromPath(QString)), pHashFileThread, SLOT(hashFile(QString)), Qt::QueuedConnection);

    pHashFileThread->moveToThread(hashThread);

    pParseDirectoryThread = new ParseDirectoryThread();
    connect(pParseDirectoryThread, SIGNAL(done(QString, QList<FileListStruct> *, ParseDirectoryThread *)), 
        this, SLOT(parseDirectoryThreadDone(QString, QList<FileListStruct> *, ParseDirectoryThread *)), Qt::QueuedConnection);
    connect(pParseDirectoryThread, SIGNAL(failed(QString, ParseDirectoryThread *)), this, SLOT(parseDirectoryThreadFailed(QString, ParseDirectoryThread *)), Qt::QueuedConnection);
    connect(this, SIGNAL(runParseThread(QString)), pParseDirectoryThread, SLOT(parseDirectory(QString)), Qt::QueuedConnection);
    connect(this, SIGNAL(stopParsingThread()), pParseDirectoryThread, SLOT(stopParsing()));

    pParseDirectoryThread->moveToThread(hashThread);

    hashThread->start();

    //Create hash thread for incoming buckets - file hashing blocks bucket hashing
    hashBucketThread = new ExecThread();
    pHashBucketThread = new HashFileThread();

    connect(pHashBucketThread, SIGNAL(doneBucket(QByteArray, int, QByteArray)), this, SLOT(hashBucketDone(QByteArray, int, QByteArray)), Qt::QueuedConnection);
    connect(this, SIGNAL(runHashBucket(QByteArray, int, QByteArray, ReturnEncoding)), pHashBucketThread, SLOT(processBucket(QByteArray, int, QByteArray, ReturnEncoding)), Qt::QueuedConnection);

    pHashBucketThread->moveToThread(hashBucketThread);
    hashBucketThread->start();

    //Create container thead
    containerThread = new ExecThread();

    pContainerThread = new ContainerThread();
    connect(pContainerThread, SIGNAL(returnContainers(QHash<QString, ContainerContentsType>)), 
        this, SIGNAL(returnContainers(QHash<QString, ContainerContentsType>)), Qt::QueuedConnection);
    connect(pContainerThread, SIGNAL(requestTTHsFromPaths(QHash<QString, QStringList>, QString)),
        this, SLOT(requestTTHsFromPaths(QHash<QString, QStringList>, QString)), Qt::QueuedConnection);
    connect(pContainerThread, SIGNAL(containersSaved()),
        this, SLOT(updateShares()), Qt::QueuedConnection);
    connect(this, SIGNAL(getContainers(QString)), 
        pContainerThread, SLOT(requestContainers(QString)), Qt::QueuedConnection);
    connect(this, SIGNAL(saveContainers(QHash<QString, ContainerContentsType>, QString)),
        pContainerThread, SLOT(saveContainers(QHash<QString, ContainerContentsType>, QString)), Qt::QueuedConnection);
    connect(this, SIGNAL(returnTTHsFromPaths(QHash<QString, QList<ContainerLookupReturnStruct> >, QString)),
        pContainerThread, SLOT(returnTTHsFromPaths(QHash<QString, QList<ContainerLookupReturnStruct> >, QString)), Qt::QueuedConnection);
    connect(this, SIGNAL(procContainer(QHostAddress, QString, QString)),
        pContainerThread, SLOT(processContainer(QHostAddress, QString, QString)), Qt::QueuedConnection);
    
    connect(pContainerThread, SIGNAL(returnProcessedContainer(QHostAddress, ContainerContentsType, QList<ContainerLookupReturnStruct>, QString)),
        pParent, SLOT(returnProcessedContainer(QHostAddress, ContainerContentsType, QList<ContainerLookupReturnStruct>, QString)), Qt::QueuedConnection);

    pContainerThread->moveToThread(containerThread);

    containerThread->start();
}

ShareSearch::~ShareSearch()
{
    //Destructor
    delete pDirList;
    delete pFileList;

    pHashFileThread->deleteLater();
    pParseDirectoryThread->deleteLater();
    pContainerThread->deleteLater();

    hashThread->quit();
    if (hashThread->wait(5000))
        delete hashThread;
    else
    {
        hashThread->terminate();
        delete hashThread;
    }
    hashBucketThread->quit();
    if (hashBucketThread->wait(5000))
        delete hashBucketThread;
    else
    {
        hashBucketThread->terminate();
        delete hashBucketThread;
    }
    containerThread->quit();
    if (containerThread->wait(5000))
        delete containerThread;
    else
    {
        containerThread->terminate();
        delete containerThread;
    }

    delete updateTime;
}

//------------------------------============================== THREAD MANAGEMENT ==============================------------------------------

void ShareSearch::startHashFileThread(QString filePath, QString rootDir)
{
    if (hashThread->isRunning())
        emit runHashThread(filePath, rootDir);
    else
        QString error = "hash thread closed?";
}

void ShareSearch::startParseDirectoryThread(QDir directory)
{
    if (hashThread->isRunning())
        emit runParseThread(directory.absolutePath());
    else
        QString error = "hash thread closed?";
    
}

//------------------------------============================== UPDATE SHARES (GUI) ==============================------------------------------

void ShareSearch::updateShares(QList<QDir> *dirList) //500 msecs to update SharePaths!
{
    updateTime->start();
    pDirList = dirList;

    //Construct a directory list insert query
    QList<QByteArray> queries;

    queries.append("BEGIN;");
    transactionInProgress = true;
    commitTimer->start();

    //Set all files as inactive
    setAllFilesInactive();

    if (!dirList->isEmpty())
    {
        //Delete everything from the sharePath table
        QByteArray removeQuery;
        removeQuery.append(tr("DELETE FROM SharePaths WHERE [path] NOT IN ("));    

        //Insert the directory and file listing
        for (int k = 0; k < dirList->size(); k++)
        {
            if (k != 0)
                removeQuery.append(",");
            removeQuery.append(tr("?"));

            QByteArray query;
            query.append(tr("INSERT INTO SharePaths ([path]) SELECT ?;"));
            queries.append(query);

        }
        removeQuery.append(");");
        queries.append(removeQuery);
    }
    else
    {
        //Delete everything from the sharePath table
        QByteArray removeQuery;
        removeQuery.append(tr("DELETE FROM SharePaths;"));    
        queries.append(removeQuery);
    }

    //Write list to database
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;
    
    int count = 0;
    for (int i = 0; i < queries.size(); i++)
    {
        if (sqlite3_prepare_v2(db, queries.at(i).data(), -1, &statement, 0) == SQLITE_OK)
        {
            if (queries.at(i).contains("DELETE FROM SharePaths WHERE"))
            {
                int res = 0;
                for (int k = 0; k < dirList->size(); k++)
                {
                    QString path = dirList->at(k).absolutePath();
                    res = res | sqlite3_bind_text16(statement, k+1, path.utf16(), path.size()*2, SQLITE_STATIC);
                }
                if (res != SQLITE_OK)
                    QString error = "error";
            }

            if (queries.at(i).contains("INSERT INTO SharePaths"))
            {
                int res = 0;
                QString path = dirList->at(count++).absolutePath();
                res = res | sqlite3_bind_text16(statement, 1, path.utf16(), path.size()*2, SQLITE_STATIC);
                
                if (res != SQLITE_OK)
                    QString error = "error";
            }

            int cols = sqlite3_column_count(statement);
            while (sqlite3_step(statement) == SQLITE_ROW);
            sqlite3_finalize(statement);    
        }

        //Catch error message
        QString error = sqlite3_errmsg(db);
        if (error != "not an error")
            QString errorStr = "Error";
    }

    if (!dirList->isEmpty())
        //Start parsing
        startDirectoryParsing();
    else
    {
        //Flag all files as inactive when everything is unshared
        setAllFilesInactive();
        emit hashingDone(0,0);
    }
}

QList<QDir> ShareSearch::getShares()
{
    //Query whether a file has been modified - returns results if it has
    QString queryStr = tr("SELECT [path] FROM SharePaths;");

    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    QList<QDir> shares;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (true)
        {
            //Step through query results
            result = sqlite3_step(statement);

            //If result is a row, add to results
            if (result == SQLITE_ROW)
            {
                for (int col = 0; col < cols; col++)
                {
                    shares.append(QDir(QString::fromUtf16((const unsigned short *)sqlite3_column_text16(statement, col))));
                }                
            }
            //Otherwise, break - usually means SQLITE_DONE
            else
            {
                break;
            }
        }
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    return shares;
}

void ShareSearch::updateShares()
{
    //Convenience function to update existing shares
    QList<QDir> *returnedValue = new QList<QDir>(getShares());
    updateShares(returnedValue);
}

//------------------------------============================== GENERIC DATABASE COMMANDS ==============================------------------------------

//Database commands
void ShareSearch::commitTransaction(bool startNewTransaction)
{    
    QList<QString> queries;

    //Add query to list
    queries.append("COMMIT;");
    if (startNewTransaction)
        queries.append("BEGIN;"); //Start another transaction by default

    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Loop through all queries
    for (int i = 0; i < queries.size(); i++)
    {
        //Prepare a query
        QByteArray query;
        query.append(queries.at(i));
        if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
        {
            int cols = sqlite3_column_count(statement);
            while (sqlite3_step(statement) == SQLITE_ROW);
            sqlite3_finalize(statement);    
        }

        //Catch all error messages
        QString error = sqlite3_errmsg(db);
        if (error != "not an error")
            QString errorStr = "Error";
    }    

    //Stop commit if transaction in progress
    if (!transactionInProgress)
    {
        commitTimer->stop();
        return;
    }
}

void ShareSearch::setAllFilesInactive() //WARNING: Blocking! 500 msecs per 10k files
{
    QString queryStr;

    //Add query to list
    queryStr.append("UPDATE FileShares SET [active] = 0;");

    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        int cols = sqlite3_column_count(statement);
        while (sqlite3_step(statement) == SQLITE_ROW);
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString errorStr = "Error";
}

qint64 ShareSearch::getTotalShareFromDB() //WARNING: Blocking! 10 msecs per 10k files
{
    QString queryStr;

    //Add query to list
    queryStr.append("SELECT SUM(fileSize) FROM FileShares WHERE [active] = 1;");

    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        int cols = sqlite3_column_count(statement);
        while (sqlite3_step(statement) == SQLITE_ROW)
            pTotalShare = sqlite3_column_int64(statement, 0);

        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString errorStr = "Error";

    return pTotalShare;
}

//------------------------------============================== HASHING ==============================------------------------------

//Hash file thread completed
void ShareSearch::hashFileThreadDone(QString filePath, QString fileName, qint64 fileSize, QString tthRoot, QString rootDir, QString lastModified, QList<QString> *oneMBList, HashFileThread *hashObj)
{
    QString done = "done";
    QList<QString> queries;

    //Generate fileShare query to insert data into database
    QString fileShareQuery = tr("INSERT INTO FileShares ([tth], [fileName], [fileSize], [filePath], [lastModified], [shareDirID], [active], [majorVersion], [minorVersion], [relativePath]) VALUES (?, ?, ?, ?, ?, (SELECT [rowID] FROM SharePaths WHERE path = ?), 1, ?, ?, ?);");

    //Add query to list
    queries.append(fileShareQuery);

    //Generate 1MB tth query - max blocks of 50 selects
    int count = 0;
    qint64 offset = -1048576;
    while (!oneMBList->isEmpty())
    {
        offset += 1048576;

        QByteArray query;
        query.append(tr("INSERT INTO OneMBTTH ([oneMBtth], [tth], [offset], [fileShareID]) SELECT '%1', '%2', %3, (SELECT rowID FROM FileShares WHERE filePath = ?001) ")
            .arg(oneMBList->takeFirst())
            .arg(tthRoot)
            .arg(offset));     

        while ((count < 50) && (!oneMBList->isEmpty()))
        {
            offset += 1048576;

            query.append(tr("UNION SELECT '%1', '%2', %3, (SELECT rowID FROM FileShares WHERE filePath = ?001) ")
                .arg(oneMBList->takeFirst())
                .arg(tthRoot)
                .arg(offset));
            
            count++;
        }
        query.append(";");    
        count = 0;

        //Add query to list
        queries.append(query);
    }    

    //Generate totalFileShares query
    QString totalFileShares = tr("SELECT SUM(fileSize) FROM FileShares WHERE [active] = 1;");
    //queries.append(totalFileShares);

    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    qint64 totalShare;
   
    //Get version numbers
    VersionStruct v = getMajorMinorVersions(fileName);   
    QString relativePath = getRelativePath(rootDir, filePath);

    //Loop through all queries
    for (int i = 0; i < queries.size(); i++)
    {
        //Prepare a query
        QByteArray query;
        query.append(queries.at(i));
        if (sqlite3_prepare_v2(db, query.data(), query.size(), &statement, 0) == SQLITE_OK)
        {
            int res = 0;
            if (query.contains("INSERT INTO FileShares")) 
            {
                /*TTH*/         res = res | sqlite3_bind_text16(statement, 1, tthRoot.utf16(), tthRoot.size()*2, SQLITE_STATIC);
                /*FileName*/    res = res | sqlite3_bind_text16(statement, 2, fileName.utf16(), fileName.size()*2, SQLITE_STATIC);
                /*FileSize*/    res = res | sqlite3_bind_int64(statement, 3, fileSize);
                /*FilePath*/    res = res | sqlite3_bind_text16(statement, 4, filePath.utf16(), filePath.size()*2, SQLITE_STATIC);
                /*LastModified*/res = res | sqlite3_bind_text16(statement, 5, lastModified.utf16(), lastModified.size()*2, SQLITE_STATIC);
                /*SharePath*/   res = res | sqlite3_bind_text16(statement, 6, rootDir.utf16(), rootDir.size()*2, SQLITE_STATIC);
                /*MajorVersion*/res = res | sqlite3_bind_int64(statement, 7, v.majorVersion);
                /*MinorVersion*/res = res | sqlite3_bind_int64(statement, 8, v.minorVersion);
                /*RelativePath*/res = res | sqlite3_bind_text16(statement, 9, relativePath.utf16(), relativePath.size()*2, SQLITE_STATIC);
            }
            else if (query.contains("INSERT INTO OneMBTTH")) 
            {
                res = sqlite3_bind_text16(statement, 1, filePath.utf16(), filePath.size()*2, SQLITE_STATIC);
            }

            if (res != SQLITE_OK)
                QString error = "Meh";

            int cols = sqlite3_column_count(statement);
            while (sqlite3_step(statement) == SQLITE_ROW)
            {
                totalShare = sqlite3_column_int64(statement, 0);    
            };
            sqlite3_finalize(statement);    
        }

        //Catch all error messages
        QString error = sqlite3_errmsg(db);
        if (error != "not an error")
            QString errorStr = "Error";
    }

    //Clean up
    delete oneMBList;

    //Update share size
    pTotalShare += fileSize;

    //Emit signal for status
    emit fileHashed(filePath, fileSize, pTotalShare);
    
    //Continue with next file in the list
    startFileHashing();
}

//Hash file thread failed
void ShareSearch::hashFileFailed(QString filePath, HashFileThread *hashObj)
{
    QString done = "failed";

    //Continue with next file in the list
    startFileHashing();
}

void ShareSearch::startFileHashing()
{
    //Start file hashing and start new thread
    FileListStruct f;
    bool notModified;

    do
    {
        if (!pFileList->isEmpty())
        {
            //QApplication::processEvents();

            f = pFileList->takeFirst();
            //Check if modified
            notModified = fileNotModified(f.fileName, f.rootDir);
            if (notModified)
                QString res = "File TTH in DB is still valid";
            else
                break;
        }
        else
        {
            notModified = true;
            break;
        }
    } while (notModified);

    if (!notModified)
        startHashFileThread(f.fileName, f.rootDir);
    else
    {
        commitTimer->stop();
        commitTransaction(false);
        int totalUpdateTime = updateTime->elapsed();
        emit hashingDone(totalUpdateTime, numberOfFilesShared);
    }
}


//Stop hashing
void ShareSearch::stopHashing()
{
    emit stopHashingThread();
}

//------------------------------============================== DIRECTORY PARSING (FILE LIST EXTRACTION) ==============================------------------------------

void ShareSearch::parseDirectoryThreadDone(QString rootDir, QList<FileListStruct> *fileList, ParseDirectoryThread *parseObj)
{
    QString done = "done";
    //Add filelist to local variable for hashing
    /*foreach (QString file, *fileList)
    {
        FileListStruct fStruct;
        fStruct.fileName = file;
        fStruct.rootDir = rootDir;
        pFileList->append(fStruct);
    }*/
    pFileList->append(*fileList);
    
    //Clean up
    delete fileList;

    //Emit directory parsed signal for status
    emit directoryParsed(rootDir);    

    //Continue with next directory
    startDirectoryParsing();
}

void ShareSearch::parseDirectoryThreadFailed(QString rootDir, ParseDirectoryThread *parseObj)
{
    QString done = "done";

    //Continue with next directory
    startDirectoryParsing();
}

void ShareSearch::startDirectoryParsing()
{
    //Start directory parsing and start new thread
    if (!pDirList->isEmpty())
    {
        startParseDirectoryThread(pDirList->takeFirst());
    }
    else
    {        
        emit parsingDone(updateTime->elapsed());

        numberOfFilesShared = pFileList->size();

        //Start file hashing on file list
        startFileHashing();
    }
}

//Stop parsing
void ShareSearch::stopParsing()
{
    emit stopParsingThread();
}

//------------------------------============================== CHECK IF FILE IN DATABASE WAS MODIFIED FROM LAST VISIT ==============================------------------------------

bool ShareSearch::fileNotModified(QString filePath, QString rootDir)
{
    //Get current modified date of file
    QFileInfo fileInfo(filePath);
    QString lastModifiedFile;
    bool fileExists = fileInfo.exists();

    QList<QString> queries;

    //Query whether a file has been modified - returns results if it has
    QString queryStr = tr("SELECT [lastModified] FROM FileShares WHERE filePath = ?;");

    //Update the file share entry - change the sharepath owner of the entry if needed
    QString updateStr = tr("UPDATE fileShares SET [shareDirID] = (SELECT rowID FROM SharePaths WHERE path = ?) WHERE filePath = ?;");

    //Set the file share active (if readding an existing share)
    QString setActiveStr = tr("UPDATE fileShares SET [active] = 1 WHERE filePath = ?;");

    //Delete all 1MB TTHs for a particular file share - should be called before deleteStr!
    QString deleteOneMBStr = tr("DELETE FROM OneMBTTH WHERE [fileShareID] = (SELECT [rowID] FROM FileShares WHERE filePath = ?);");

    //Delete the hash entry from the database if it exists
    QString deleteStr = tr("DELETE FROM FileShares WHERE filePath = ?;");  

    bool result = true; //By default, DO NOT hash file

    //Check if file exists
    if (fileExists)
    {
        lastModifiedFile = fileInfo.lastModified().toString("dd-MM-yyyy HH:mm:ss:zzz");
        queries.append(queryStr);
    }
    else
    {
        queries.append(deleteOneMBStr);
        queries.append(deleteStr);
    }

    QString lastModifiedDB;
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    while (!queries.isEmpty())
    {
        QByteArray query;
        query.append(queries.takeFirst());
        if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
        {
            //Bind parameters for each query
            int res = 0;
            if (query.contains("SELECT [lastModified]"))
            {
                res = res | sqlite3_bind_text16(statement, 1, filePath.utf16(), filePath.size()*2, SQLITE_STATIC);
            }
            else if (query.contains("UPDATE fileShares SET [shareDirID]"))
            {
                res = res | sqlite3_bind_text16(statement, 1, rootDir.utf16(), rootDir.size()*2, SQLITE_STATIC);
                //res = res | sqlite3_bind_text16(statement, 2, lastModifiedFile.utf16(), lastModifiedFile.size()*2, SQLITE_STATIC);
                res = res | sqlite3_bind_text16(statement, 2, filePath.utf16(), filePath.size()*2, SQLITE_STATIC);
            }
            else if (query.contains("UPDATE fileShares SET [active]"))
            {
                res = res | sqlite3_bind_text16(statement, 1, filePath.utf16(), filePath.size()*2, SQLITE_STATIC);
            }
            else if (query.contains("DELETE FROM FileShares"))
            {
                res = res | sqlite3_bind_text16(statement, 1, filePath.utf16(), filePath.size()*2, SQLITE_STATIC);
            }
            else if (query.contains("DELETE FROM OneMBTTH"))
            {
                res = res | sqlite3_bind_text16(statement, 1, filePath.utf16(), filePath.size()*2, SQLITE_STATIC);
            }

            if (res != SQLITE_OK)
                QString error = "error";

            //Get results
            int cols = sqlite3_column_count(statement);
            while (sqlite3_step(statement) == SQLITE_ROW)
            {
                if (cols == 1 && query.contains("SELECT [lastModified]"))
                    lastModifiedDB = QString::fromUtf16((const unsigned short *)sqlite3_column_text16(statement, 0));
            }    
            sqlite3_finalize(statement);

            //This should give a result if the filepath exists in the db
            if (query.contains("SELECT [lastModified]"))
            {
                if (lastModifiedDB.isEmpty())
                {
                    //File doesn't exist in db - hash
                    result = false;
                }
                else if (lastModifiedDB != lastModifiedFile)
                {
                    //File was modified - delete entries and hash
                    queries.append(deleteOneMBStr);
                    queries.append(deleteStr);
                    result = false;
                }
                else
                {
                    //File is still fine in the db - update entries but don't hash
                    queries.append(updateStr);
                    queries.append(setActiveStr);
                }
            }
        }

        //Catch all error messages
        QString error = sqlite3_errmsg(db);
        if (error != "not an error")
            QString error = "error";
    }

    return result;
}

//------------------------------============================== SHARE QUERIES (DISPATCHER) ==============================------------------------------

//Query search string
void ShareSearch::querySearchString(QHostAddress senderHost, QByteArray cid, quint64 id, QByteArray searchPacket)
{
    if (searchPacket.size() < 4)
        return;

    //Packet query structure
    //========================================
    //majorVersion      qint16
    //minorVersion      qint16
    //searchStr         QString (variable size)
    
    //Get data from packet
    qint16 majorVersion = getQint16FromByteArray(&searchPacket);
    qint16 minorVersion = getQint16FromByteArray(&searchPacket);
    QString searchStr = getStringFromByteArray(&searchPacket); 
   
    //Check if the same host asked for the same search query in the past few seconds
    qint64 currentAge = QDateTime::currentMSecsSinceEpoch();
    qint64 age = searchHistoryHash.value(cid).value(searchStr); //will be 0 if not found
    if (age != 0 && (currentAge - age < 10000))
    {
        //Return if found and the host searched for this query in the past 10 seconds
        return;
    }
    else
    {
        //Either not found or searched more than 10 seconds ago for this string
        if (!searchHistoryHash.contains(cid))
        {
            //This host hasn't searched yet - insert
            QHash<QString, qint64> hash;
            hash.insert(searchStr, currentAge);
            searchHistoryHash.insert(cid, hash);
        }
        else
        {
            //This host has searched, insert/overwrite the current searchstring
            searchHistoryHash[cid].insert(searchStr, currentAge);
        }
    }

    //Split string into words
    QStringList wordList = searchStr.split(" ");

    //Query the database with the search string
    QString queryStr = tr("SELECT [tth], [fileName], [fileSize], [majorVersion], [minorVersion], [relativePath] FROM FileShares WHERE [active] = 1 AND (");

    bool first = true;

    //Add major and minor versions
    if (majorVersion > 0)
    {
        first = false;
        queryStr.append(tr("([majorVersion] = %1 ").arg(majorVersion));
    }
    if (minorVersion > 0)
    {
        if (!first)
            queryStr.append("AND ");
        else
            queryStr.append("(");
        first = false;
        queryStr.append(tr("[minorVersion] = %1 ").arg(minorVersion));
    }

    //Check for Base32 TTH's in the searchStr
    QString regex = "([a-z2-7]{39})";
    QRegExp rxTTH(regex, Qt::CaseInsensitive);
    QStringList tthList;

    //Iterate through all words
    int count = 0;
    while (count < wordList.size())
    {
        bool found = false;
        QString word = wordList.at(count++);

        int pos = 0;
        while ((pos = rxTTH.indexIn(word, pos)) != -1)
        {
            found = true;
            QByteArray tth;
            tth.append(rxTTH.cap(1).toUpper());
            int size = tth.size();
            base32Decode(tth);
            tthList.append(tth.toBase64());
            pos++;

            //Only add max 10 TTH's per word
            if (tthList.size() >= 10)
                break;
        }

        if (found)
        {
            count--;
            wordList.removeAt(count);
        }
    }

    //Add word queries
    for (int i = 0; i < wordList.size(); i++)
    {
        if (wordList.at(i).isEmpty())
            continue;
        if (!first)
            queryStr.append(" AND ");
        else
            queryStr.append("(");
        queryStr.append(tr("[fileName] || [relativePath] LIKE '%' || ? || '%'"));
        first = false;
    }
    if (!first)
        queryStr.append(")");

    //Add tth queries
    for (int i = 0; i < tthList.size(); i++)
    {
        if (tthList.at(i).isEmpty())
            continue;
        if (!first)
            queryStr.append(" OR ");
        queryStr.append(tr("[tth] = '%1'").arg(tthList.at(i)));
        first = false;
    }

    queryStr.append(tr(") LIMIT %1;").arg(pMaxResults));

    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        for (int i = 0; i < wordList.size(); i++)
        {
            int res = 0;
            res = res | sqlite3_bind_text16(statement, i+1, wordList.at(i).utf16(), wordList.at(i).size()*2, SQLITE_STATIC);
            
            if (res != SQLITE_OK)
                QString error = "error";
        }


        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            if (cols = 6)
            {
                SearchStruct s;
                QByteArray tthRoot = QByteArray().append((char*)sqlite3_column_text(statement, 0));
                s.tthRoot = QByteArray::fromBase64(tthRoot);
                s.fileName = QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 1));
                s.fileSize = sqlite3_column_int64(statement, 2);
                s.majorVersion = sqlite3_column_int64(statement, 3);
                s.minorVersion = sqlite3_column_int64(statement, 4);
                s.relativePath = QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 5));

                //Construct packet
                QByteArray packet;

                //Packet reply structure
                //===========================================
                //fileName          String (variable size)
                //relativePath      String (variable size)
                //fileSize          quint64
                //majorVersion      qint16
                //minorVersion      qint16
                //tthRoot           QByteArray (variable size)

                packet.append(stringToByteArray(s.fileName));
                packet.append(stringToByteArray(s.relativePath));
                packet.append(quint64ToByteArray(s.fileSize));
                packet.append(qint16ToByteArray(s.majorVersion));
                packet.append(qint16ToByteArray(s.minorVersion));
                packet.append(s.tthRoot);
                
                //Report results
                emit returnSearchResult(senderHost, cid, id, packet);    
            }                
        }
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";
}

//Return a struct of a file for a given TTH root
void ShareSearch::queryTTH(QByteArray tthRoot)
{
    //Query the database with the search string
    QString queryStr = tr("SELECT DISTINCT [tth], [fileName], [fileSize] FROM FileShares WHERE [active] = 1 AND tth = '%1';").arg(QString(tthRoot.toBase64()));

    SearchStruct results;
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            if (cols = 3)
            {
                QByteArray tthRoot = QByteArray().append((char*)sqlite3_column_text(statement, 0));
                results.tthRoot = QByteArray::fromBase64(tthRoot);
                results.fileName = QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 1));
                results.fileSize = sqlite3_column_int64(statement, 2);
            }                    
        }
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    //Report results
    emit returnTTHResult(results);
    
}

//Return the 1MB TTH given a TTH root and file offset
void ShareSearch::query1MBTTH(QByteArray tthRoot, qint64 offset)
{
    //Return the 1MB TTH when the tth match and the offset is within the 1MB bucket
    QString queryStr = tr("SELECT [oneMBtth] FROM OneMBTTH WHERE [tth] = '%1' AND ([offset] > %2 AND [offset] <= %3);").arg(QString(tthRoot.toBase64())).arg(offset-1048576).arg(offset);

    QByteArray results;
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            QByteArray tthRoot = QByteArray().append((char*)sqlite3_column_text(statement, 0));
            results = QByteArray::fromBase64(tthRoot);
        }
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    //Report results
    emit return1MBTTH(results);
}


//Save the last known bootstrapped peers
void ShareSearch::saveLastKnownPeers(QList<QHostAddress> peers)
{
    if (peers.isEmpty())
        return;

    //Insert a bootstrap peer into the database
    QStringList queryStr;
    queryStr.append(tr("DELETE FROM LastKnownPeers;"));
    for (int i = 0; i < peers.size(); i++)
    {
        if (peers.at(i).toString().isEmpty())
            peers.removeAt(i--);
        else
            queryStr.append(tr("INSERT INTO LastKnownPeers ([ip]) VALUES (?);"));
    }

    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    for (int i = 0; i < queryStr.size(); i++)
    {
        //Prepare a query
        QByteArray query;
        query.append(queryStr.at(i));
        if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
        {
            //Bind parameters
            int res = 0;
            if (query.contains("INSERT INTO"))
            {
                QString ip = peers.at(i-1).toString();
                res = res | sqlite3_bind_text16(statement, 1, ip.utf16(), ip.size()*2, SQLITE_STATIC);
            }

            int cols = sqlite3_column_count(statement);
            int result = 0;
            while (sqlite3_step(statement) == SQLITE_ROW);
            sqlite3_finalize(statement);    
        }

        //Catch all error messages
        QString error = sqlite3_errmsg(db);
        if (error != "not an error")
            QString error = "error";
    }

    //Commit to ensure access to database hasn't blocked hashing process
    commitTransaction();
}

//Return the last known bootstrapped peers
void ShareSearch::requestLastKnownPeers()
{
    //Query the database with the search string
    QString queryStr = tr("SELECT DISTINCT [ip] FROM LastKnownPeers;");

    QList<QHostAddress> peers;
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            peers.append(QHostAddress(QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 0))));
        }
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    //Signal to return last known peers
    emit sendLastKnownPeers(peers);
}

//------------------------------============================== USER COMMANDS (SETTINGS WIDGET) ==============================------------------------------

//Get all user commands from the database
void ShareSearch::requestUserCommands()
{
    QHash<QString, UserCommandStruct> *commands = new QHash<QString, UserCommandStruct>();

    //Query the database with the search string
    QString queryStr = tr("SELECT [name], [command], [parameterCount], [output] FROM UserCommands;");

    sqlite3 *db = pParent->database();
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            UserCommandStruct u;
            QString name = QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 0));
            u.command = QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 1));
            u.parameterCount = sqlite3_column_int(statement, 2);
            u.output = QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 3));
            commands->insert(name, u);
        }
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    emit returnUserCommands(commands);
}

//Save user commands to the database
void ShareSearch::saveUserCommands(QHash<QString, UserCommandStruct> *commands)
{
    if (commands->isEmpty())
        return;

    //Insert a bootstrap peer into the database
    QStringList queryStr;
    queryStr.append(tr("DELETE FROM UserCommands;"));

    QMutableHashIterator<QString, UserCommandStruct> i(*commands);
    while (i.hasNext())
    {
        i.next();
        if (i.key().isEmpty() || i.value().command.isEmpty())
            i.remove();
        else
            queryStr.append(tr("INSERT INTO UserCommands ([name], [command], [parameterCount], [output]) VALUES (?, ?, ?, ?);"));
    }
    
    sqlite3 *db = pParent->database();
    sqlite3_stmt *statement;

    QHashIterator<QString, UserCommandStruct> k(*commands);
    for (int i = 0; i < queryStr.size(); i++)
    {
        //Prepare a query
        QByteArray query;
        query.append(queryStr.at(i));
        if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
        {
            //Bind parameters
            int res = 0;
            if (query.contains("INSERT INTO") && k.hasNext())
            {
                k.next();
                UserCommandStruct u = k.value();
                QString name = k.key();
                res = res | sqlite3_bind_text16(statement, 1, name.utf16(), name.size()*2, SQLITE_STATIC);
                res = res | sqlite3_bind_text16(statement, 2, u.command.utf16(), u.command.size()*2, SQLITE_STATIC);
                res = res | sqlite3_bind_int(statement, 3, u.parameterCount);
                res = res | sqlite3_bind_text16(statement, 4, u.output.utf16(), u.output.size()*2, SQLITE_STATIC);
            }

            int cols = sqlite3_column_count(statement);
            int result = 0;
            while (sqlite3_step(statement) == SQLITE_ROW);
            sqlite3_finalize(statement);    
        }

        //Catch all error messages
        QString error = sqlite3_errmsg(db);
        if (error != "not an error")
            QString error = "error";
    }

    //Commit to ensure access to database hasn't blocked hashing process
    commitTransaction();
}

//------------------------------============================== GET HASH FROM FILE PATCH (SHARE WIDGET) ==============================------------------------------

//Gets the hash from a filepath if it exists in the database
void ShareSearch::requestTTHFromPath(QString filePath)
{
    //Query the database with the search string
    QString queryStr = tr("SELECT DISTINCT [tth], [fileSize] FROM FileShares WHERE [active] = 1 AND [filePath] = ?;");

    QByteArray tthResult;
    quint64 fileSize;
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        //Bind parameters
        int res = 0;
        res = res | sqlite3_bind_text16(statement, 1, filePath.utf16(), filePath.size()*2, SQLITE_STATIC);

        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            QByteArray tthRoot = QByteArray().append((char*)sqlite3_column_text(statement, 0));
            tthResult = QByteArray::fromBase64(tthRoot);
            fileSize = sqlite3_column_int64(statement, 1);
        }
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    if (!tthResult.isEmpty())
    {
        //Report results
        emit returnTTHFromPath(filePath, tthResult, fileSize);
    }
    else
    {
        //Hash file
        emit calculateTTHFromPath(filePath);
    }
}

//Request the current shares
void ShareSearch::requestShares()
{
    emit returnShares(getShares());
}

//------------------------------============================== GET HASHES FROM FILEPATHS (CONTAINERTHREAD) ==============================------------------------------

//Gets a list of TTHs from a list of paths that exist in the database
void ShareSearch::requestTTHsFromPaths(QHash<QString, QStringList> filePaths, QString containerPath)
{
    //Query the database with the search string
    QString queryStr;
    
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    QHash<QString, QList<ContainerLookupReturnStruct> > result;

    QHashIterator<QString, QStringList> i(filePaths);
    //Iterate through all paths
    while (i.hasNext())
    {
        i.next();

        QList<ContainerLookupReturnStruct> returnList;

        QListIterator<QString> k(i.value());
        //Iterate through all files in each path
        while (k.hasNext())
        {
            QString filePath = k.next();
            queryStr = tr("SELECT [tth], [fileSize] FROM FileShares WHERE [filePath] = ?;");

            QByteArray tthResult;
            quint64 fileSize;
    
            //Prepare a query
            QByteArray query;
            query.append(queryStr);
            if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
            {
                //Bind parameters
                int res = 0;
                res = res | sqlite3_bind_text16(statement, 1, filePath.utf16(), filePath.size()*2, SQLITE_STATIC);

                int cols = sqlite3_column_count(statement);
                int result = 0;
                while (sqlite3_step(statement) == SQLITE_ROW)
                {
                    QByteArray tthRoot = QByteArray().append((char*)sqlite3_column_text(statement, 0));
                    tthResult = QByteArray::fromBase64(tthRoot);
                    fileSize = sqlite3_column_int64(statement, 1);
                }
                sqlite3_finalize(statement);    
            }

            //Catch all error messages
            QString error = sqlite3_errmsg(db);
            if (error != "not an error")
                QString error = "error";

            //Construct struct
            ContainerLookupReturnStruct c;
            c.filePath = filePath;
            c.fileSize = fileSize;
            c.rootTTH = tthResult;

            //Add struct to list
            returnList.append(c);
        }

        //Add list to hash
        result.insert(i.key(), returnList);
    }
    
    //Return results
    emit returnTTHsFromPaths(result, containerPath);
}

//Request all containers in a directory
void ShareSearch::requestContainers(QString containerDirectory)
{
    //Emit signal to containerThread
    emit getContainers(containerDirectory);
}
    
//Process a container
void ShareSearch::processContainer(QHostAddress host, QString containerPath, QString downloadPath)
{
    //Emit signal to containerThread
    emit procContainer(host, containerPath, downloadPath);
}


//------------------------------============================== HASH 1MB BUCKET (TRANSFER MANAGER) ==============================------------------------------

//Hashes a 1MB bucket
void ShareSearch::hashBucketRequest(QByteArray rootTTH, int bucketNumber, QByteArray bucket)
{
    if (!bucket.isEmpty())
        //Start hash bucket thread
        emit runHashBucket(rootTTH, bucketNumber, bucket, BinaryEncoded);
    else
        //Meh. Not going to hash an empty bucket
        emit hashBucketReply(rootTTH, bucketNumber, QByteArray());
}

//Hash bucket thread done
void ShareSearch::hashBucketDone(QByteArray rootTTH, int bucketNumber, QByteArray bucketTTH)
{
    //Signal the reply of the 1MB bucket hash
    emit hashBucketReply(rootTTH, bucketNumber, bucketTTH);
}

//------------------------------============================== TTH SOURCES FOR TRANSFERS (TRANSFER MANAGER) ==============================------------------------------

//Save a source for a particular TTH
void ShareSearch::saveTTHSource(QByteArray tthRoot, QHostAddress peerAddress)
{
    //Insert a source for a particular TTH value
    QString queryStr = tr("INSERT INTO TTHSources ([tthRoot], [source]) VALUES (?, ?);");

    QByteArray results;
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        //Bind parameters
        int res = 0;
        QString tthRootStr = QString(tthRoot.toBase64().data());
        res = res | sqlite3_bind_text16(statement, 1, tthRootStr.utf16(), tthRootStr.size()*2, SQLITE_STATIC);
        res = res | sqlite3_bind_text16(statement, 2, peerAddress.toString().utf16(), peerAddress.toString().size()*2, SQLITE_STATIC);

        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW);
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    //Commit to ensure access to database hasn't blocked hashing process
    commitTransaction();
}

//Load a source from a TTH
void ShareSearch::loadTTHSource(QByteArray tthRoot)
{
    //Return the source IP for a specified TTH
    QString queryStr = tr("SELECT [source] FROM TTHSources WHERE [tthRoot] = ?;");

    QString results;
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        //Bind parameters
        int res = 0;
        QString tthRootStr = QString(tthRoot.toBase64().data());
        res = res | sqlite3_bind_text16(statement, 1, tthRootStr.utf16(), tthRootStr.size(), SQLITE_STATIC);

        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            results = QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 0));
        }
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    QHostAddress ip(results);
    emit tthSourceLoaded(tthRoot, ip);
}

//Request filepath from a TTH
void ShareSearch::requestFilePath(QByteArray tthRoot)
{
    //Return the source IP for a specified TTH
    QString queryStr = tr("SELECT [filePath], [fileSize] FROM FileShares WHERE [tth] = ?;");

    QString filePath;
    quint64 fileSize;
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        //Bind parameters
        int res = 0;
        QString tthRootStr = QString(tthRoot.toBase64().data());
        res = res | sqlite3_bind_text16(statement, 1, tthRootStr.utf16(), tthRootStr.size()*2, SQLITE_STATIC);

        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            filePath = QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 0));
            fileSize = sqlite3_column_int64(statement, 1);
        }
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    // temporary short-circuit to continue debugging transfers
    //results = "/mnt/data/iso/tinycore_2.5.iso";
    emit filePathReply(tthRoot, filePath, fileSize);
}

//Release all sources for a particular TTH
void ShareSearch::deleteTTHSources(QByteArray tthRoot)
{
    //Delete all sources for a TTH
    QString queryStr = tr("DELETE FROM TTHSources WHERE [tthRoot] = ?;");

    QByteArray results;
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        //Bind parameters
        int res = 0;
        QString tthRootStr = QString(tthRoot.toBase64().data());
        res = res | sqlite3_bind_text16(statement, 1, tthRootStr.utf16(), tthRootStr.size(), SQLITE_STATIC);

        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW);
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    //Commit to ensure access to database hasn't blocked hashing process
    commitTransaction();
}

//Save state bitmap to database
void ShareSearch::saveBucketFlushStateBitmap(QByteArray tthRoot, QByteArray bitmap)
{
    //Insert a bitmap
    QString queryStr = tr("INSERT INTO FileStateBitmaps ([tthRoot], [bitmap]) VALUES (?, ?);");

    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        //Bind parameters
        int res = 0;
        QString tthRootStr = QString(tthRoot.toBase64().data());
        res = res | sqlite3_bind_text16(statement, 1, tthRootStr.utf16(), tthRootStr.size()*2, SQLITE_STATIC);
        QString bitmapStr = QString(bitmap.toBase64().data());
        res = res | sqlite3_bind_text16(statement, 2, bitmapStr.utf16(), bitmapStr.size()*2, SQLITE_STATIC);

        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW);
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    //Commit to ensure access to database hasn't blocked hashing process
    commitTransaction();
}

//Get state bitmap from database
void ShareSearch::loadBucketFlushStateBitmap(QByteArray tthRoot)
{
    //Return the bitmap for a file
    QString queryStr = tr("SELECT [bitmap] FROM FileStateBitmaps WHERE [tthRoot] = ?;");

    QByteArray bitmap;
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        //Bind parameters
        int res = 0;
        QString tthRootStr = QString(tthRoot.toBase64().data());
        res = res | sqlite3_bind_text16(statement, 1, tthRootStr.utf16(), tthRootStr.size()*2, SQLITE_STATIC);

        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            bitmap.append(QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 0)));
            bitmap = QByteArray::fromBase64(bitmap);            
        }
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    // Restore transfer state bitmap from database
    emit restoreBucketFlushStateBitmap(tthRoot, bitmap);
}

//Delete state bitmap
void ShareSearch::deleteBucketFlushStateBitmap(QByteArray tthRoot)
{
    //Delete a bitmap for a specific file (typically when done downloading)
    QString queryStr = tr("DELETE FROM FileStateBitmaps WHERE [tthRoot] = ?;");

    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        //Bind parameters
        int res = 0;
        QString tthRootStr = QString(tthRoot.toBase64().data());
        res = res | sqlite3_bind_text16(statement, 1, tthRootStr.utf16(), tthRootStr.size(), SQLITE_STATIC);

        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW);
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    //Commit to ensure access to database hasn't blocked hashing process
    commitTransaction();
}

//------------------------------============================== TTH REQUESTS FOR ALTERNATE SEARCHING ==============================------------------------------

//Request whether a file is being shared
void ShareSearch::TTHSearchQuestionReceived(QByteArray tth, QHostAddress host)
{
    //Request a small parameter from an active share entry corresponding to the given TTH
    QString queryStr = tr("SELECT [active] FROM FileShares WHERE tth = ? AND [active] = 1;");

    SearchStruct results;
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        //Bind the TTH parameter
        QString tthStr = tth.toBase64();
        int res = sqlite3_bind_text16(statement, 1, tthStr.utf16(), tthStr.size()*2, SQLITE_STATIC);

        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            //Don't care about the parameter, the fact that a row is returned shows the record exists
            //If this signal is emitted, it means the tth is shared!
            emit sendTTHSearchResult(host, tth);
            break; //Only emit once
        }
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";
}

//------------------------------============================== TTH TREE REQUEST FOR TRANSFERS ==============================------------------------------

//Request a tth tree for a file
void ShareSearch::incomingTTHTreeRequest(QHostAddress host, QByteArray tth, quint32 startBucket, quint32 bucketCount)
{
    QByteArray tthTreePacket;
    tthTreePacket.append(tth);

    //Return all 1MB TTHs for a root TTH
    QString queryStr = tr("SELECT [oneMBtth], [offset] FROM OneMBTTH WHERE [tth] = ? AND [offset] >= ? ORDER BY [offset] ASC LIMIT ?;");

    QByteArray results;
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        QString tthStr = tth.toBase64();
        int res = sqlite3_bind_text16(statement, 1, tthStr.utf16(), tthStr.size()*2, SQLITE_STATIC);
        res = res | sqlite3_bind_int64(statement, 2, (quint64)startBucket * (1<<20));
        res = res | sqlite3_bind_int(statement, 3, bucketCount);

        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            //Get 1MB TTH and its offset
            QByteArray oneMBTTH;
            oneMBTTH.append(QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 0)));
            oneMBTTH = QByteArray::fromBase64(oneMBTTH);
            qint64 offset = sqlite3_column_int64(statement, 1);

            //Assume 1MB bucket size?
            quint32 bucketNumber = offset / (1 << 20); //Thus bucketNumber 0 is the 1MB chunk between 0 and 1MB, 1 is between 1MB and 2MB etc... 
            //This number *should* divide cleanly as the TTH size is specified in HashThread.cpp as 1MB

            //Build tthTree packet structure
            //===============================================================
            //BucketNumber              quint32
            //TTH size                  quint8
            //TTH                       QByteArray (variable size = TTH size)

            tthTreePacket.append(quint32ToByteArray(bucketNumber));
            tthTreePacket.append(quint8ToByteArray((quint8)oneMBTTH.size()));
            tthTreePacket.append(oneMBTTH);

            //Send the packet if it's full
            if (tthTreePacket.size() + TTH_TREE_HASH_SIZE >= PACKET_DATA_MTU)
            {
                emit sendTTHTreeReply(host, tthTreePacket);
                //Clear packet for next data
                tthTreePacket.clear();
                tthTreePacket.append(tth);
            }
        }
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    //Send the data in the packet if everything is not sent already
    if (!tthTreePacket.isEmpty())
    {    
        emit sendTTHTreeReply(host, tthTreePacket);
    }
}

//------------------------------============================== AUTO COMPLETION WORD ENTRY ==============================------------------------------

//Request the complete word list
void ShareSearch::requestAutoCompleteWordList(QStandardItemModel *wordList)
{
    //Return the list of words from database
    QString queryStr = tr("SELECT [word] FROM AutoCompleteWords;");

    //Clear model first
    wordList->removeRows(0, wordList->rowCount());

    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            QString word = QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 0));
            
            //Add result to model
            QStandardItem *item = new QStandardItem(word);
            wordList->appendRow(item);
        }
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    wordList->sort(0);
    emit searchWordListReceived(wordList);
}

//Save a word to the database
void ShareSearch::saveAutoCompleteWordList(QString searchWord)
{
    QStringList queries;
    int count = 1;
    int totalCount = -1;

    //Get the count value for duplicates
    QString queryStr = tr("SELECT [count] FROM AutoCompleteWords WHERE [word] = ?;");
    queries.append(queryStr);

    //Save a word
    QString insertStr = tr("INSERT INTO AutoCompleteWords ([word], [count]) VALUES (?, ?);");

    //Update a word count
    QString updateStr = tr("UPDATE AutoCompleteWords SET [count] = ? WHERE [word] = ?;");

    //Count number of entries
    QString entryStr = tr("SELECT COUNT(*) FROM AutoCompleteWords;");

    //Delete the last entry
    QString deleteStr = tr("DELETE FROM AutoCompleteWords WHERE [rowID] = (SELECT MIN(rowID) FROM AutoCompleteWords);");

    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    while (!queries.isEmpty())
    {
        //Prepare a query
        QByteArray query;
        query.append(queries.takeFirst());
        if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
        {
            //Bind parameters to select query
            if (query.contains("SELECT [count]"))
            {
                int res = sqlite3_bind_text16(statement, 1, searchWord.utf16(), searchWord.size()*2, SQLITE_STATIC);
            }

            //Bind parameters to insert query
            if (query.contains("INSERT INTO"))
            {
                int res = sqlite3_bind_text16(statement, 1, searchWord.utf16(), searchWord.size()*2, SQLITE_STATIC);
                res = res | sqlite3_bind_int(statement, 2, count);
            }

            //Bind parameters to update query
            if (query.contains("UPDATE AutoCompleteWords"))
            {
                int res = sqlite3_bind_int(statement, 1, count);
                res = res | sqlite3_bind_text16(statement, 2, searchWord.utf16(), searchWord.size()*2, SQLITE_STATIC);
            }
            
            int cols = sqlite3_column_count(statement);
            while (sqlite3_step(statement) == SQLITE_ROW)
            {
                //If already in database, increase count value
                if (cols == 1 && query.contains("SELECT [count]"))
                    count = sqlite3_column_int(statement, 0) + 1;
                if (cols == 1 && query.contains("SELECT COUNT"))
                    totalCount = sqlite3_column_int(statement, 0);
                    
            }

            //If not in database, check the amount of entries
            if (count == 1 && query.contains("SELECT [count]"))
                queries.append(entryStr);
            //If already in database, update it
            else if (query.contains("SELECT [count]"))
                queries.append(updateStr);
            //If max entries have not been reached, add entry
            if (totalCount != -1 && query.contains("SELECT COUNT"))
            {
                queries.append(insertStr);
                if (totalCount > MAX_AUTOCOMPLETE_ENTRIES)
                    queries.append(deleteStr);
            }             
            sqlite3_finalize(statement);    
        }

        //Commit to ensure access to database hasn't blocked hashing process
        commitTransaction();

        //Catch all error messages
        QString error = sqlite3_errmsg(db);
        if (error != "not an error")
            QString error = "error";
    }
}

//------------------------------============================== DOWNLOAD QUEUE (DOWNLOAD QUEUE WIDGET) ==============================------------------------------

//Save a new entry
void ShareSearch::saveQueuedDownload(QueueStruct file)
{
    //Save a queued download
    QString queryStr = tr("INSERT INTO QueuedDownloads ([fileName], [filePath], [fileSize], [priority], [tthRoot], [hostIP]) VALUES (?, ?, ?, ?, ?, ?);");

    QByteArray results;
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        //Bind parameters
        int res = 0;
        res = res | sqlite3_bind_text16(statement, 1, file.fileName.utf16(), file.fileName.size()*2, SQLITE_STATIC);
        res = res | sqlite3_bind_text16(statement, 2, file.filePath.utf16(), file.filePath.size()*2, SQLITE_STATIC);
        res = res | sqlite3_bind_int64(statement, 3, file.fileSize);
        QString priority = QString((char)file.priority);
        res = res | sqlite3_bind_text16(statement, 4, priority.utf16(), priority.size()*2, SQLITE_STATIC);
        QString tthRoot = QString(file.tthRoot.toBase64().data());
        res = res | sqlite3_bind_text16(statement, 5, tthRoot.utf16(), tthRoot.size()*2, SQLITE_STATIC);
        QString hostIP = file.fileHost.toString();
        res = res | sqlite3_bind_text16(statement, 6, hostIP.utf16(), hostIP.size()*2, SQLITE_STATIC);
        
        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW);
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    //Commit to ensure access to database hasn't blocked hashing process
    commitTransaction();

    emit queuedDownloadAdded(file);
}

//Remove an entry
void ShareSearch::removeQueuedDownload(QByteArray tthRoot)
{
    //Delete an entry from the queued downloads list
    QString queryStr = tr("DELETE FROM QueuedDownloads WHERE [tthRoot] = ?;");

    QByteArray results;
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        //Bind parameters
        int res = 0;
        QString tthRootStr = QString(tthRoot.toBase64().data());
        res = res | sqlite3_bind_text16(statement, 1, tthRootStr.utf16(), tthRootStr.size()*2, SQLITE_STATIC);

        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW);
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    //Commit to ensure access to database hasn't blocked hashing process
    commitTransaction();
}

//Sets the priority of a queued download
void ShareSearch::setQueuedDownloadPriority(QByteArray tthRoot, QueuePriority priority)
{
    //Updates an entry in the queued downloads list
    QString queryStr = tr("UPDATE QueuedDownloads SET [priority] = ? WHERE [tthRoot] = ?;");

    QByteArray results;
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        //Bind parameters
        int res = 0;
        QString priorityStr = QString((char)priority);
        res = res | sqlite3_bind_text16(statement, 1, priorityStr.utf16(), priorityStr.size()*2, SQLITE_STATIC);
    
        QString tthRootStr = QString(tthRoot.toBase64().data());
        res = res | sqlite3_bind_text16(statement, 2, tthRootStr.utf16(), tthRootStr.size()*2, SQLITE_STATIC);

        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW);
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    //Commit to ensure access to database hasn't blocked hashing process
    commitTransaction();
}

//Request queued download list
void ShareSearch::requestQueueList()
{
    //Return the list of queued downloads
    QString queryStr = tr("SELECT [fileName], [filePath], [fileSize], [priority], [tthRoot], [hostIP] FROM QueuedDownloads;");

    QHash<QByteArray, QueueStruct> *results = new QHash<QByteArray, QueueStruct>();
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            QueueStruct s;
            s.fileName = QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 0));
            s.filePath = QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 1));
            s.fileSize = sqlite3_column_int64(statement, 2);
            s.priority = (QueuePriority)QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 3)).at(0).toAscii();
            //s.tthRoot = new QByteArray();
            s.tthRoot.append(QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 4)));
            s.tthRoot = QByteArray::fromBase64(s.tthRoot);
            s.fileHost = QHostAddress(QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 5)));

            results->insert(s.tthRoot, s);
        }
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    emit returnQueueList(results);
}

//------------------------------============================== FINISHED DOWNLOADS (FINISHED DOWNLOADS WIDGET) ==============================------------------------------
    
//Save an entry
void ShareSearch::saveFinishedDownload(FinishedDownloadStruct file)
{
    //Inserts a finished download into the database
    QString queryStr = tr("INSERT INTO FinishedDownloads ([fileName], [filePath], [fileSize], [tthRoot], [downloadedDate]) VALUES (?, ?, ?, ?, ?);");

    QByteArray results;
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        //Bind parameters
        int res = 0;
        res = res | sqlite3_bind_text16(statement, 1, file.fileName.utf16(), file.fileName.size()*2, SQLITE_STATIC);
        res = res | sqlite3_bind_text16(statement, 2, file.filePath.utf16(), file.filePath.size()*2, SQLITE_STATIC);
         res = res | sqlite3_bind_int64(statement, 3, file.fileSize);
        QString tthRoot = QString(file.tthRoot.toBase64().data());
        res = res | sqlite3_bind_text16(statement, 4, tthRoot.utf16(), tthRoot.size()*2, SQLITE_STATIC);
        res = res | sqlite3_bind_text16(statement, 5, file.downloadedDate.utf16(), file.downloadedDate.size()*2, SQLITE_STATIC);

        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW);
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    //Commit to ensure access to database hasn't blocked hashing process
    commitTransaction();
}

//Removes an entry
void ShareSearch::removeFinishedDownload(QByteArray tth)
{
    //Delete an entry from finished downloads
    QString queryStr = tr("DELETE FROM FinishedDownloads WHERE [tthRoot] = ?;");

    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        //Bind parameter
        QString tthStr(tth.toBase64().data());
        int res = sqlite3_bind_text16(statement, 1, tthStr.utf16(), tthStr.size()*2, SQLITE_STATIC);

        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW);
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    //Commit to ensure access to database hasn't blocked hashing process
    commitTransaction();
}

//Clear all entries
void ShareSearch::clearFinishedDownloads()
{
    //Clear finished downloads
    QString queryStr = tr("DELETE FROM FinishedDownloads;");

    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW);
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    //Commit to ensure access to database hasn't blocked hashing process
    commitTransaction();
}

//Request list
void ShareSearch::requestFinishedList()
{
    //Return the list of finished downloads
    QString queryStr = tr("SELECT [fileName], [filePath], [fileSize], [tthRoot], [downloadedDate] FROM FinishedDownloads;");

    QHash<QByteArray, FinishedDownloadStruct> *results = new QHash<QByteArray, FinishedDownloadStruct>();
    sqlite3 *db = pParent->database();    
    sqlite3_stmt *statement;

    //Prepare a query
    QByteArray query;
    query.append(queryStr);
    if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
    {
        int cols = sqlite3_column_count(statement);
        int result = 0;
        while (sqlite3_step(statement) == SQLITE_ROW)
        {
            FinishedDownloadStruct s;
            s.fileName = QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 0));
            s.filePath = QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 1));
            s.fileSize = sqlite3_column_int64(statement, 2);
            s.tthRoot.append(QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 3)));
            s.tthRoot = QByteArray::fromBase64(s.tthRoot);
            s.downloadedDate = QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 4));

            results->insert(s.tthRoot, s);
        }
        sqlite3_finalize(statement);    
    }

    //Catch all error messages
    QString error = sqlite3_errmsg(db);
    if (error != "not an error")
        QString error = "error";

    emit returnFinishedList(results);
}

//------------------------------=============================== MISC. OTHER UTILITY FUNCTIONS ===============================------------------------------

//Search history
void ShareSearch::garbageCollectSearchHistory()
{
    qint64 currentAge = QDateTime::currentMSecsSinceEpoch();
    QMutableHashIterator<QByteArray, QHash<QString, qint64> > i(searchHistoryHash);
    while (i.hasNext())
    {
        QMutableHashIterator<QString, qint64> *k = new QMutableHashIterator<QString, qint64>(i.next().value());
        while (k->hasNext())
        {
            k->next();
            if (currentAge - k->value() > 30000) //Remove all entries older than 30 seconds
                k->remove();
        }

        delete k; //Need to delete the iterator still attached to the hash that might be deleted in the following step

        if (i.value().isEmpty()) //Remove the host entry if no entries are contained in its hash
            i.remove();
    }
}

//Get the major and minor versions for a specific fileName
VersionStruct ShareSearch::getMajorMinorVersions(QString fileName)
{    
    VersionStruct v;
    v.majorVersion = -1;
    v.minorVersion = -1;

    QFileInfo fi(fileName);
    QString ext = fi.suffix();
    QString name = fi.completeBaseName();

    //Check if file is a movie file
    if (!ext.contains(QRegExp("\\b(?:avi|mpg|mkv|wmv|asf|flv|mp4|ts|mpeg)\\b",Qt::CaseInsensitive)))
        return v;

    //Get major and minor numbers
    //Regex for separated numbers i.e. s03e26 / 03e26 / 03ep26 / 3x26 / 3.26 / 3-26
    QString regex = "s?([0-9]{1,2})(?:e|ep|x|\\.|\\-)+([0-9]{1,3})";
    QRegExp rxS(regex, Qt::CaseInsensitive);
    int pos = 0;
    if (pos = rxS.indexIn(name) != -1)
    {
        //Don't match dates like 26-3-2006 / 23.3.2006
        if (!name.contains(QRegExp("[0-9]{1,2}(?:\\.|\\-)[0-9]{1,2}(?:\\.|\\-)[0-9]{2,4}")))
        {
            v.majorVersion = rxS.cap(1).toShort();
            v.minorVersion = rxS.cap(2).toShort();
        }
        else
            return v;
    }

    //Regex for normal numbers i.e. 326 / 1516 but not 15123 or 35
    regex = "(([0-1]?[0-9]{1})([0-9]{2}).?)";
    QRegExp rxN(regex, Qt::CaseInsensitive);
    if (pos = rxN.indexIn(name) != -1)
    {
        //Don't match large numbers greater than 4 characters and don't match 1080i/p and 720i/p
        QString totalMatch = rxN.cap(1);
        if (!name.contains(QRegExp("[0-9]{5,}"))  && !totalMatch.contains(QRegExp("(?:1080p|720p|1080i|720i)")))
        {
            v.majorVersion = rxN.cap(2).toShort();
            v.minorVersion = rxN.cap(3).toShort();
        }
        else
            return v;
    }
    
    //If any of the versions are zero, invalid
    if (!v.majorVersion || !v.minorVersion)
    {
        v.majorVersion = -1;
        v.minorVersion = -1;
    }
    return v;
}

//Get relative path from root directory
QString ShareSearch::getRelativePath(QString absoluteRootDir, QString absoluteFilePath)
{
    //Include the shared directory's name in the relative path
    absoluteRootDir = absoluteRootDir.left(absoluteRootDir.lastIndexOf("/"));

    return absoluteFilePath.remove(absoluteRootDir, Qt::CaseInsensitive);
}

//Get share size from DB
void ShareSearch::requestTotalShare(bool fromDB)
{
    emit returnTotalShare(totalShare(fromDB));
}

quint64 ShareSearch::totalShare(bool fromDB)
{
    if (fromDB)
        pTotalShare = getTotalShareFromDB();

    return pTotalShare;
}

void ShareSearch::setTotalShare(quint64 size)
{
    pTotalShare = size;
}
