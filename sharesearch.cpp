#include "sharesearch.h"
#include "arpmanetdc.h"

ShareSearch::ShareSearch(quint32 maxSearchResults, ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;
	pMaxResults = maxSearchResults;

	pFileList = new QList<FileListStruct>();
	pTotalShare = 0;

	//Commit transactions every minute
	commitTimer = new QTimer(this);
	connect(commitTimer, SIGNAL(timeout()), this, SLOT(commitTransaction()));
	commitTimer->setInterval(60000);

	updateTime = new QTime();

	//Create a new thread
	hashThread = new ExecThread();
	
	pHashFileThread = new HashFileThread();
	connect(pHashFileThread, SIGNAL(done(QString, QString, qint64, QString, QString, QString, QList<QString> *, HashFileThread *)),
		this, SLOT(hashFileThreadDone(QString, QString, qint64, QString, QString, QString, QList<QString> *, HashFileThread *)), Qt::QueuedConnection);
	connect(pHashFileThread, SIGNAL(failed(QString, HashFileThread *)), this, SLOT(hashFileFailed(QString, HashFileThread *)), Qt::QueuedConnection);
	connect(this, SIGNAL(runHashThread(QString, QString)), pHashFileThread, SLOT(processFile(QString, QString)), Qt::QueuedConnection);
	pHashFileThread->moveToThread(hashThread);

	pParseDirectoryThread = new ParseDirectoryThread();
	connect(pParseDirectoryThread, SIGNAL(done(QString, QList<QString> *, ParseDirectoryThread *)), 
		this, SLOT(parseDirectoryThreadDone(QString, QList<QString> *, ParseDirectoryThread *)), Qt::QueuedConnection);
	connect(pParseDirectoryThread, SIGNAL(failed(QString, ParseDirectoryThread *)), this, SLOT(parseDirectoryThreadFailed(QString, ParseDirectoryThread *)), Qt::QueuedConnection);
	connect(this, SIGNAL(runParseThread(QString)), pParseDirectoryThread, SLOT(parseDirectory(QString)), Qt::QueuedConnection);
	pParseDirectoryThread->moveToThread(hashThread);

	hashThread->start();
}

ShareSearch::~ShareSearch()
{
	//Destructor
	delete pDirList;
	delete pFileList;
	hashThread->exit(0);
	if (hashThread->wait())
		delete hashThread;
}

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

