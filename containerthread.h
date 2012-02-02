#ifndef CONTAINERTHREAD_H
#define CONTAINERTHREAD_H

#include <QObject>
#include <QHash>
#include <QPair>
#include <QFileInfo>
#include <QStringList>

#define CONTAINER_EXTENSION "adcc"

#define HEADER_LENGTH 8

typedef QPair<quint64, QHash<QString, quint64> > ContainerContentsType;

struct ContainerLookupReturnStruct
{
    QString filePath;
    QByteArray rootTTH;
    quint64 fileSize;
};

class ContainerThread : public QObject
{
    Q_OBJECT
public:
    ContainerThread(QObject *parent = 0);
    ~ContainerThread();

public slots:
    //Request all containers in a directory
    void requestContainers(QString containerDirectory);
    //Save the containers to files in the directory specified
    void saveContainers(QHash<QString, ContainerContentsType> containerHash, QString containerDirectory);
    //Return hashes from DB
    void returnTTHsFromPaths(QHash<QString, QList<ContainerLookupReturnStruct> > results, QString containerPath);

signals:
    //Return the containers requested
    void returnContainers(QHash<QString, ContainerContentsType> containerHash);
    //Request hashes from a list of filepaths from DB
    void requestTTHsFromPaths(QHash<QString, QStringList> filePaths, QString containerPath);

private:
    //Process a file
    ContainerContentsType processContainerFileIndex(QFileInfo fileInfo);
    //Write a file
    bool writeContainerFileIndex(QString name, ContainerContentsType contents);
};

#endif