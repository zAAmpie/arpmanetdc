#ifndef ARPMANETDC_H
#define ARPMANETDC_H

#include <QtGui/QMainWindow>
#include <QtGui>
#include <QNetworkInterface>

#include "hubconnection.h"
#include "customtableitems.h"
#include "searchwidget.h"
#include "downloadqueuewidget.h"
#include "pmwidget.h"
#include "sharewidget.h"
#include "sharesearch.h"
#include "dispatcher.h"
#include "sqlite/sqlite3.h"

#define DISPATCHER_PORT 4012

#define DEFAULT_HUB_ADDRESS "172.31.65.115"
#define DEFAULT_HUB_PORT 4012

#define DEFAULT_NICK "Bassline"
#define DEFAULT_PASSWORD "enilssab"

#define SHARE_DATABASE_PATH "arpmanetdc.sqlite"

#define MAX_MAINCHAT_LINES 1000
#define MAX_SEARCH_RESULTS 100

//Main GUI window class
class ArpmanetDC : public QMainWindow
{
	Q_OBJECT

public:
	ArpmanetDC(QWidget *parent = 0, Qt::WFlags flags = 0);
	~ArpmanetDC();

	//Get functions
	QString nick();
	QString password();

	//HTML link converters
	void convertHTMLLinks(QString &msg);
	void convertMagnetLinks(QString &msg);

	//Link to database
	sqlite3 *database() const;
	QString databasePath();

private slots:
	//Hub Connection slots
	void appendChatLine(QString msg);
	void userListInfoReceived(QString nick, QString desc, QString mode);
	void userListUserLoggedOut(QString nick);
	void userListNickListReceived(QStringList list);

	//Sort user list
	void sortUserList();

	//GUI interaction
	void sendChatMessage();

	//Direct user interaction with actions
	void searchActionPressed();
	void queueActionPressed();
	void shareActionPressed();
	void downloadFinishedPressed();
	void settingsActionPressed();
	void helpActionPressed();
	void privateMessageActionPressed();
	void reconnectActionPressed();

	//Tab clicked
	void tabDeleted(int index);
	void tabChanged(int index);

	//Right-click menus
	void showUserListContextMenu(const QPoint&);
	void showTransferListContextMenu(const QPoint&);

	//Search widget slots
	void searchButtonPressed(quint64, QString, SearchWidget *);

	//PM widget slots
	void pmSent(QString otherNick, QString msg, PMWidget *);
	void receivedPrivateMessage(QString otherNick, QString msg);

	//Share widget slots
	void shareSaveButtonPressed();

	//ShareSearch slot
	void fileHashed(QString fileName);
	void directoryParsed(QString path);
	void hashingDone(int msecs);
	void parsingDone();

signals:
	void updateShares();

private:
	//GUI setup functions
	void createWidgets();
	void placeWidgets();
	void connectWidgets();

	//SQLite setup
	bool setupDatabase();

	//Objects
	sqlite3 *db; //SQLite database
	HubConnection *pHub;
	Dispatcher *pDispatcher;
	ShareSearch *pShare;

	//Parameters
	QString pNick;
	QString pPassword;
	QString pHubIP;
	quint16 pHubPort;

	//Global lists
	QList<QueueStruct> *pQueueList;

	//===== Main GUI parameters =====

	//Determines if sorting should be done
	bool sortDue;

	//Lines in mainchat
	quint32 mainChatLines;

	//User list icons
	QPixmap *userIcon, *userFirewallIcon;

	//Actions
	QAction *reconnectAction, *shareAction, *searchAction, *queueAction, *downloadFinishedAction, *settingsAction, *helpAction, *privateMessageAction;

	//----- Widgets -----
	//Labels
	QLabel *userHubCountLabel;
	QLabel *additionalInfoLabel;
	QLabel *statusLabel;
	QLabel *shareSizeLabel;

	//Progressbar
	QProgressBar *hashingProgressBar;

	//Tab
	CTabWidget *tabs;
	QColor tabTextColorNotify;
	QColor tabTextColorNormal;

	//Bars
	QToolBar *toolBar;
	QMenuBar *menuBar;
	QStatusBar *statusBar, *infoStatusBar;

	//Chat
	QLineEdit *chatLineEdit;
	QTextBrowser *mainChatTextEdit;

	//Tables and models
	QTableView *userListTable, *transferListTable;
	QStandardItemModel *userListModel, *transferListModel;
	QSortFilterProxyModel *userSortProxy, *transferSortProxy;

	//Splitters
	QSplitter *splitterVertical, *splitterHorizontal;

	//Full page custom widgets
	QHash<QWidget *, SearchWidget *> searchWidgetHash;
	QHash<QWidget *, PMWidget *> pmWidgetHash;
	DownloadQueueWidget *downloadQueueWidget;
	ShareWidget *shareWidget;
	DownloadQueueWidget *queueWidget;

	ExecThread *dbThread;
};

#endif
