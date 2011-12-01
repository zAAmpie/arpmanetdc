#ifndef PARSEDIRECTORYTHREAD_H
#define PARSEDIRECTORYTHREAD_H

#include <QObject>
#include <QDir>
#include <QList>

#define RECURSION_LIMIT 100 //Ensure the parsing only reaches 100 levels deep

class ParseDirectoryThread : public QObject
{
	Q_OBJECT
public:
	//Constructor
	ParseDirectoryThread(QObject *parent = 0);

public slots:
	//Called by another thread to parse a directory
	void parseDirectory(QString directoryPath);

    //Stop parsing
    void stopParsing();

signals:
	//Done parsing directory
	void done(QString rootDir, QList<QString> *fileList, ParseDirectoryThread *parseObj);
	//Failed parsing directory
	void failed(QString rootDir, ParseDirectoryThread *parseObj);

private:
	//Recursive private function
    void parse(QDir directory);

	QList<QString> *pFileList;
	QDir pDir;

    bool pStopParsing;

	quint8 recursionLimit;
};

#endif