void ShareSearch::updateShares(QList<QDir> *dirList) //500 msecs to update SharePaths!
{
	updateTime->start();

	pDirList = dirList;
	if (dirList->isEmpty())
		return;

	//Construct a directory list insert query
	QList<QByteArray> queries;

	queries.append("BEGIN;");
	transactionInProgress = true;
	commitTimer->start();

	//Set all files as inactive
	setAllFilesInactive();

	//Delete everything from the sharePath table
	QByteArray removeQuery;
	removeQuery.append(tr("DELETE FROM SharePaths WHERE [path] NOT IN ("));	

	//Insert the directory and file listing
	for (int k = 0; k < dirList->size(); k++)
	{
		if (k != 0)
			removeQuery.append(",");
		removeQuery.append(tr("?"));//.arg(QByteArray().append(dirList->at(k).absolutePath()).toBase64().data()));

		QByteArray query;
		query.append(tr("INSERT INTO SharePaths ([path]) SELECT ?;"));//.arg(QByteArray().append(dirList->at(k).absolutePath()).toBase64().data()));
		queries.append(query);

	}
	removeQuery.append(");");
	queries.append(removeQuery);

	//Write list to database
	sqlite3 *db = pParent->database();	
	sqlite3_stmt *statement;
	
	int count = 0;
	for (int i = 0; i < queries.size(); i++)
	{
		if (sqlite3_prepare_v2(db, queries.at(i).data(), -1, &statement, 0) == SQLITE_OK)
		{
			if (queries.at(i).contains("DELETE FROM SharePaths"))
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

	//Start parsing
	startDirectoryParsing();
}

QList<QDir> *ShareSearch::getShares()
{
	//Query whether a file has been modified - returns results if it has
	QString queryStr = tr("SELECT [path] FROM SharePaths;");

	QList<QString> results;
	sqlite3 *db = pParent->database();	
	sqlite3_stmt *statement;

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
					//results.append(QByteArray::fromBase64(QByteArray((char *)sqlite3_column_text(statement, col))));
					results.append(QString::fromUtf16((const unsigned short *)sqlite3_column_text16(statement, col)));
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

	//Report results
	QList<QDir> *shares = new QList<QDir>();
	while (!results.isEmpty())
		shares->append(QDir(results.takeFirst()));

	return shares;
}

void ShareSearch::updateShares()
{
	//Convenience function to update existing shares
	updateShares(getShares());
}

//Hash file thread completed
void ShareSearch::hashFileThreadDone(QString filePath, QString fileName, qint64 fileSize, QString tthRoot, QString rootDir, QString lastModified, QList<QString> *oneMBList, HashFileThread *hashObj)
{
	QString done = "done";
	QList<QString> queries;

	//Generate fileShare query to insert data into database
	QString fileShareQuery = tr("INSERT INTO FileShares ([tth], [fileName], [fileSize], [filePath], [lastModified], [shareDirID], [active], [majorVersion], [minorVersion], [relativePath]) VALUES (?, ?, ?, ?, ?, (SELECT [rowID] FROM SharePaths WHERE path = ?), 1, ?, ?, ?);");
		/*.arg(tthRoot)
		//.arg("?")
		//.arg(QByteArray().append(fileName).toBase64().data())
		.arg("?")
		.arg(fileSize)
		//.arg(QByteArray().append(filePath).toBase64().data())
		.arg("?")
		.arg(lastModified)
		//.arg(QByteArray().append(rootDir).toBase64().data())
		.arg("?")
		//.arg(filePath.replace("'","''"))
		.arg("?"); //Test bound params*/

	//Add query to list
	queries.append(fileShareQuery);

	//Generate 1MB tth query - max blocks of 50 selects
	int count = 0;
	qint64 offset = 0;
	while (!oneMBList->isEmpty())
	{
		QByteArray query;
		query.append(tr("INSERT INTO OneMBTTH ([oneMBtth], [tth], [offset], [fileShareID]) SELECT '%1', '%2', %3, (SELECT rowID FROM FileShares WHERE filePath = ?001) ")
			.arg(oneMBList->takeFirst())
			.arg(tthRoot)
			.arg(offset));

		while ((count < 50) && (!oneMBList->isEmpty()))
		{
			query.append(tr("UNION SELECT '%1', '%2', %3, (SELECT rowID FROM FileShares WHERE filePath = ?001) ")
				.arg(oneMBList->takeFirst())
				.arg(tthRoot)
				.arg(offset+=1048576));
			
			count++;
		}
		query.append(";");	
		count = 0;

		//Add query to list
		queries.append(query);
	}	

	//Generate totalFileShares query
	QString totalFileShares = tr("SELECT SUM(fileSize) FROM FileShares WHERE [active] = 1;");
	queries.append(totalFileShares);

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
			if (query.contains("INSERT INTO FileShares")) 
			{
				int num = sqlite3_bind_parameter_count(statement);
				int res = 0;
				res = res | sqlite3_bind_text16(statement, 1, tthRoot.utf16(), tthRoot.size()*2, SQLITE_STATIC);
				res = res | sqlite3_bind_text16(statement, 2, fileName.utf16(), fileName.size()*2, SQLITE_STATIC);
				res = res | sqlite3_bind_int64(statement, 3, fileSize);
				res = res | sqlite3_bind_text16(statement, 4, filePath.utf16(), filePath.size()*2, SQLITE_STATIC);
				res = res | sqlite3_bind_text16(statement, 5, lastModified.utf16(), lastModified.size()*2, SQLITE_STATIC);
				res = res | sqlite3_bind_text16(statement, 6, rootDir.utf16(), rootDir.size()*2, SQLITE_STATIC);
                res = res | sqlite3_bind_int64(statement, 7, v.majorVersion);
                res = res | sqlite3_bind_int64(statement, 8, v.minorVersion);
                res = res | sqlite3_bind_text16(statement, 9, relativePath.utf16(), relativePath.size()*2, SQLITE_STATIC);
				if (res == SQLITE_OK)
					QString awesome = "awesome";
				else
					QString lessAwesome = tr("%1").arg(res);
			}
			if (query.contains("INSERT INTO OneMBTTH")) 
			{
				int num = sqlite3_bind_parameter_count(statement);
				int res = sqlite3_bind_text16(statement, 1, filePath.utf16(), filePath.size()*2, SQLITE_STATIC);
				if (res == SQLITE_OK)
					QString awesome = "awesome";
				else
					QString lessAwesome = tr("%1").arg(res);
			}


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
	pTotalShare = totalShare;

	//Emit signal for status
	emit fileHashed(filePath);
	
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

void ShareSearch::parseDirectoryThreadDone(QString rootDir, QList<QString> *fileList, ParseDirectoryThread *parseObj)
{
	QString done = "done";
	//Add filelist to local variable for hashing
	foreach (QString file, *fileList)
	{
		FileListStruct fStruct;
		fStruct.fileName = file;
		fStruct.rootDir = rootDir;
		pFileList->append(fStruct);
	}
	
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
		startParseDirectoryThread(pDirList->takeFirst());
	else
	{		
		emit parsingDone();

		//Start file hashing on file list
		startFileHashing();
	}
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
		emit hashingDone(totalUpdateTime);
	}
}

bool ShareSearch::fileNotModified(QString filePath, QString rootDir)
{
	QTime *time = new QTime();
	time->start();

	//Get current modified date of file
	QFileInfo fileInfo(filePath);
	QString modifiedDate;
	bool fileExists = fileInfo.exists();

	QList<QByteArray> queries;

	//Check if file exists
	if (fileExists)
	{
		modifiedDate = fileInfo.lastModified().toString("dd-MM-yyyy HH:mm:ss:zzz");
	
		//Query whether a file has been modified - returns results if it has
		QString queryStr = tr("SELECT [filePath] FROM FileShares WHERE lastModified = ?001 AND filePath = ?002;");
			//.arg(modifiedDate)
			//.arg(QByteArray().append(filePath).toBase64().data());

		queries.append(QByteArray().append(queryStr));

		//Update the file share entry - change the sharepath owner of the entry if needed
		QString updateStr = tr("UPDATE fileShares SET [shareDirID] = (SELECT rowID FROM SharePaths WHERE path = ?001) WHERE filePath = (SELECT [filePath] FROM FileShares WHERE lastModified = ?002 AND filePath = ?003);");
			//.arg(QByteArray().append(rootDir).toBase64().data())
			//.arg(modifiedDate)
			//.arg(QByteArray().append(filePath).toBase64().data());

		queries.append(QByteArray().append(updateStr));

		//Set the file share active (if readding an existing share
		QString setActiveStr = tr("UPDATE fileShares SET [active] = 1 WHERE filePath = ?001;");
			//.arg(QByteArray().append(filePath).toBase64().data());

		queries.append(QByteArray().append(setActiveStr));

	}
	else
	{
		//Delete the hash entry from the database if it exists
		QString queryStr = tr("DELETE FROM FileShares WHERE filePath = ?001;");
			//.arg(QByteArray().append(filePath).toBase64().data());

		queries.append(QByteArray().append(queryStr));
	}

        QList<QList<QString> > results;
	sqlite3 *db = pParent->database();	
	sqlite3_stmt *statement;

	//Prepare a query
	for (int i = 0; i < queries.size(); i++)
	{
		int res = sqlite3_prepare_v2(db, queries.at(i).data(), -1, &statement, 0);
		if (res == SQLITE_OK)
		{
			if (queries.at(i).contains("SELECT [filePath]"))
			{
				int res = 0;
				res = res | sqlite3_bind_text16(statement, 1, modifiedDate.utf16(), modifiedDate.size()*2, SQLITE_STATIC);
				res = res | sqlite3_bind_text16(statement, 2, filePath.utf16(), filePath.size()*2, SQLITE_STATIC);
				
				if (res != SQLITE_OK)
					QString error = "error";
			}

			if (queries.at(i).contains("UPDATE fileShares SET [shareDirID]"))
			{
				int res = 0;
				res = res | sqlite3_bind_text16(statement, 1, rootDir.utf16(), rootDir.size()*2, SQLITE_STATIC);
				res = res | sqlite3_bind_text16(statement, 2, modifiedDate.utf16(), modifiedDate.size()*2, SQLITE_STATIC);
				res = res | sqlite3_bind_text16(statement, 3, filePath.utf16(), filePath.size()*2, SQLITE_STATIC);
				
				if (res != SQLITE_OK)
					QString error = "error";
			}

			if (queries.at(i).contains("UPDATE fileShares SET [active]"))
			{
				int res = 0;
				res = res | sqlite3_bind_text16(statement, 1, filePath.utf16(), filePath.size()*2, SQLITE_STATIC);
				
				if (res != SQLITE_OK)
					QString error = "error";
			}

			if (queries.at(i).contains("DELETE FROM FileShares"))
			{
				int res = 0;
				res = res | sqlite3_bind_text16(statement, 1, filePath.utf16(), filePath.size()*2, SQLITE_STATIC);
				
				if (res != SQLITE_OK)
					QString error = "error";
			}

			int cols = sqlite3_column_count(statement);
			int result = 0;
			while (true)
			{
				//Step through query results
				result = sqlite3_step(statement);

				//If result is a row, add to results
				if (result == SQLITE_ROW)
				{
					QList<QString> oneResult;
					for (int col = 0; col < cols; col++)
					{
						//oneResult.append(QByteArray::fromBase64(QByteArray((char *)sqlite3_column_text(statement, col))));
						oneResult.append(QString::fromUtf16((const unsigned short *)sqlite3_column_text16(statement, col)));
					}		
					results.append(oneResult);
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
	}

	int elapsed = time->elapsed();
	QString finished;	

	//Test results
	if (results.size() > 0 && fileExists)
	{
		QString filePathResult = results.first().first();
		if (filePathResult == filePath)
			//File hasn't been modified
			return true; //Skip hashing
		else
		//Somehow the paths do not match? Maybe due to some weird character in the filename? Base64 encoded they are the same though... weird
		{
			return true; //Skip hashing
		}
	}
	
	if (fileExists) //File has been modified or doesn't exist in the database
		return false; //Hash file
	return true; //Skip hashing - obviously if the file doesn't exist
}

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

//Query search string
void ShareSearch::querySearchString(qint16 majorVersion, qint16 minorVersion, QString searchStr)
{
	//Split string into words
	QStringList wordList = searchStr.split(" ");

	//Query the database with the search string
	QString queryStr = tr("SELECT [tth], [fileName], [fileSize], [relativePath] FROM FileShares WHERE ");

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

	queryStr.append(tr(" LIMIT %1;").arg(MAX_SEARCH_RESULTS));

	QList<ShareStruct> *results = new QList<ShareStruct>();
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
			if (cols = 3)
			{
				ShareStruct s;
				QByteArray tthRoot = QByteArray().append((char*)sqlite3_column_text(statement, 0));
				s.tthRoot = QByteArray::fromBase64(tthRoot);
				s.fileName = QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 1));
				s.fileSize = sqlite3_column_int64(statement, 2);
                s.relativePath = QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 3));
                s.majorVersion = majorVersion;
                s.minorVersion = minorVersion;
				results->append(s);
			}				
		}
		sqlite3_finalize(statement);	
	}

	//Catch all error messages
	QString error = sqlite3_errmsg(db);
	if (error != "not an error")
		QString error = "error";

	//Report results
	emit returnSearchResults(results);	
}

