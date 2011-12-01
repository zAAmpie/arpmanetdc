#include "parsedirectorythread.h"
#include <QtGui>

//Constructor
ParseDirectoryThread::ParseDirectoryThread(QObject *parent) : QObject(parent)
{
	recursionLimit = 0;
    pStopParsing = false;
}

void ParseDirectoryThread::parseDirectory(QString dirPath)
{
    //Reset parsing to be able to stop tasks individually.
    //Comment this out to enable a single use stop for all parsing
    pStopParsing = false;

	pFileList = new QList<QString>();
	parse(QDir(dirPath));	

	//Check if recursion limit has been reached somewhere in the structure
	//if (recursionLimit < RECURSION_LIMIT)
    if (!pStopParsing)
	    emit done(dirPath, pFileList, this);
	else
		emit failed(dirPath, this);		
}

void ParseDirectoryThread::parse(QDir dir)
{
	//Ensure recursion limit hasn't been reached
	//if (recursionLimit >= RECURSION_LIMIT)
	//	return;
	
    //Stop parsing if variable is changed
    if (pStopParsing)
        return;

	//Get only files for now
	dir.setFilter(QDir::Files | QDir::NoSymLinks);

	QFileInfo fi(dir.absolutePath());
	if (fi.isFile())
	{
		//If user shared a single file, add directly to list
		pFileList->append(dir.absolutePath());
		return;
	}

	QFileInfoList list = dir.entryInfoList();
	for (int i = 0; i < list.size(); i++)
	{
		//Add every file in the current directory
		pFileList->append(list.at(i).absoluteFilePath());
	}

	//Get only directories
	dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);

	list = dir.entryInfoList();
	for (int k = 0; k < list.size(); k++)
	{
		//Iterate through all subdirectories and recursively call this function
		QDir nextDir(list.at(k).absoluteFilePath());
		if (nextDir.isReadable())
		{
            recursionLimit++;
			parse(nextDir);
		}
	}
}

void ParseDirectoryThread::stopParsing()
{
    pStopParsing = true;
}