#include "parsedirectorythread.h"

//Constructor
ParseDirectoryThread::ParseDirectoryThread(QObject *parent) : QObject(parent)
{

}

void ParseDirectoryThread::parseDirectory(QString dirPath)
{
	pFileList = new QList<QString>();
	parse(QDir(dirPath));	

	emit done(dirPath, pFileList, this);
}

void ParseDirectoryThread::parse(QDir dir)
{
	//Get only files for now
	dir.setFilter(QDir::Files | QDir::NoSymLinks);

	QFileInfo fi(dir.absolutePath());
	if (fi.isFile())
	{
		pFileList->append(dir.absolutePath());
		return;
	}

	QFileInfoList list = dir.entryInfoList();
	for (int i = 0; i < list.size(); i++)
	{
		pFileList->append(list.at(i).absoluteFilePath());
	}

	dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);

	list = dir.entryInfoList();
	for (int k = 0; k < list.size(); k++)
	{
		QDir nextDir(list.at(k).absoluteFilePath());
		if (nextDir.isReadable())
			parse(nextDir);
	}
}
