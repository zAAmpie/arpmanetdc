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

    pRootDir = dirPath;

    pFileList = new QList<FileListStruct>();
    //Old method to parse directories
    //parse(QDir(dirPath));    

    //Experimental new method to parse directory - much faster
    FileListStruct f;
    f.rootDir = pRootDir;

    QFileInfo fi(dirPath);
    if (fi.isFile())
    {
        f.fileName = dirPath;
        pFileList->append(f);
    }
    else
    {
        QDirIterator it(dirPath, QDir::Files | QDir::NoSymLinks | QDir::Readable, QDirIterator::Subdirectories);  
        while (it.hasNext())
        {
            it.next();
            
            f.fileName = it.filePath(); 
            pFileList->append(f);
        }
    }

    //Check if recursion limit has been reached somewhere in the structure
    //if (recursionLimit < RECURSION_LIMIT)
    if (!pStopParsing)
        emit done(pRootDir, pFileList, this);
    else
        emit failed(pRootDir, this);        
}

void ParseDirectoryThread::parse(QDir dir)
{
    //Ensure recursion limit hasn't been reached
    //if (recursionLimit >= RECURSION_LIMIT)
    //    return;
    
    //Stop parsing if variable is changed
    if (pStopParsing)
        return;

    //Get only files for now
    dir.setFilter(QDir::Files | QDir::NoSymLinks);

    QFileInfo fi(dir.path());
    if (fi.isFile())
    {
        //If user shared a single file, add directly to list
        FileListStruct f;
        f.rootDir = pRootDir;
        f.fileName = dir.path();
        pFileList->append(f);
        return;
    }

    QFileInfoList list = dir.entryInfoList();
    for (int i = 0; i < list.size(); i++)
    {
        //Add every file in the current directory
        FileListStruct f;
        f.rootDir = pRootDir;
        f.fileName = list.at(i).filePath();
        pFileList->append(f);
        //pFileList->append(list.at(i).absoluteFilePath());
    }

    //Get only directories
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);

    list = dir.entryInfoList();
    for (int k = 0; k < list.size(); k++)
    {
        //Iterate through all subdirectories and recursively call this function
        QDir nextDir(list.at(k).filePath());
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