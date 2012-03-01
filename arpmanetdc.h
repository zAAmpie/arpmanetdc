/* This file is part of ArpmanetDC. Copyright (C) 2012
 * Source code can be found at http://code.google.com/p/arpmanetdc/
 * 
 * ArpmanetDC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ArpmanetDC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with ArpmanetDC.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ARPMANETDC_H
#define ARPMANETDC_H

#include <QtGui/QMainWindow>
#include <QtGui>
#include <QNetworkInterface>

#include "hubconnection.h"
#include "customtableitems.h"
#include "searchwidget.h"
#include "downloadqueuewidget.h"
#include "downloadfinishedwidget.h"
#include "displaycontainerwidget.h"
#include "pmwidget.h"
#include "settingswidget.h"
#include "sharewidget.h"
#include "helpwidget.h"
#include "transferwidget.h"
#include "sharesearch.h"
#include "dispatcher.h"
#include "transfermanager.h"
#include "bucketflushthread.h"
#include "resourceextractor.h"
#include "ftpupdate.h"
#include "util.h"
#include <sqlite/sqlite3.h>
#include "protocoldef.h"
#include "settingsmanager.h"

//#define QT_NO_DEBUG
//#define QT_NO_DEBUG_OUTPUT

#define DEFAULT_SHARE_DATABASE_PATH "arpmanetdc.sqlite"
static QString shareDatabasePath;

#define UNSUPPORTED_TRANSFER_PROTOCOLS "BTP;uTP;FECTP" //Semi-colon separated - only used to gray out protocol in settings
//#define UNSUPPORTED_TRANSFER_PROTOCOLS "BTP;FECTP"

//Initialize the protocol map
static QMap<QString, char> initMapValues() {
    QMap<QString, char> map;
    map.insert("FSTP", FailsafeTransferProtocol);
    map.insert("BTP", BasicTransferProtocol);
    map.insert("uTP", uTPProtocol);
    map.insert("FECTP", ArpmanetFECProtocol);
    return map;
}
static const QMap<QString, char> PROTOCOL_MAP = initMapValues();

#define VERSION_STRING "0.1.8"

//Main GUI window class
class ArpmanetDC : public QMainWindow
{
    Q_OBJECT

public:
    ArpmanetDC(QStringList arguments, QWidget *parent = 0, Qt::WFlags flags = 0);
    ~ArpmanetDC();

    //Settings
    static SettingsManager* settingsManager();

    //Get functions
    QueueStruct queueEntry(QByteArray tth);

    //Link and nick converters
    void convertHTMLLinks(QString &msg); //Replace links with hrefs
    void convertMagnetLinks(QString &msg); //Replace magnets with links
    void convertNickname(QString nick, QString &msg); //Replace own nickname with coloured text
    void convertOPName(QString &msg); //Replace OP nicks with coloured text
    void convertAllNicks(QString &msg); //Replace online nicknames with escaped versions

    //Replace text emoticons with images
    void insertEmoticonsInLastBlock();

    //Get nick match list
    QStringList nickMatchList(QString partialNick);

    //Windows only implementation to get Winamp song title - anything more requires a lot of work ;)
    QString getWinampSongTitle();
    
    //User commands
    QString processUserCommand(QString command);

    //Link to database
    sqlite3 *database() const;
    QString databasePath();

    //Save settings to database
    //bool saveSettings();

    //Guess the computer's IP
    QHostAddress getIPGuess();

    //Get path for downloads
    QString getDefaultDownloadPath();

    //Get the bootstrap node number
    quint32 getBootstrapNodeNumber();
    quint32 getBoostrapStatus();

    //Get access to the GUI's objects
    TransferManager *transferManagerObject() const;
    ShareSearch *shareSearchObject() const;
    Dispatcher *dispatcherObject() const;
    TransferWidget *transferWidgetObject() const;
    ResourceExtractor *resourceExtractorObject() const;

    //Used to determine if objects were created yet upon application exit
    bool createdGUI;

    QSize sizeHint() const; //reimplement sizeHint to determine initial screen size

    //User commands
    QHash<QString, UserCommandStruct> *userCommands();
    void setUserCommands(QHash<QString, UserCommandStruct> *commands);

public slots:
    //Sets the global status label in the status bar to msg
    void setStatus(QString msg);
    //Sets the additional info label to msg
    void setAdditionalInfo(QString msg);

    //-----------========== SHARE SEARCH ==========----------

    void hashingProgress(qint64 bytesThisSecond, qint64 fileProgressBytes, qint64 fileSize);

    //-----------========== QUEUED DOWNLOADS ==========----------

    //Add a download to the queue
    void addDownloadToQueue(QueueStruct item);
    //Remove download from the queue
    void deleteFromQueue(QByteArray tth);
    //Change queue item priority
    void setQueuePriority(QByteArray tth, QueuePriority priority);
    //Returns a queuelist
    void returnQueueList(QHash<QByteArray, QueueStruct> *queue);

    //-----------========== FINISHED DOWNLOADS ==========----------

    //Add a finished download to the finished list
    void addFinishedDownloadToList(FinishedDownloadStruct item);
    //Remove a finished download
    void deleteFinishedDownload(QByteArray tth);
    //Remove all downloads from the list
    void clearFinishedDownloadList();
    //Returns the finished download list
    void returnFinishedList(QHash<QByteArray, FinishedDownloadStruct> *list);

    //-----------========== TRANSFER WIDGET ==========----------
    
    //Stops a transfer
    void removeTransfer(QByteArray tth, int transferType, QHostAddress hostAddr);

    //-----------========== SHARE WIDGET - CONTAINERS ==========----------

    //Queue the containers for saving after shares have been updated
    void queueSaveContainers(QHash<QString, ContainerContentsType> containerHash, QString containerDirectory);

    //Return the contents of a container downloaded
    void returnProcessedContainer(QHostAddress host, ContainerContentsType index, QList<ContainerLookupReturnStruct> data, QString downloadPath, QString containerName);

    //Called when a file has been assembled correctly
    void fileAssemblyComplete(QString fileName);

    //-----------========== USER COMMANDS ==========----------

    //Called to set user commands
    void returnUserCommands(QHash<QString, UserCommandStruct> *commands);

private slots:
    //-----===== OBJECT SLOTS =====-----

    //Hub Connection slots
    void appendChatLine(QString msg);
    void userListInfoReceived(QString nick, QString desc, QString mode, QString client, QString version, QString registerMode, quint16 openSlots, quint64 shareBytes);
    void userListUserLoggedOut(QString nick);
    void userListNickListReceived(QStringList list);
    void opListReceived(QStringList list);
    void hubOnline();
    void hubOffline();

    //TransferManager slots
    void downloadStarted(QByteArray tth);
    void downloadCompleted(QByteArray tth);
    void closeClientEventReturn(); //Return when everything is saved
    void requeueDownload(QByteArray tth);

    //Dispatcher slots
    void bootstrapStatusChanged(int status);
    void searchResultReceived(QHostAddress senderHost, QByteArray senderCID, quint64 searchID, QByteArray searchResult);
    void returnHostCount(int count);

    //ShareSearch slot
    void fileHashed(QString fileName, quint64 fileSize, quint64 totalShare);
    void directoryParsed(QString path);
    void hashingDone(int msecs, int numFiles);
    void parsingDone(int msecs);
    void searchWordListReceived(QStandardItemModel *wordList);
    void returnTotalShare(quint64 size);

    //FTP Update slots
    void ftpReturnUpdateResults(bool result, QString newVersion);
    void ftpDownloadCompleted(bool error);
    void ftpDataTransferProgress(qint64 done, qint64 total);

    //-----===== CUSTOM WIDGET SLOTS =====-----

    //Search widget slots
    void searchButtonPressed(quint64 id, QString searchStr,  QByteArray searchPacket, SearchWidget *widget);

    //PM widget slots
    void pmSent(QString otherNick, QString msg, PMWidget *);
    void receivedPrivateMessage(QString otherNick, QString msg);

    //Share widget slots
    void shareSaveButtonPressed();

    //Settings widget slots
    void settingsSaved();

    //-----===== GUI SLOTS =====-----

    //Sort user list
    void sortUserList();

    //Update GUI slot
    void updateGUIEverySecond();

    //Calculate rates
    void calculateHashRate();

    //GUI interaction
    void mainChatLinkClicked(const QUrl &link);
    void chatLineEditReturnPressed();
    void sendChatMessage();
    void quickSearchPressed();

    //Direct user interaction with actions
    void searchActionPressed();
    void queueActionPressed();
    void shareActionPressed();
    void downloadFinishedActionPressed();
    void settingsActionPressed();
    void helpActionPressed();
    void privateMessageActionPressed();
    void reconnectActionPressed();
    void openDownloadDirActionPressed();
    void openContainerActionPressed();
    void pmActionPressed();

    void userCommandMenuPressed(QAction *action);

    //Tab clicked
    void tabDeleted(int index);
    void tabChanged(int index);

    //Right-click menus
    void showUserListContextMenu(const QPoint &pos);
    void showMainChatContextMenu(const QPoint &pos);

    //Userlist keypresses
    void userListKeyPressed(Qt::Key key, QString keyStr);
    
    //Chat line edit keypresses
    void chatLineEditKeyPressed(Qt::Key key, QString keyStr);

    //-----===== SYSTEM TRAY ICON =====-----
    void systemTrayActivated(QSystemTrayIcon::ActivationReason reason);

    //-----===== SHARED MEMORY =====-----
    void checkSharedMemory();

signals:
    //Private queued signal for cross-thread comms
    void updateShares();

    //Private signals for FTP updating
    void ftpCheckForUpdate();
    void ftpDownloadNewestVersion();

    //Get user commands
    void requestUserCommands();

    //----------========== SHARESEARCH SIGNALS ==========----------

    //Signals for queues
    void saveQueuedDownload(QueueStruct item);
    void removeQueuedDownload(QByteArray tth);
    void requestQueueList();
    void setQueuedDownloadPriority(QByteArray tth, QueuePriority priority);

    //Signal for share size
    void requestTotalShare(bool fromDB = false);

    //Signals for finished downloads
    void clearFinishedDownloads();
    void saveFinishedDownload(FinishedDownloadStruct item);
    void removeFinishedDownload(QByteArray tth);
    void requestFinishedList();

    //Auto complete searches from database
    void requestAutoCompleteWordList(QStandardItemModel *wordList);
    void saveAutoCompleteWordList(QString word);

    //Save the containers to files in the directory specified
    void saveContainers(QHash<QString, ContainerContentsType> containerHash, QString containerDirectory);
    //Process downloaded container
    void processContainer(QHostAddress host, QString containerPath, QString downloadPath);

    //Delete a state bitmap on download completion
    void deleteBucketFlushStateBitmap(QByteArray tthRoot);

    //----------========== TRANSFERMANAGER SIGNALS ==========----------

    //Signals for queue
    void removeQueuedDownload(int priority, QByteArray tth);
    void queueDownload(int priority, QByteArray tth, QString filePath, quint64 fileSize, QHostAddress host);
    void changeQueuedDownloadPriority(int oldpriority, int newpriority, QByteArray tth);

    //Signals for transferWidget
    void stopTransfer(QByteArray tth, int transferType, QHostAddress hostAddr);

    //Signal for close event to save bitmaps
    void closeClientEvent();

    //----------========== DISPATCHER SIGNALS ==========----------

    //Initiate search
    void initiateSearch(quint64 id, QByteArray searchPacket);
    //Signals for host count
    void getHostCount();

protected:
    void changeEvent(QEvent *e);
    void closeEvent(QCloseEvent *e);

private:
    //GUI setup functions
    void createWidgets();
    void placeWidgets();
    void connectWidgets();

    //SQLite setup
    bool setupDatabase();

    //Load settings from database
    bool loadSettings();
    
    //Calculate total share size
    quint64 calculateTotalShareSize();

    //Used to determine if an icon change should be done for notifications
    void updateNotify(int count);

    //Load emoticons into icon database
    void loadEmoticonsFromFile();

    //Objects
    sqlite3 *db; //SQLite database
    HubConnection *pHub;
    Dispatcher *pDispatcher;
    TransferManager *pTransferManager;
    ShareSearch *pShare;
    BucketFlushThread *pBucketFlushThread;
    ResourceExtractor *pTypeIconList;
    ResourceExtractor *pEmoticonResourceList;
    FTPUpdate *pFtpUpdate;
    static SettingsManager *pSettingsManager;

    //Threads
    ExecThread *dispatcherThread;
    ExecThread *dbThread;
    ExecThread *transferThread;
    ExecThread *bucketFlushThread;

    //Parameters
    //SettingsStruct pSettings;
    //QHash<QString, QString> *pSettings;
    QHash<QString, UserCommandStruct> *pUserCommands;

    //Containers
    QHash<QString, ContainerContentsType> pContainerHash;
    QHash<QString, QueueStruct> pContainerProcessHash;
    QSet<QString> pContainerAssembledHash;
    QString pContainerDirectory;

    //Hub connection - userlist and OP list
    QSet<QString> pOPList;
    QHash<QString, QPair<QStandardItem *, quint64> > pUserList;
    QSet<QString> pADCUserList;

    quint32 notifyCount; //Used to keep track of the amount of PM windows that has new messages to change icon

    bool saveSharesPressed;

    quint64 pFilesHashedSinceUpdate, pFileSizeHashedSinceUpdate;
    QTimer *hashRateTimer;

    QTimer *updateSharesTimer;
    QTimer *updateTimer;

    QTimer *checkForFTPUpdatesTimer;

    QDateTime uptime;

    int arpmanetDCUsers;

    QStringList pArguments;
    QSharedMemory *pSharedMemory;

    //Event parameter to save when a closeEvent is called to pass to QMainWindow (this can also serve to check for duplicate events)
    QCloseEvent *pCloseEvent;

    //Global lists
    QHash<QByteArray, QueueStruct> *pQueueList;
    QHash<QByteArray, FinishedDownloadStruct> *pFinishedList;
    QList<QString> *pStatusHistoryList;
    QList<QString> *pAdditionalInfoHistoryList;
    QStandardItemModel *searchWordList;

    QStringList pEmoticonList;
    QImage pEmoticon;

    QStringList pCurrentMatchList;
    QString pCurrentMatchChatString;
    int tabCyclingIndex;

    //-----===== Main GUI parameters =====-----

    //Determines if sorting should be done
    bool sortDue;

    //Lines in mainchat
    quint32 mainChatBlocks;

    //User list icons
    QPixmap *userIcon, *arpmanetUserIcon, *oldVersionUserIcon, *newerVersionUserIcon, *userFirewallIcon, *bootstrappedIcon, *unbootstrappedIcon, *fullyBootstrappedIcon;
    
    //Application icons
    QPixmap *arpmanetDCLogoNormal, *arpmanetDCLogoNotify;

    //Menus
    QMenu *userListMenu;
    QMenu *userCommandListMenu;

    //Actions
    QAction *reconnectAction, *shareAction, *searchAction, *queueAction, *downloadFinishedAction, *settingsAction, *helpAction, *privateMessageAction, *openDownloadDirAction, *pmAction, *openContainerAction;

    //-----===== System tray =====-----

    QSystemTrayIcon *systemTrayIcon;
    QMenu *systemTrayMenu;
    QAction *restoreAction, *quitAction;
    QSize windowSize;
    bool wasMaximized;

    //-----===== Widgets =====-----

    //Quicksearch
    QLineEdit *quickSearchLineEdit;
    QCompleter *searchCompleter;

    //Labels
    QLabel *userHubCountLabel;
    QLabel *additionalInfoLabel;
    QLabel *statusLabel;
    QLabel *shareSizeLabel;
    QLabel *connectionIconLabel;
    QLabel *bootstrapStatusLabel;
    QLabel *CIDHostsLabel;
    QLabel *totalShareSizeLabel;

    //Progressbar
    TextProgressBar *hashingProgressBar;

    //Tab
    CTabWidget *tabs;
    QColor tabTextColorNotify;
    QColor tabTextColorNormal;
    QColor tabTextColorOffline;

    //Bars
    QToolBar *toolBar;
    QMenuBar *menuBar;
    QStatusBar *statusBar, *infoStatusBar;

    //Chat
    KeyLineEdit *chatLineEdit;
    QTextBrowser *mainChatTextEdit;

    //Tables and models
    QTableView *userListTable;
    QStandardItemModel *userListModel;
    QSortFilterProxyModel *userSortProxy;

    //Splitters
    QSplitter *splitterVertical, *splitterHorizontal;

    //Full page custom widgets
    QHash<QWidget *, SearchWidget *> searchWidgetHash;
    QHash<quint64, SearchWidget *> searchWidgetIDHash;
    QHash<QWidget *, PMWidget *> pmWidgetHash;
    QHash<QWidget *, DisplayContainerWidget *> displayContainerWidgetHash;
    DownloadQueueWidget *downloadQueueWidget;
    ShareWidget *shareWidget;
    DownloadQueueWidget *queueWidget;
    DownloadFinishedWidget *finishedWidget;
    SettingsWidget *settingsWidget;
    HelpWidget *helpWidget;
    TransferWidget *transferWidget;
};

#endif
