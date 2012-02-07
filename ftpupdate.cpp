#include "ftpupdate.h"
#include "util.h"
#include "arpmanetdc.h"

FTPUpdate::FTPUpdate(QString ftpHost, QString ftpDirectory, ArpmanetDC *parent) : QObject(parent)
{
    //Constructor
    pParent = parent;
    pFtpDirectory = ftpDirectory;
    pFtpHost = ftpHost;

    downloadListIndex = -1;
    downloadGetIndex = -1;
    listIndex = -1;

    ftpServer = new QFtp(this);

    connect(ftpServer, SIGNAL(listInfo(const QUrlInfo &)), this, SLOT(listInfoReceived(const QUrlInfo &)));
    connect(ftpServer, SIGNAL(done(bool)), this, SLOT(ftpServerDone(bool)));
    connect(ftpServer, SIGNAL(commandFinished(int, bool)), this, SLOT(commandFinished(int, bool)));
    connect(ftpServer, SIGNAL(readyRead()), this, SLOT(readFromServer()));
    connect(ftpServer, SIGNAL(dataTransferProgress(qint64, qint64)), this, SIGNAL(dataTransferProgress(qint64, qint64)));
}

FTPUpdate::~FTPUpdate()
{
    //Destructor
}

void FTPUpdate::checkForUpdate()
{
    if (pFtpHost.isEmpty())
        return;

    newerVersionAvailable = false;

    //Connect to host and list contents of directory
    QUrl host(pFtpHost);
    ftpServer->connectToHost(host.host(), host.port(21));
    ftpServer->login();
    ftpServer->cd(pFtpDirectory);
    listIndex = ftpServer->list();
    ftpServer->close();
}

void FTPUpdate::listInfoReceived(const QUrlInfo &info)
{
    //Only check filenames
    if (info.isFile())
    {
        //Get version from file
        QString fileName = info.name();
        if (fileName.startsWith("ArpmanetDC"))
        {
            QString version = fileName;
            version.remove("ArpmanetDC v");
            version.remove(".exe");

            //Check if newer version
            if (firstVersionLarger(version, VERSION_STRING))
            {
                //If this is just a version check
                if (ftpServer->currentId() == listIndex)
                {
                    newestVersion = version;
                    newerVersionAvailable = true;
                }
                //If this is a download request
                else if (ftpServer->currentId() == downloadListIndex)
                {
                    //Get file
                    downloadingFileName = fileName;
                    QString currentPath = QDir::currentPath();
                    if (!currentPath.endsWith("/"))
                        currentPath.append("/");
                    file.setFileName(currentPath + downloadingFileName);
                    if (file.exists())
                        file.remove(); //Remove existing file to ensure appending works
                    file.open(QIODevice::Append);

                    downloadGetIndex = ftpServer->get(fileName);
                    ftpServer->close();
                }
            }
        }
    }
}

void FTPUpdate::downloadNewestVersion()
{
    if (pFtpHost.isEmpty())
        return;

    //Only download if no other download command is being processed
    if (ftpServer->currentId() != downloadListIndex && ftpServer->currentId() != downloadGetIndex)
    {
        //Abort if update check is in progress
        ftpServer->abort();
        ftpServer->close();

        //Connect to host and list contents of directory for download
        QUrl host(pFtpHost);
        ftpServer->connectToHost(host.host(), host.port(21));
        ftpServer->login();
        ftpServer->cd(pFtpDirectory);
        downloadListIndex = ftpServer->list();    
    }
}

void FTPUpdate::ftpServerDone(bool error)
{
    
}

void FTPUpdate::commandFinished(int index, bool error)
{
    if (index == listIndex)
    {
        if (!error)
            //Emit results if no error was received
            emit returnUpdateResults(newerVersionAvailable, newestVersion);
        listIndex = -1;
    }
    else if (index == downloadGetIndex)
    {
        file.close();

        //Determine if the file was successfully downloaded
        emit downloadCompleted(error);

        downloadGetIndex = -1;
    }
    else if (index == downloadListIndex)
    {
        downloadListIndex = -1;
    }
}

void FTPUpdate::readFromServer()
{
    while (ftpServer->bytesAvailable() > 0)
    {
        //Read and write in 1MB chunks if possible
        qint64 size = qMin(ftpServer->bytesAvailable(), (qint64)1<<20);
        char *buffer = new char[size];
            
        qint64 read = ftpServer->read(buffer, size);
        qint64 written = file.write(buffer, size);
            
        delete [] buffer;

        //Check if everything was written
        if (read != written)
            return;
    }
}

//Get/Set functions
void FTPUpdate::setFtpDirectory(QString dir)
{
    if (!dir.isEmpty())
        pFtpDirectory = dir;
}

QString FTPUpdate::ftpDirectory()
{
    return pFtpDirectory;
}