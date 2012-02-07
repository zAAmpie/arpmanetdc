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
#include "util.h"
#include <sqlite/sqlite3.h>
#include "protocoldef.h"

#define QT_NO_DEBUG_OUTPUT

#define SHARED_MEMORY_KEY "ArpmanetDCv0.1"

#define DEFAULT_EXTERNAL_PORT "4012"

#define DEFAULT_HUB_ADDRESS "arpmanet.ath.cx"
#define DEFAULT_HUB_PORT "4012"

#define DEFAULT_NICK "Testnick"
#define DEFAULT_PASSWORD "test123"

#define DEFAULT_SHARE_UPDATE_INTERVAL "3600000" //Default 60min

#define DEFAULT_SHOW_ADVANCED "0" //By default don't show advanced settings

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

#define MAX_SEARCH_RESULTS 100 //Max to give a query, not max to display
#define MAX_SIMULTANEOUS_DOWNLOADS 3 //Max to download from queue at a time

#define MAX_MAINCHAT_BLOCKS 1000
#define MAX_STATUS_HISTORY_ENTRIES 20 //Tooltip of status label

#define CONTAINER_DIRECTORY "/Containers/" //The directory in which the containers are stored

#define VERSION_STRING "0.1.5"

//Main GUI window class
class ArpmanetDC : public QMainWindow
{
	Q_OBJECT

public:
    ArpmanetDC(QStringList arguments, QWidget *parent = 0, Qt::WFlags flags = 0);
	~ArpmanetDC();

	//Get functions
	QString nick();
	QString password();
    QString downloadPath();
    QueueStruct queueEntry(QByteArray tth);

	//HTML link converters
	void convertHTMLLinks(QString &msg);
	void convertMagnetLinks(QString &msg);

	//Link to database
	sqlite3 *database() const;
	QString databasePath();

    //Guess the computer's IP
    QHostAddress getIPGuess();

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

public slots:
    //Sets the global status label in the status bar to msg
    void setStatus(QString msg);

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
    void returnProcessedContainer(QHostAddress host, ContainerContentsType index, QList<ContainerLookupReturnStruct> data, QString downloadPath);

private slots:
    //-----===== OBJECT SLOTS =====-----

	//Hub Connection slots
	void appendChatLine(QString msg);
	void userListInfoReceived(QString nick, QString desc, QString mode, QString client, QString version);
	void userListUserLoggedOut(QString nick);
	void userListNickListReceived(QStringList list);
	void hubOnline();
	void hubOffline();

    //TransferManager slots
    void downloadStarted(QByteArray tth);
    void downloadCompleted(QByteArray tth);

	//Dispatcher slots
	void bootstrapStatusChanged(int status);
    void searchResultReceived(QHostAddress senderHost, QByteArray senderCID, quint64 searchID, QByteArray searchResult);
    void returnHostCount(int count);

    //ShareSearch slot
	void fileHashed(QString fileName, quint64 fileSize);
	void directoryParsed(QString path);
	void hashingDone(int msecs, int numFiles);
	void parsingDone(int msecs);
    void searchWordListReceived(QStandardItemModel *wordList);

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

	//Tab clicked
	void tabDeleted(int index);
	void tabChanged(int index);

	//Right-click menus
	void showUserListContextMenu(const QPoint&);

    //Userlist keypresses
    void userListKeyPressed(Qt::Key key, QString keyStr);

    //-----===== SYSTEM TRAY ICON =====-----
    void systemTrayActivated(QSystemTrayIcon::ActivationReason reason);

    //-----===== SHARED MEMORY =====-----
    void checkSharedMemory();

signals:
    //Private queued signal for cross-thread comms
	void updateShares();

    //----------========== SHARESEARCH SIGNALS ==========----------

    //Signals for queues
    void saveQueuedDownload(QueueStruct item);
    void removeQueuedDownload(QByteArray tth);
    void requestQueueList();
    void setQueuedDownloadPriority(QByteArray tth, QueuePriority priority);

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

    //----------========== TRANSFERMANAGER SIGNALS ==========----------

    //Signals for queue
    void removeQueuedDownload(int priority, QByteArray tth);
    void queueDownload(int priority, QByteArray tth, QString filePath, quint64 fileSize, QHostAddress host);
    void changeQueuedDownloadPriority(int oldpriority, int newpriority, QByteArray tth);
    
    //Signals for transferWidget
    void stopTransfer(QByteArray tth, int transferType, QHostAddress hostAddr);

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
    bool saveSettings();

    //Get path for downloads
    QString getDefaultDownloadPath();

	//Objects
	sqlite3 *db; //SQLite database
	HubConnection *pHub;
	Dispatcher *pDispatcher;
    TransferManager *pTransferManager;
	ShareSearch *pShare;
    BucketFlushThread *pBucketFlushThread;
    ResourceExtractor *pTypeIconList;

    //Threads
    ExecThread *dispatcherThread;
    ExecThread *dbThread;
    ExecThread *transferThread;
    ExecThread *bucketFlushThread;

	//Parameters
	//SettingsStruct pSettings;
    QHash<QString, QString> *pSettings;

    //Containers
    QHash<QString, ContainerContentsType> pContainerHash;
    QString pContainerDirectory;

    quint64 pFilesHashedSinceUpdate, pFileSizeHashedSinceUpdate;
    QTimer *hashRateTimer;

	QTimer *updateSharesTimer;
    QTimer *updateTimer;

    QDateTime uptime;

    int arpmanetDCUsers;

    QStringList pArguments;
    QSharedMemory *pSharedMemory;

	//Global lists
	QHash<QByteArray, QueueStruct> *pQueueList;
    QHash<QByteArray, FinishedDownloadStruct> *pFinishedList;
    QList<QString> *pStatusHistoryList;
    QStandardItemModel *searchWordList;

	//-----===== Main GUI parameters =====-----

	//Determines if sorting should be done
	bool sortDue;

	//Lines in mainchat
	quint32 mainChatBlocks;

	//User list icons
	QPixmap *userIcon, *arpmanetUserIcon, *oldVersionUserIcon, *newerVersionUserIcon, *userFirewallIcon, *bootstrappedIcon, *unbootstrappedIcon, *fullyBootstrappedIcon;

    //Menus
    QMenu *userListMenu;

	//Actions
        QAction *reconnectAction, *shareAction, *searchAction, *queueAction, *downloadFinishedAction, *settingsAction, *helpAction, *privateMessageAction, *openDownloadDirAction;

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
	QLineEdit *chatLineEdit;
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
	DownloadQueueWidget *downloadQueueWidget;
	ShareWidget *shareWidget;
	DownloadQueueWidget *queueWidget;
	DownloadFinishedWidget *finishedWidget;
	SettingsWidget *settingsWidget;
    HelpWidget *helpWidget;
    TransferWidget *transferWidget;
};

#endif
