#ifndef FTPUPDATE_H
#define FTPUPDATE_H

#include <QObject>
#include <QFtp>
#include <QFile>

class ArpmanetDC;

class FTPUpdate : public QObject
{
    Q_OBJECT
public:
    FTPUpdate(QString ftpHost, QString ftpDirectory, ArpmanetDC *parent);
    ~FTPUpdate();

    void setFtpDirectory(QString dir);
    QString ftpDirectory();

public slots:
    void checkForUpdate();
    void downloadNewestVersion();

private slots:
    void ftpServerDone(bool error);
    void listInfoReceived(const QUrlInfo &);
    void commandFinished(int index, bool error);
    void readFromServer();

signals:
    void returnUpdateResults(bool result, QString version);
    void downloadCompleted(bool error);
    void dataTransferProgress(qint64 done, qint64 total);

private:
    ArpmanetDC *pParent;
    QFtp *ftpServer;

    QString pFtpDirectory;
    QString pFtpHost;

    int downloadListIndex;
    int downloadGetIndex;
    int listIndex;
    QString downloadingFileName;
    QString newestVersion;

    QFile file;

    bool newerVersionAvailable;
};

#endif