//Return a struct of a file for a given TTH root
void ShareSearch::queryTTH(QByteArray tthRoot)
{
	//Query the database with the search string
	QString queryStr = tr("SELECT DISTINCT [tth], [fileName], [fileSize] FROM FileShares WHERE tth = '%1';").arg(QString(tthRoot.toBase64()));

	ShareStruct results;
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

//Slots for transfers
//Save a source for a particular TTH
void ShareSearch::saveTTHSource(QByteArray *tthRoot, QHostAddress *peerAddress)
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
		QString tthRootStr = QString(tthRoot->data());
		res = res | sqlite3_bind_text16(statement, 1, tthRootStr.utf16(), tthRootStr.size(), SQLITE_STATIC);
		res = res | sqlite3_bind_text16(statement, 2, peerAddress->toString().utf16(), peerAddress->toString().size()*2, SQLITE_STATIC);

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

//Load a source from a TTH
void ShareSearch::loadTTHSource(QByteArray *tthRoot)
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
		QString tthRootStr = QString(tthRoot->data());
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

	QHostAddress *ip = new QHostAddress(results);
	emit tthSourceLoaded(tthRoot, ip);
}

//Request filepath from a TTH
void ShareSearch::requestFilePath(QByteArray *tthRoot)
{
	//Return the source IP for a specified TTH
	QString queryStr = tr("SELECT [filePath] FROM FileShares WHERE [tth] = ?;");

	QString *results = new QString();
	sqlite3 *db = pParent->database();	
	sqlite3_stmt *statement;

	//Prepare a query
	QByteArray query;
	query.append(queryStr);
	if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
	{
		//Bind parameters
		int res = 0;
		QString tthRootStr = QString(tthRoot->data());
		res = res | sqlite3_bind_text16(statement, 1, tthRootStr.utf16(), tthRootStr.size(), SQLITE_STATIC);

		int cols = sqlite3_column_count(statement);
		int result = 0;
		while (sqlite3_step(statement) == SQLITE_ROW)
		{
			*results = QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 0));
		}
		sqlite3_finalize(statement);	
	}

	//Catch all error messages
	QString error = sqlite3_errmsg(db);
	if (error != "not an error")
		QString error = "error";

	emit filePathReply(tthRoot, results);
}

//Release all sources for a particular TTH
void ShareSearch::deleteTTHSources(QByteArray *tthRoot)
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
		QString tthRootStr = QString(tthRoot->data());
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
}

//===== DOWNLOAD QUEUE =====
//Save a new entry
void ShareSearch::saveQueuedDownload(QueueStruct file)
{
	//Save a queued download
	QString queryStr = tr("INSERT INTO QueuedDownloads ([fileName], [filePath], [fileSize], [priority], [tthRoot]) VALUES (?, ?, ?, ?, ?);");

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
		QString tthRoot = QString(file.tthRoot->data());
		res = res | sqlite3_bind_text16(statement, 5, tthRoot.utf16(), tthRoot.size()*2, SQLITE_STATIC);

		int cols = sqlite3_column_count(statement);
		int result = 0;
		while (sqlite3_step(statement) == SQLITE_ROW);
		sqlite3_finalize(statement);	
	}

	//Catch all error messages
	QString error = sqlite3_errmsg(db);
	if (error != "not an error")
		QString error = "error";

	emit queuedDownloadAdded(file);
}

//Remove an entry
void ShareSearch::removeQueuedDownload(QByteArray *tthRoot)
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
		QString tthRootStr = QString(tthRoot->data());
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
}

//Sets the priority of a queued download
void ShareSearch::setQueuedDownloadPriority(QByteArray *tthRoot, QueuePriority priority)
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
	
		QString tthRootStr = QString(tthRoot->data());
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
}

//Request queued download list
void ShareSearch::requestQueueList()
{
	//Return the list of queued downloads
	QString queryStr = tr("SELECT [fileName], [filePath], [fileSize], [priority], [tthRoot] FROM QueuedDownloads;");

	QList<QueueStruct> *results = new QList<QueueStruct>();
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
			s.tthRoot = new QByteArray();
			s.tthRoot->append(QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 4)));

			results->append(s);
		}
		sqlite3_finalize(statement);	
	}

	//Catch all error messages
	QString error = sqlite3_errmsg(db);
	if (error != "not an error")
		QString error = "error";

	emit returnQueueList(results);
}

//===== FINISHED DOWNLOADS =====
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
		QString tthRoot = QString(file.tthRoot->data());
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

	emit finishedDownloadAdded(file);
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
}

//Request list
void ShareSearch::requestFinishedList()
{
	//Return the list of finished downloads
	QString queryStr = tr("SELECT [fileName], [filePath], [fileSize], [tthRoot], [downloadDate] FROM FinishedDownloads;");

	QList<FinishedDownloadStruct> *results = new QList<FinishedDownloadStruct>();
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
			s.tthRoot = new QByteArray();
			s.tthRoot->append(QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 3)));
			s.downloadedDate = QString::fromUtf16((const unsigned short*)sqlite3_column_text16(statement, 4));

			results->append(s);
		}
		sqlite3_finalize(statement);	
	}

	//Catch all error messages
	QString error = sqlite3_errmsg(db);
	if (error != "not an error")
		QString error = "error";

	emit returnFinishedList(results);
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
    if (!ext.contains(QRegExp("\\b(?:avi|mpg|mkv|wmv|asf|flv)\\b",Qt::CaseInsensitive)))
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

quint64 ShareSearch::totalShare(bool fromDB)
{
	if (fromDB)
		return getTotalShareFromDB();
	return pTotalShare;
}

QString ShareSearch::totalShareStr(bool fromDB)
{
	double share;
	if (fromDB)
		share = getTotalShareFromDB();
	else
		share = pTotalShare;
	QString unit = "bytes";

	if (share > 1024.0)
	{
		share /= 1024.0;
		unit = "KiB";
		if (share > 1024.0)
		{
			share /= 1024.0;
			unit = "MiB";
			if (share > 1024.0)
			{
				share /= 1024.0;
				unit = "GiB";
				if (share > 1024.0)
				{
					share /= 1024.0;
					unit = "TiB";
				}
			}
		}
	}

	return tr("%1 %2").arg(share, 0, 'f', 2).arg(unit);
}

void ShareSearch::setTotalShare(quint64 size)
{
	pTotalShare = size;
}
