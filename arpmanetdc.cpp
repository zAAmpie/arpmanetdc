#include "arpmanetdc.h"

ArpmanetDC::ArpmanetDC(QWidget *parent, Qt::WFlags flags)
	: QMainWindow(parent, flags)
{
	//QApplication::setStyle(new QCleanlooksStyle());

    //Register QHostAddress for queueing over threads
    qRegisterMetaType<QHostAddress>("QHostAddress");
    qRegisterMetaType<QueueStruct>("QueueStruct");
    qRegisterMetaType<QueuePriority>("QueuePriority");

    //Set database pointer to zero at start
	db = 0;

    setupDatabase();
    pSettings = new QHash<QString, QString>();

    //Load settings from database or initialize settings from defaults
   	if (!loadSettings())
    {
        pSettings->insert("nick", DEFAULT_NICK);
        pSettings->insert("password", DEFAULT_PASSWORD);
        pSettings->insert("hubAddress", DEFAULT_HUB_ADDRESS);
        pSettings->insert("hubPort", DEFAULT_HUB_PORT);
        pSettings->insert("externalIP", getIPGuess().toString());
        pSettings->insert("externalPort", DEFAULT_EXTERNAL_PORT);
        pSettings->insert("downloadPath", getDefaultDownloadPath());
    }

	mainChatBlocks = 0;

	//Set window title
	setWindowTitle(tr("ArpmanetDC %1").arg(VERSION_STRING));

	//Create Hub Connection
	pHub = new HubConnection(pSettings->value("hubAddress"), pSettings->value("hubPort").toShort(), pSettings->value("nick"), pSettings->value("password"), VERSION_STRING, this);

    //Connect HubConnection to GUI
	connect(pHub, SIGNAL(receivedChatMessage(QString)), this, SLOT(appendChatLine(QString)));
	connect(pHub, SIGNAL(receivedMyINFO(QString, QString, QString)), this, SLOT(userListInfoReceived(QString, QString, QString)));
	connect(pHub, SIGNAL(receivedNickList(QStringList)), this, SLOT(userListNickListReceived(QStringList)));
	connect(pHub, SIGNAL(userLoggedOut(QString)), this, SLOT(userListUserLoggedOut(QString)));	
	connect(pHub, SIGNAL(receivedPrivateMessage(QString, QString)), this, SLOT(receivedPrivateMessage(QString, QString)));
	connect(pHub, SIGNAL(hubOnline()), this, SLOT(hubOnline()));
	connect(pHub, SIGNAL(hubOffline()), this, SLOT(hubOffline()));

    //pHub->connectHub();

    //Create Dispatcher connection
	pDispatcher = new Dispatcher(QHostAddress(pSettings->value("externalIP")), pSettings->value("externalPort").toShort());

    // conjure up something unique here and save it for every subsequent client invocation
    //Maybe make a SHA1 hash of the Nick + Password - unique and consistent? Except if you have two clients open with the same login details? Meh
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(QByteArray().append(pSettings->value("nick")));
    hash.addData(QByteArray().append(pSettings->value("password")));
    QByteArray cid = hash.result();

    //QByteArray cid = "012345678901234567890123";
    pDispatcher->setCID(cid);

    //Connect Dispatcher to GUI - handle search replies from other clients
	connect(pDispatcher, SIGNAL(bootstrapStatusChanged(int)), this, SLOT(bootstrapStatusChanged(int)));
    connect(pDispatcher, SIGNAL(searchResultsReceived(QHostAddress, QByteArray, quint64, QByteArray)),
            this, SLOT(searchResultReceived(QHostAddress, QByteArray, quint64, QByteArray)));

    // Create Transfer manager
    pTransferManager = new TransferManager();
    pTransferManager->setMaximumSimultaneousDownloads(30);

    //Connect Dispatcher to TransferManager - handles upload/download requests and transfers
    connect(pDispatcher, SIGNAL(incomingUploadRequest(QByteArray,QHostAddress,QByteArray,quint64,quint64)),
            pTransferManager, SLOT(incomingUploadRequest(QByteArray,QHostAddress,QByteArray,quint64,quint64)));
    connect(pDispatcher, SIGNAL(incomingDataPacket(quint8,QByteArray&)),
            pTransferManager, SLOT(incomingDataPacket(quint8,QByteArray&)));
    connect(pTransferManager, SIGNAL(transmitDatagram(QHostAddress&,QByteArray&)),
            pDispatcher, SLOT(sendUnicastRawDatagram(QHostAddress&,QByteArray&)));
    connect(pDispatcher, SIGNAL(receivedTTHTree(QByteArray,QByteArray)),
            pTransferManager, SLOT(incomingTTHTree(QByteArray,QByteArray)));
    connect(pTransferManager, SIGNAL(TTHTreeRequest(QHostAddress&,QByteArray&)),
            pDispatcher, SLOT(sendTTHTreeRequest(QHostAddress&,QByteArray&)));
    connect(pDispatcher, SIGNAL(TTHSearchResultsReceived(QByteArray,QHostAddress)),
            pTransferManager, SLOT(incomingTTHSource(QByteArray,QHostAddress)));
    connect(pTransferManager, SIGNAL(searchTTHAlternateSources(QByteArray&)),
            pDispatcher, SLOT(initiateTTHSearch(QByteArray&)));

    // Set network scan ranges in Dispatcher, initial shotgun approach
    pDispatcher->addNetworkScanRange(QHostAddress("143.160.0.1").toIPv4Address(), 65534);
    pDispatcher->addNetworkScanRange(QHostAddress("172.31.0.1").toIPv4Address(), 65534);

	//Set up thread for database / ShareSearch
	dbThread = new ExecThread();
	pShare = new ShareSearch(MAX_SEARCH_RESULTS, this);

    //Connect ShareSearch to GUI - share files on this computer and hash them
	connect(pShare, SIGNAL(fileHashed(QString)), this, SLOT(fileHashed(QString)), Qt::QueuedConnection);
	connect(pShare, SIGNAL(directoryParsed(QString)), this, SLOT(directoryParsed(QString)), Qt::QueuedConnection);
	connect(pShare, SIGNAL(hashingDone(int, int)), this, SLOT(hashingDone(int, int)), Qt::QueuedConnection);
	connect(pShare, SIGNAL(parsingDone()), this, SLOT(parsingDone()), Qt::QueuedConnection);
	connect(pShare, SIGNAL(returnQueueList(QHash<QByteArray, QueueStruct> *)), this, SLOT(returnQueueList(QHash<QByteArray, QueueStruct> *)), Qt::QueuedConnection);
    connect(this, SIGNAL(updateShares()), pShare, SLOT(updateShares()), Qt::QueuedConnection);
    connect(this, SIGNAL(requestQueueList()), pShare, SLOT(requestQueueList()), Qt::QueuedConnection);
    connect(this, SIGNAL(setQueuedDownloadPriority(QByteArray, QueuePriority)), pShare, SLOT(setQueuedDownloadPriority(QByteArray, QueuePriority)), Qt::QueuedConnection);
    connect(this, SIGNAL(removeQueuedDownload(QByteArray)), pShare, SLOT(removeQueuedDownload(QByteArray)), Qt::QueuedConnection);
    connect(this, SIGNAL(saveQueuedDownload(QueueStruct)), pShare, SLOT(saveQueuedDownload(QueueStruct)), Qt::QueuedConnection);
    emit requestQueueList();    

    //Connect ShareSearch to Dispatcher - reply to search request from other clients
    connect(pShare, SIGNAL(returnSearchResult(QHostAddress, QByteArray, quint64, QByteArray)), 
            pDispatcher, SLOT(sendSearchResult(QHostAddress, QByteArray, quint64, QByteArray)), Qt::QueuedConnection);
    connect(pDispatcher, SIGNAL(searchQuestionReceived(QHostAddress, QByteArray, quint64, QByteArray)), 
            pShare, SLOT(querySearchString(QHostAddress, QByteArray, quint64, QByteArray)), Qt::QueuedConnection);

    //Connect ShareSearch to TransferManager - loads and saves a set of sources to the database
    connect(pTransferManager, SIGNAL(filePathNameRequest(QByteArray)), pShare, SLOT(requestFilePath(QByteArray)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(saveTTHSourceToDatabase(QByteArray, QHostAddress)), pShare, SLOT(saveTTHSource(QByteArray, QHostAddress)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(loadTTHSourcesFromDatabase(QByteArray)), pShare, SLOT(loadTTHSource(QByteArray)), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(deleteTTHSourcesFromDatabase(QByteArray)), pShare, SLOT(deleteTTHSources(QByteArray)), Qt::QueuedConnection);
    connect(pShare, SIGNAL(filePathReply(QByteArray, QString)), pTransferManager, SLOT(filePathNameReply(QByteArray, QString)), Qt::QueuedConnection);
    connect(pShare, SIGNAL(tthSourceLoaded(QByteArray, QHostAddress)), pTransferManager, SLOT(incomingTTHSource(QByteArray, QHostAddress)), Qt::QueuedConnection);
    
    //Temporary signal to search local database

    connect(pShare, SIGNAL(returnSearchResult(QHostAddress,QByteArray,quint64,QByteArray)),
            this, SLOT(searchResultReceived(QHostAddress,QByteArray,quint64,QByteArray)));

	pShare->moveToThread(dbThread);
	dbThread->start();

	//GUI setup
	createWidgets();
	placeWidgets();
	connectWidgets();	

    pStatusHistoryList = new QList<QString>();
    pQueueList = new QHash<QByteArray, QueueStruct>();

    //Update shares
	emit updateShares();
    setStatus(tr("Share update procedure started. Parsing directories/paths..."));

	downloadQueueWidget = 0;
	shareWidget = 0;
	queueWidget = 0;
	finishedWidget = 0;
	settingsWidget = 0;
    helpWidget = 0;
    transferWidget = 0;

	//Icon generation
	userIcon = new QPixmap();
	*userIcon = QPixmap(":/ArpmanetDC/Resources/UserIcon.png").scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	userFirewallIcon = new QPixmap();
	*userFirewallIcon = QPixmap(":/ArpmanetDC/Resources/FirewallIcon.png").scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    fullyBootstrappedIcon = new QPixmap();
	*fullyBootstrappedIcon = QPixmap(":/ArpmanetDC/Resources/ServerOnlineIcon.png").scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	bootstrappedIcon = new QPixmap();
	*bootstrappedIcon = QPixmap(":/ArpmanetDC/Resources/ServerIcon.png").scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	unbootstrappedIcon = new QPixmap();
	*unbootstrappedIcon = QPixmap(":/ArpmanetDC/Resources/ServerOfflineIcon.png").scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    //Init type icon list with resource extractor class
    pTypeIconList = new ResourceExtractor();

	//Set up timer to ensure each and every update doesn't signal a complete list sort!
	sortDue = false;
	QTimer *sortTimer = new QTimer();
	connect(sortTimer, SIGNAL(timeout()), this, SLOT(sortUserList()));
	sortTimer->start(1000); //Meh, every second ought to do it
}

ArpmanetDC::~ArpmanetDC()
{
	//Destructor
	delete pHub;
    delete pTransferManager;
    delete pDispatcher;

    //saveSettings();
    delete pSettings;

	dbThread->quit();
	if (dbThread->wait(5000))
		delete dbThread;
    else
    {
        dbThread->terminate();
        delete dbThread;
    }
	sqlite3_close(db);
	
}

bool ArpmanetDC::setupDatabase()
{
	//Create database/open existing database
	if (sqlite3_open(SHARE_DATABASE_PATH, &db))
	{
		QString error("Can't open database");
		sqlite3_close(db);
		return false;
	}

	//Check if tables exist and create them otherwise
	sqlite3_stmt *statement;

	QList<QString> queries;

	//Set full synchronicity
	queries.append("PRAGMA synchronous = FULL;");

    //Commit any outstanding queries
    queries.append("COMMIT;");

	//Create FileShares table - list of all files hashed
	queries.append("CREATE TABLE FileShares (rowID INTEGER PRIMARY KEY, tth TEXT, fileName TEXT, fileSize INTEGER, filePath TEXT, lastModified TEXT, shareDirID INTEGER, active INTEGER, majorVersion INTEGER, minorVersion INTEGER, relativePath TEXT, FOREIGN KEY(shareDirID) REFERENCES SharePaths(rowID), UNIQUE(filePath));");
	queries.append("CREATE INDEX IDX_FILESHARES_FILENAME on FileShares(fileName);");
    queries.append("CREATE INDEX IDX_FILESHARES_FILEPATH on FileShares(filePath);");
    queries.append("CREATE INDEX IDX_FILESHARES_FILESIZE on FileShares(fileSize);");
    queries.append("CREATE INDEX IDX_FILESHARES_ACTIVE on FileShares(active);");

	//Create 1MB TTH table - list of the 1MB bucket TTHs for every fileshare
	queries.append("CREATE TABLE OneMBTTH (rowID INTEGER PRIMARY KEY, oneMBtth TEXT, tth TEXT, offset INTEGER, fileShareID INTEGER, FOREIGN KEY(fileShareID) REFERENCES FileShares(rowID));");
	queries.append("CREATE INDEX IDX_TTH on OneMBTTH(tth);");

	//Create SharePaths table - list of all the folders/files chosen in ShareWidget
	queries.append("CREATE TABLE SharePaths (rowID INTEGER PRIMARY KEY, path TEXT, UNIQUE(path));");
    queries.append("CREATE INDEX IDX_SHAREPATHS_PATH on SharePaths(path);");

	//Create TTHSources table - list of all sources for a transfer
	queries.append("CREATE TABLE TTHSources (rowID INTEGER PRIMARY KEY, tthRoot TEXT, source TEXT, UNIQUE(tthRoot, source));");
    queries.append("CREATE INDEX IDX_TTHSOURCES_TTHROOT on TTHSources(tthRoot);");
	
	//Create QueuedDownloads table - saves all queued downloads to restart transfers after restart
	queries.append("CREATE TABLE QueuedDownloads (rowID INTEGER PRIMARY KEY, fileName TEXT, filePath TEXT, fileSize INTEGER, priority INTEGER, tthRoot TEXT, UNIQUE(tthRoot));");
    queries.append("CREATE INDEX IDX_QUEUED_FILEPATH on QueuedDownloads(filePath);");

	//Create FinishedDownloads table - saves download paths that were downloaded for list
	queries.append("CREATE TABLE FinishedDownloads (rowID INTEGER PRIMARY KEY, fileName TEXT, filePath TEXT, fileSize INTEGER, tthRoot TEXT, downloadedDate TEXT);");
    queries.append("CREATE INDEX IDX_FINISHED_FILEPATH on FinishedDownloads(filePath);");

    //Create Settings table - saves settings (doh)
    queries.append("CREATE TABLE Settings (rowID INTEGER PRIMARY KEY, parameter TEXT, value TEXT);");

	QList<QString> queryErrors(queries);

	//Loop through all queries
	for (int i = 0; i < queries.size(); i++)
	{
		//Prepare a query
		QByteArray query;
		query.append(queries.at(i).toUtf8());
		if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
		{
			int cols = sqlite3_column_count(statement);
			int result = 0;
			while (sqlite3_step(statement) == SQLITE_ROW);
			sqlite3_finalize(statement);	
		}

		//Catch all error messages
		QString error = sqlite3_errmsg(db);
		if (error != "not an error")
			queryErrors[i] = error;
	}

	return true;
}

bool ArpmanetDC::loadSettings()
{
    QList<QString> queries;
    pSettings->clear();
    
     //Create Settings table - saves settings (doh)
    queries.append("SELECT [parameter], [value]  FROM Settings;");

    sqlite3_stmt *statement;
	QList<QString> queryErrors(queries);

	//Loop through all queries
	for (int i = 0; i < queries.size(); i++)
	{
		//Prepare a query
		QByteArray query;
		query.append(queries.at(i).toUtf8());
		if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
		{
			int cols = sqlite3_column_count(statement);
			int result = 0;
			while (sqlite3_step(statement) == SQLITE_ROW)
            {
                //Load settings
                if (cols == 2)
                {
                    QString parameter = QString::fromUtf16((const unsigned short *)sqlite3_column_text16(statement, 0));
                    QString value = QString::fromUtf16((const unsigned short *)sqlite3_column_text16(statement, 1));
                    if (!parameter.isEmpty())
                        pSettings->insert(parameter, value);
                }
            };
			sqlite3_finalize(statement);	
		}

		//Catch all error messages
		QString error = sqlite3_errmsg(db);
		if (error != "not an error")
			queryErrors[i] = error;
	}

	return !pSettings->isEmpty();
}

bool ArpmanetDC::saveSettings()
{
    QList<QString> queries;
    //queries.append("BEGIN;");
    queries.append("DELETE FROM Settings;");
    
    QList<QString> parameters = pSettings->keys();
    for (int i = 0; i < pSettings->size(); i++)
        queries.append("INSERT INTO Settings([parameter], [value]) VALUES (?, ?);");

    queries.append("COMMIT;");
    queries.append("BEGIN;");

    bool result = true;
	QList<QString> queryErrors(queries);
    sqlite3_stmt *statement;

	//Loop through all queries
	for (int i = 0; i < queries.size(); i++)
	{
		//Prepare a query
		QByteArray query;
		query.append(queries.at(i).toUtf8());
		if (sqlite3_prepare_v2(db, query.data(), -1, &statement, 0) == SQLITE_OK)
		{
            if (query.contains("INSERT INTO Settings") && !parameters.isEmpty())
            {
                int res = 0;
                QString parameter = parameters.takeFirst();
                QString value = pSettings->value(parameter);
		        res = res | sqlite3_bind_text16(statement, 1, parameter.utf16(), parameter.size()*2, SQLITE_STATIC);
                res = res | sqlite3_bind_text16(statement, 2, value.utf16(), value.size()*2, SQLITE_STATIC);
                if (res != SQLITE_OK)
                    result = false;
            }

			int cols = sqlite3_column_count(statement);
			int result = 0;
			while (sqlite3_step(statement) == SQLITE_ROW);
			sqlite3_finalize(statement);	
		}

		//Catch all error messages
		QString error = sqlite3_errmsg(db);
		if (error != "not an error")
			queryErrors[i] = error;
	}

	return result;
}

void ArpmanetDC::createWidgets()
{
	//Labels
	userHubCountLabel = new QLabel(tr("Hub Users"));
	additionalInfoLabel = new QLabel(tr("Additional Info"));

	statusLabel = new QLabel(tr("Status"));
	shareSizeLabel = new QLabel(tr("Share: 0 bytes"));

	bootstrapStatusLabel = new QLabel();
	//connectionIconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	connectionIconLabel = new QLabel();
	//connectionIconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	//Progress bar
	hashingProgressBar = new QProgressBar(this);
	hashingProgressBar->setRange(0,0);
	hashingProgressBar->setStyle(new QPlastiqueStyle());
	hashingProgressBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);

	//Splitters
	splitterVertical = new QSplitter(this);
	splitterHorizontal = new QSplitter(Qt::Vertical, this);
	
	//Tab widget
	tabs = new CTabWidget(this);
	tabs->setIconSize(QSize(16,16));
	tabs->setTabsClosable(true);
	tabTextColorNotify = QColor(Qt::red);
	
	//Chat
	mainChatTextEdit = new QTextBrowser(this);
	mainChatTextEdit->setOpenExternalLinks(true);
	
	chatLineEdit = new QLineEdit(this);
	
	//===== User list =====
	//Model
	userListModel = new QStandardItemModel(0,5);
	userListModel->setHeaderData(0, Qt::Horizontal, tr("Nickname"));
	userListModel->setHeaderData(1, Qt::Horizontal, tr("Description"));
	userListModel->setHeaderData(2, Qt::Horizontal, tr("Nickname"));
	userListModel->setHeaderData(3, Qt::Horizontal, tr("Description"));
	userListModel->setHeaderData(4, Qt::Horizontal, tr("Mode"));

	userSortProxy = new QSortFilterProxyModel(this);
	userSortProxy->setSortCaseSensitivity(Qt::CaseInsensitive);
	userSortProxy->setSourceModel(userListModel);
	
	//Table
	userListTable = new QTableView(this);
	userListTable->setContextMenuPolicy(Qt::CustomContextMenu);
	
	//Link table and model
	userListTable->setModel(userSortProxy);

	//Sizing
	userListTable->horizontalHeader()->setResizeMode(QHeaderView::Interactive);
	userListTable->setColumnWidth(0,150);
	userListTable->setColumnWidth(1,250);
	userListTable->horizontalHeader()->setMinimumSectionSize(150);
	
	//Style
	userListTable->setShowGrid(true);
	userListTable->setGridStyle(Qt::DotLine);
	userListTable->verticalHeader()->hide();
	userListTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	userListTable->setItemDelegate(new HTMLDelegate(userListTable));

	userListTable->hideColumn(2);
	userListTable->hideColumn(3);
	userListTable->hideColumn(4);

    //TransferWidget
    transferWidget = new TransferWidget(pTransferManager, this);

	//===== Bars =====
	//Toolbar
	toolBar = new QToolBar(tr("Actions"), this);
	//toolBar->setToolButtonStyle(Qt::ToolButtonFollowStyle);
	toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	//toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	toolBar->setMovable(false);

	//Menu bar
	menuBar = new QMenuBar(this);

	//Status bars
	statusBar = new QStatusBar(this);
	statusBar->addPermanentWidget(statusLabel,1);
	statusBar->addPermanentWidget(connectionIconLabel);
	statusBar->addPermanentWidget(bootstrapStatusLabel);
	statusBar->addPermanentWidget(hashingProgressBar);
	statusBar->addPermanentWidget(shareSizeLabel);

	infoStatusBar = new QStatusBar(this);
	infoStatusBar->addPermanentWidget(additionalInfoLabel,1);
	infoStatusBar->addPermanentWidget(userHubCountLabel);
	infoStatusBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
	infoStatusBar->setSizeGripEnabled(false);
	
	//===== Actions =====
	queueAction = new QAction(QIcon(":/ArpmanetDC/Resources/QueueIcon.png"), tr("Download queue"), this);
	downloadFinishedAction = new QAction(QIcon(":/ArpmanetDC/Resources/DownloadFinishedIcon.png"), tr("Finished downloads"), this);
	searchAction = new QAction(QIcon(":/ArpmanetDC/Resources/SearchIcon.png"), tr("Search"), this);
	shareAction = new QAction(QIcon(":/ArpmanetDC/Resources/ShareIcon.png"), tr("Share"), this);
	settingsAction = new QAction(QIcon(":/ArpmanetDC/Resources/SettingsIcon.png"), tr("Settings"), this);
	helpAction = new QAction(QIcon(":/ArpmanetDC/Resources/HelpIcon.png"), tr("Help"), this);
	privateMessageAction = new QAction(QIcon(":/ArpmanetDC/Resources/PMIcon.png"), tr("Send private message"), this);
	reconnectAction = new QAction(QIcon(":/ArpmanetDC/Resources/ServerIcon.png"), tr("Reconnect"), this);

    //===== Menus =====
    userListMenu = new QMenu(this);
	userListMenu->addAction(privateMessageAction);
}

void ArpmanetDC::placeWidgets()
{
	//Add actions to toolbar
	toolBar->addAction(searchAction);
	toolBar->addAction(shareAction);
	toolBar->addAction(reconnectAction);
	toolBar->addSeparator();
	toolBar->addAction(queueAction);
	toolBar->addAction(downloadFinishedAction);
	toolBar->addSeparator();
	toolBar->addAction(settingsAction);
	toolBar->addAction(helpAction);
	toolBar->addSeparator();

	//Add search widget on toolbar
	QWidget *quickSearchWidget = new QWidget();
	QHBoxLayout *searchLayout = new QHBoxLayout;
	searchLayout->addStretch(1);

	QVBoxLayout *searchVertLayout = new QVBoxLayout;
	searchVertLayout->addStretch(1);
	searchVertLayout->addWidget(new QLineEdit());
	searchVertLayout->addStretch(1);
	searchLayout->addLayout(searchVertLayout);
	searchLayout->setContentsMargins(0,0,0,0);
	quickSearchWidget->setLayout(searchLayout);

	toolBar->addWidget(quickSearchWidget);

	//Place splitter
	splitterVertical->addWidget(mainChatTextEdit);
	splitterVertical->addWidget(userListTable);

	QList<int> sizes;
	sizes << 600 << 200;
	splitterVertical->setSizes(sizes);
	
	//Place vertical widgets
	QGridLayout *layout = new QGridLayout;
	layout->addWidget(splitterVertical,0,0);
	layout->addWidget(chatLineEdit,1,0);
	layout->addWidget(infoStatusBar,2,0);
	layout->setContentsMargins(0,0,0,0);

	QWidget *layoutWidget = new QWidget(this);
	layoutWidget->setLayout(layout);

	//Place main tab widget
	QWidget *tabWidget = new QWidget();
	QVBoxLayout *tabLayout = new QVBoxLayout();
	tabLayout->setContentsMargins(0,0,0,0);
	tabLayout->setSpacing(0);
	tabLayout->addWidget(layoutWidget);
	tabWidget->setLayout(tabLayout);
	
	tabs->addTab(tabWidget, QIcon(":/ArpmanetDC/Resources/ServerOfflineIcon.png"), tr("Arpmanet Chat"));
	tabs->setTabPosition(QTabWidget::North);
	tabTextColorNormal = tabs->tabBar()->tabTextColor(tabs->indexOf(tabWidget));

	//Place horizontal widgets
	splitterHorizontal->addWidget(tabs);
	splitterHorizontal->addWidget(transferWidget->widget());
	
	sizes.clear();
	sizes << 400 << 200;
	splitterHorizontal->setSizes(sizes);

	//Set MainWindow paramters
	setMenuBar(menuBar);
	addToolBar(toolBar);
	setStatusBar(statusBar);
	setIconSize(QSize(64,64));	
	setCentralWidget(splitterHorizontal);	
	setMinimumSize(800,600);
}

void ArpmanetDC::connectWidgets()
{
	//Context menu
	connect(userListTable, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showUserListContextMenu(const QPoint&)));

	//Send chat message when enter is pressed
    connect(chatLineEdit, SIGNAL(returnPressed()), this, SLOT(chatLineEditReturnPressed()));
	
	//Connect actions
	connect(queueAction, SIGNAL(triggered()), this, SLOT(queueActionPressed()));
	connect(shareAction, SIGNAL(triggered()), this, SLOT(shareActionPressed()));
	connect(searchAction, SIGNAL(triggered()), this, SLOT(searchActionPressed()));
	connect(downloadFinishedAction, SIGNAL(triggered()), this, SLOT(downloadFinishedActionPressed()));
	connect(settingsAction, SIGNAL(triggered()), this, SLOT(settingsActionPressed()));
	connect(helpAction, SIGNAL(triggered()), this, SLOT(helpActionPressed()));
	connect(privateMessageAction, SIGNAL(triggered()), this, SLOT(privateMessageActionPressed()));
	connect(reconnectAction, SIGNAL(triggered()), this, SLOT(reconnectActionPressed()));

	//Tab widget
	connect(tabs, SIGNAL(tabCloseRequested(int)), this, SLOT(tabDeleted(int)));
	connect(tabs, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));
}

void ArpmanetDC::chatLineEditReturnPressed()
{
    if (chatLineEdit->text().compare("+debugbuckets") == 0)
    {
        appendChatLine("DEBUG Bucket contents");
        appendChatLine(pDispatcher->getDebugBucketsContents());
        chatLineEdit->setText("");
    }
    else if (chatLineEdit->text().compare("+debugcidhosts") == 0)
    {
        appendChatLine("DEBUG CID Host contents");
        appendChatLine(pDispatcher->getDebugCIDHostContents());
        chatLineEdit->setText("");
    }
    else
        sendChatMessage();

}

void ArpmanetDC::sendChatMessage()
{
	//Send the chat message in the line
	if (!chatLineEdit->text().isEmpty())
	{
		pHub->sendChatMessage(chatLineEdit->text());
		chatLineEdit->setText("");
	}
}

void ArpmanetDC::searchActionPressed()
{
	SearchWidget *sWidget = new SearchWidget(pTypeIconList, pTransferManager, this);
	
    connect(sWidget, SIGNAL(search(quint64, QString, QByteArray, SearchWidget *)), this, SLOT(searchButtonPressed(quint64, QString, QByteArray, SearchWidget *)));

	searchWidgetHash.insert(sWidget->widget(), sWidget);
    searchWidgetIDHash.insert(sWidget->id(), sWidget);

	tabs->addTab(sWidget->widget(), QIcon(":/ArpmanetDC/Resources/SearchIcon.png"), tr("Search"));

	tabs->setCurrentIndex(tabs->indexOf(sWidget->widget()));
}

void ArpmanetDC::queueActionPressed()
{
	//Check if queue widget exists already
	if (queueWidget)
	{
		//If it does, select it and return
		if (tabs->indexOf(queueWidget->widget()) != -1)
		{
			tabs->setCurrentIndex(tabs->indexOf(queueWidget->widget()));
			return;
		}
	}

	//Otherwise, create it
	queueWidget = new DownloadQueueWidget(pQueueList, pShare, this);
	
	tabs->addTab(queueWidget->widget(), QIcon(":/ArpmanetDC/Resources/QueueIcon.png"), tr("Download Queue"));

	tabs->setCurrentIndex(tabs->indexOf(queueWidget->widget()));
}

void ArpmanetDC::downloadFinishedActionPressed()
{
	//Check if widget exists already
	if (finishedWidget)
	{
		//If it does, select it and return
		if (tabs->indexOf(finishedWidget->widget()) != -1)
		{
			tabs->setCurrentIndex(tabs->indexOf(finishedWidget->widget()));
			return;
		}
	}

	//Otherwise, create it
	finishedWidget = new DownloadFinishedWidget(this);

	connect(finishedWidget, SIGNAL(requestFinishedList()), pShare, SLOT(requestFinishedList()));
	connect(pShare, SIGNAL(returnFinishedList(QList<FinishedDownloadStruct> *)), finishedWidget, SLOT(returnFinishedList(QList<FinishedDownloadStruct> *)));
	connect(finishedWidget, SIGNAL(clearFinishedList()), pShare, SLOT(clearFinishedDownloads()));
	connect(pShare, SIGNAL(finishedDownloadAdded(FinishedDownloadStruct)), finishedWidget, SLOT(addFinishedDownload(FinishedDownloadStruct)));
	
	tabs->addTab(finishedWidget->widget(), QIcon(":/ArpmanetDC/Resources/DownloadFinishedIcon.png"), tr("Finished Downloads"));

	tabs->setCurrentIndex(tabs->indexOf(finishedWidget->widget()));
}

void ArpmanetDC::shareActionPressed()
{
	//Check if share widget exists already
	if (shareWidget)
	{
		//If it does, select it and return
		if (tabs->indexOf(shareWidget->widget()) != -1)
		{
			tabs->setCurrentIndex(tabs->indexOf(shareWidget->widget()));
			return;
		}
	}

	//Otherwise, create it
	shareWidget = new ShareWidget(pShare, this);
	connect(shareWidget, SIGNAL(saveButtonPressed()), this, SLOT(shareSaveButtonPressed()));
	
	tabs->addTab(shareWidget->widget(), QIcon(":/ArpmanetDC/Resources/ShareIcon.png"), tr("Share"));

	tabs->setCurrentIndex(tabs->indexOf(shareWidget->widget()));
}

void ArpmanetDC::reconnectActionPressed()
{
	//TODO: Reconnect was pressed
	pHub->setHubAddress(pSettings->value("hubAddress"));
	pHub->setHubPort(pSettings->value("hubPort").toShort());
	pHub->connectHub();
}

void ArpmanetDC::settingsActionPressed()
{
	//Check if share widget exists already
	if (settingsWidget)
	{
		//If it does, select it and return
		if (tabs->indexOf(settingsWidget->widget()) != -1)
		{
			tabs->setCurrentIndex(tabs->indexOf(settingsWidget->widget()));
			return;
		}
	}

	//Otherwise, create it
	settingsWidget = new SettingsWidget(pSettings, this);
	connect(settingsWidget, SIGNAL(settingsSaved()), this, SLOT(settingsSaved()));
	
	tabs->addTab(settingsWidget->widget(), QIcon(":/ArpmanetDC/Resources/SettingsIcon.png"), tr("Settings"));

	tabs->setCurrentIndex(tabs->indexOf(settingsWidget->widget()));
}

void ArpmanetDC::helpActionPressed()
{
	//Check if help widget exists already
	if (helpWidget)
	{
		//If it does, select it and return
		if (tabs->indexOf(helpWidget->widget()) != -1)
		{
			tabs->setCurrentIndex(tabs->indexOf(helpWidget->widget()));
			return;
		}
	}

	//Otherwise, create it
	helpWidget = new HelpWidget(this);
	
	tabs->addTab(helpWidget->widget(), QIcon(":/ArpmanetDC/Resources/HelpIcon.png"), tr("Help"));

	tabs->setCurrentIndex(tabs->indexOf(helpWidget->widget()));
}

void ArpmanetDC::privateMessageActionPressed()
{
	//Get selected users
	QModelIndex selectedIndex = userListTable->selectionModel()->selectedRows().first();//userListTable->selectionModel()->selection().indexes().first();
	//Get nick of first user
	QString nick = userSortProxy->data(userSortProxy->index(selectedIndex.row(), 2)).toString();

	//Check if a tab exists with a PM for this nick
	QWidget *foundWidget = 0;
	QHashIterator<QWidget *, PMWidget *> i(pmWidgetHash);
	while (i.hasNext())
	{
		if (i.peekNext().value()->otherNick().compare(nick) == 0)
		{
			foundWidget = i.value()->widget();
			break;
		}
		i.next();
	}

	//If no existing tab is found - create new one
	if (!foundWidget)
	{
		PMWidget *pmWidget = new PMWidget(nick, this);
		connect(pmWidget, SIGNAL(sendPrivateMessage(QString, QString, PMWidget *)), this, SLOT(pmSent(QString, QString, PMWidget *)));
		pmWidgetHash.insert(pmWidget->widget(), pmWidget);

		tabs->addTab(pmWidget->widget(), QIcon(":/ArpmanetDC/Resources/UserIcon.png"), tr("PM - %1").arg(nick));
		tabs->setCurrentIndex(tabs->indexOf(pmWidget->widget()));
	}
	//Else switch to existing tab
	else
	{
		tabs->setCurrentIndex(tabs->indexOf(foundWidget));
	}
}

//Show right-click menu on user list
void ArpmanetDC::showUserListContextMenu(const QPoint &pos)
{
    if (userListTable->selectionModel()->selectedRows().size() == 0)
        return;

	QPoint globalPos = userListTable->viewport()->mapToGlobal(pos);

	userListMenu->popup(globalPos);
}

//When a tab is deleted - free from memory
void ArpmanetDC::tabDeleted(int index)
{
	//Don't delete arpmanetDC tab
	if (index == 0)
		return;

	//Delete search tab
	if (searchWidgetHash.contains(tabs->widget(index)))
	{
        SearchWidget *widget = searchWidgetHash.value(tabs->widget(index));
        searchWidgetIDHash.remove(widget->id());
		widget->deleteLater();
		searchWidgetHash.remove(tabs->widget(index));
	}

	//Delete PM tab
	if (pmWidgetHash.contains(tabs->widget(index)))
	{
		pmWidgetHash.value(tabs->widget(index))->deleteLater();
		pmWidgetHash.remove(tabs->widget(index));
	}
	//Delete share tab
	if (shareWidget)
	{
		if (shareWidget->widget() == tabs->widget(index))
		{
			shareWidget->deleteLater();
			shareWidget = 0;
		}
	}
	//Delete share tab
	if (queueWidget)
	{
		if (queueWidget->widget() == tabs->widget(index))
		{
			queueWidget->deleteLater();
			queueWidget = 0;
		}
	}
	//Delete share tab
	if (settingsWidget)
	{
		if (settingsWidget->widget() == tabs->widget(index))
		{
			settingsWidget->deleteLater();
			settingsWidget = 0;
		}
	}

	tabs->removeTab(index);
}

//Delete notifications when switching to that tab
void ArpmanetDC::tabChanged(int index)
{
	if (tabs->tabBar()->tabTextColor(index) == tabTextColorNotify)
		tabs->tabBar()->setTabTextColor(index, tabTextColorNormal);
}

void ArpmanetDC::searchButtonPressed(quint64 id, QString searchStr, QByteArray searchPacket, SearchWidget *sWidget)
{
	//Search button was pressed on a search tab
	pDispatcher->initiateSearch(id, searchPacket);
    pShare->querySearchString(QHostAddress("127.0.0.1"), QByteArray("ME!"), id, searchPacket);

	tabs->setTabText(tabs->indexOf(sWidget->widget()), tr("Search - %1").arg(searchStr.left(20)));
}

/*
//Received a search result from ShareSearch
void ArpmanetDC::ownResultReceived(quint64 id, QByteArray searchPacket)
{
   // if (searchWidgetIDHash.contains(id))
   //     searchWidgetIDHash.value(id)->addSearchResult(QHostAddress("127.0.0.1"), "ME!", searchPacket);
}
*/

//Received a search result from dispatcher
void ArpmanetDC::searchResultReceived(QHostAddress senderHost, QByteArray senderCID, quint64 searchID, QByteArray searchResult)
{
    if (searchWidgetIDHash.contains(searchID))
        searchWidgetIDHash.value(searchID)->addSearchResult(senderHost, senderCID, searchResult);
}

void ArpmanetDC::shareSaveButtonPressed()
{
	//Delete share tab
	if (shareWidget)
	{
		setStatus(tr("Share update procedure started. Parsing directories/paths..."));
		tabs->removeTab(tabs->indexOf(shareWidget->widget()));
		shareWidget->deleteLater();
		shareWidget = 0;
		//Show hashing progress
		hashingProgressBar->setRange(0,0);
	}
}

void ArpmanetDC::settingsSaved()
{
    //Save settings to database
    saveSettings();

    //Reconnect hub if necessary
    if (pSettings->value("hubAddress") != pHub->getHubAddress() || pSettings->value("hubPort").toShort() != pHub->getHubPort())
        reconnectActionPressed();

    //Reconnect dispatcher if necessary
    QString externalIP = pSettings->value("externalIP");
    quint16 externalPort = pSettings->value("externalPort").toShort();
    
    if (externalIP != pDispatcher->getDispatchIP().toString() || externalPort != pDispatcher->getDispatchPort())
        pDispatcher->reconfigureDispatchHostPort(QHostAddress(externalIP), externalPort);

	//Delete settings tab
	if (settingsWidget)
	{
		setStatus(tr("Settings saved"));
		tabs->removeTab(tabs->indexOf(settingsWidget->widget()));
		settingsWidget->deleteLater();
		settingsWidget = 0;
	}
}

void ArpmanetDC::pmSent(QString otherNick, QString msg, PMWidget *pmWidget)
{
	//PM was sent in a tab
	pHub->sendPrivateMessage(otherNick, msg);
}

void ArpmanetDC::fileHashed(QString fileName)
{
	//Show on GUI when file has finished hashing
	setStatus(tr("Finished hashing file: %1").arg(fileName));
	shareSizeLabel->setText(tr("Share: %1").arg(pShare->totalShareStr()));
	//QApplication::processEvents();
}

void ArpmanetDC::directoryParsed(QString path)
{
	//Show on GUI when directory has been parsed
	setStatus(tr("Finished parsing directory/path: %1").arg(path));
	//QApplication::processEvents();
}

void ArpmanetDC::hashingDone(int msecs, int numFiles)
{
	QString timeStr = tr("%1 seconds").arg((double)msecs / 1000.0, 0, 'f', 2);

	//Show on GUI when hashing is completed
	setStatus(tr("Shares updated in %1").arg(timeStr));
	shareSizeLabel->setText(tr("Share: %1").arg(pShare->totalShareStr(true)));
    shareSizeLabel->setToolTip(tr("Share size: %1\nFiles shared: %2").arg(pShare->totalShareStr(false)).arg(numFiles));
	hashingProgressBar->setRange(0,1);
}

void ArpmanetDC::parsingDone()
{
	//Show on GUI when directory parsing is completed
	setStatus(tr("Finished directory/path parsing. Checking for new/modified files..."));
}

void ArpmanetDC::receivedPrivateMessage(QString otherNick, QString msg)
{
	//Check if a tab exists with a PM for this nick
	QWidget *foundWidget = 0;
	QHashIterator<QWidget *, PMWidget *> i(pmWidgetHash);
	while (i.hasNext())
	{
		if (i.peekNext().value())
		{
			if (i.peekNext().value()->otherNick() == otherNick)
			{
				foundWidget = i.peekNext().key();
				break;
			}
		}
		i.next();
	}

	//If no existing tab is found - create new one
	if (!foundWidget)
	{
		PMWidget *pmWidget = new PMWidget(otherNick, this);
		connect(pmWidget, SIGNAL(sendPrivateMessage(QString, QString, PMWidget *)), this, SLOT(pmSent(QString, QString, PMWidget *)));
		pmWidgetHash.insert(pmWidget->widget(), pmWidget);

		tabs->addTab(pmWidget->widget(), QIcon(":/ArpmanetDC/Resources/UserIcon.png"), tr("PM - %1").arg(otherNick));
		
		//If on mainchat, switch to PM
		if (tabs->currentIndex() == 0)
		{
			tabs->setCurrentIndex(tabs->indexOf(pmWidget->widget()));
		}
		//Else notify tab
		else
		{
			//Don't notify if already in that tab
			if (tabs->currentIndex() != tabs->indexOf(pmWidget->widget()))
			{
				//Notify tab
				tabs->tabBar()->setTabTextColor(tabs->indexOf(pmWidget->widget()), tabTextColorNotify);
			}
		}
		//tabs->setCurrentIndex(tabs->indexOf(pmWidget->widget()));

		pmWidget->receivePrivateMessage(msg);
	}
	//Else notify existing tab
	else
	{
		//If on mainchat, switch to PM
		if (tabs->currentIndex() == 0)
		{
			tabs->setCurrentIndex(tabs->indexOf(foundWidget));
		}
		//Else notify tab
		else
		{
			//Don't notify if already in that tab
			if (tabs->currentIndex() != tabs->indexOf(foundWidget))
			{
				//Notify tab
				tabs->tabBar()->setTabTextColor(tabs->indexOf(foundWidget), tabTextColorNotify);
			}
		}
		//Process message
		pmWidgetHash.value(foundWidget)->receivePrivateMessage(msg);
	}
}

//Received a main chat message from the hub
void ArpmanetDC::appendChatLine(QString msg)
{
	//Change mainchat user nick format
	if (msg.left(1).compare("<") == 0)
	{
		QString nick = msg.mid(1,msg.indexOf(">")-1);
		msg.remove(0,msg.indexOf(">")+1);
		msg.prepend(tr("<b>%1</b>").arg(nick));
	}

	//Replace new lines with <br/>
	msg.replace("\n"," <br/>");
	msg.replace("\r","");

	//Replace nick with red text
	msg.replace(pSettings->value("nick"), tr("<font color=\"red\">%1</font>").arg(pSettings->value("nick")));

	//Convert plain text links to HTML links
	convertHTMLLinks(msg);

	//Convert plain text magnet links
	convertMagnetLinks(msg);

	//Delete the first line if buffer is full
    while (mainChatBlocks > MAX_MAINCHAT_BLOCKS)
	{
		QTextCursor tcOriginal = mainChatTextEdit->textCursor();
		QTextCursor tc = mainChatTextEdit->textCursor();
		tc.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
		tc.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
		tc.removeSelectedText();
		tc.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
				
		mainChatTextEdit->setTextCursor(tc);
		mainChatBlocks--;
	}

	//Output chat line with current time
	mainChatTextEdit->append(tr("<b>[%1]</b> %2").arg(QTime::currentTime().toString()).arg(msg));
	mainChatBlocks++;

	//If not on mainchat, notify tab
	if (tabs->currentIndex() != 0)
	{
		tabs->tabBar()->setTabTextColor(0, tabTextColorNotify);
	}
}

void ArpmanetDC::sortUserList()
{
	//Sort user list when timer has expired and sorting is required
	if (sortDue)
	{
		userSortProxy->sort(2, Qt::AscendingOrder);
		resizeRowsToContents(userListTable);
		sortDue = false;
	}
}

void ArpmanetDC::userListInfoReceived(QString nick, QString desc, QString mode)
{
	//Don't add empty nicknames
	if (nick.isEmpty())
		return;

	//Check if user is already present in the list
	QList<QStandardItem *> foundItems = userListModel->findItems(nick, Qt::MatchExactly, 2);

	//Not present - create new
	if (foundItems.isEmpty())
	{
		//Add new row into table
		userListModel->appendRow(new QStandardItem());
		
		//Use different formats for Active/Passive users
		QStandardItem *item = new QStandardItem();
		item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

		if (mode.compare("P") == 0)
		{
			//Passive user
			item->setText(tr("<font size=\"2\">%1</font>").arg(nick));
			item->setIcon(QIcon(*userFirewallIcon));
		}
		else
		{
			//Active user
			item->setText(tr("<font size=\"2\">%1</font>").arg(nick));
			item->setIcon(QIcon(*userIcon));
		}
		
		//Add nick to model
		userListModel->setItem(userListModel->rowCount()-1, 0, item);		

		//Add description to model
		QStandardItem *descItem = new QStandardItem(tr("<font size=\"2\">%1</font>").arg(desc));
		descItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		userListModel->setItem(userListModel->rowCount()-1, 1, descItem);

		//Add non displaying fields
		userListModel->setItem(userListModel->rowCount()-1, 2, new QStandardItem(nick));
		userListModel->setItem(userListModel->rowCount()-1, 3, new QStandardItem(desc));
		userListModel->setItem(userListModel->rowCount()-1, 4, new QStandardItem(mode));
		
		//Signal for sorting
		sortDue = true;

		//Update user count
		userHubCountLabel->setText(tr("Hub Users: %1").arg(userListModel->rowCount()));		
	}
	//Present - edit existing
	else
	{
		//Get first match (there should be only one)
		int foundIndex = foundItems.first()->index().row();

		//Use different formats for Active/Passive users
		if (mode.compare("P") == 0)
		{
			//Passive user
			userListModel->item(foundIndex, 0)->setIcon(QIcon(*userFirewallIcon));
		}
		else
		{
			//Active user
			userListModel->item(foundIndex, 0)->setIcon(QIcon(*userIcon));
		}

		if (!desc.isEmpty())
		{
			//Edit description
			userListModel->item(foundIndex, 1)->setText(tr("<font size=\"2\">%1</font>").arg(desc));
			userListModel->item(foundIndex, 1)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

			//Edit non displaying field
			userListModel->item(foundIndex, 3)->setText(desc);
		}

		if (!mode.isEmpty())
			userListModel->item(foundIndex, 4)->setText(mode);
		
		//Signal for sorting
		sortDue = true;
	}
}

void ArpmanetDC::userListUserLoggedOut(QString nick)
{
	//Find the items matching the nickname in the model
	QList<QStandardItem *> items = userListModel->findItems(nick, Qt::MatchFixedString, 2);
	
	if (items.size() == 0)
		return;

	//Remove the user from the list
	if (userListModel->removeRows(items.first()->index().row(), 1))
		additionalInfoLabel->setText(tr("User logged out: %1").arg(nick));
	else
		additionalInfoLabel->setText(tr("User not in list: %1").arg(nick));

	//Update user count
	userHubCountLabel->setText(tr("Hub Users: %1").arg(userListModel->rowCount()));
}

void ArpmanetDC::userListNickListReceived(QStringList list)
{
	//Add/update user list for every nick in nickList
	foreach (QString nick, list)
		userListInfoReceived(nick, "", "");
}

void ArpmanetDC::hubOffline()
{
	tabs->setTabIcon(0, QIcon(":/ArpmanetDC/Resources/ServerOfflineIcon.png"));
	setStatus("Hub offline");
}

void ArpmanetDC::hubOnline()
{
	tabs->setTabIcon(0, QIcon(":/ArpmanetDC/Resources/ServerIcon.png"));
	setStatus("Hub online");
}

void ArpmanetDC::bootstrapStatusChanged(int status)
{
    // bootstrapStatus
    // -2: Absoluut clueless en nowhere
    // -1: Besig om multicast bootstrap te probeer
    //  0: Besig om broadcast bootstrap te probeer
    //  1: Aanvaar broadcast, geen ander nodes naby, het bucket ID gegenereer.
    //  2: Suksesvol gebootstrap op broadcast
    //  3: Suksesvol gebootstrap op multicast

	bootstrapStatusLabel->setText(tr("%1").arg(status));
	
	//If not bootstrapped yet
	if (status <= 0)
		connectionIconLabel->setPixmap(*unbootstrappedIcon);
	//No other nodes nearby, waiting
	else if (status == 1)
		connectionIconLabel->setPixmap(*bootstrappedIcon);
    //If bootstrapped in either broadcast/multicast
    else
        connectionIconLabel->setPixmap(*fullyBootstrappedIcon);
}

void ArpmanetDC::setStatus(QString msg)
{
    //Save history
    pStatusHistoryList->append(tr("[%1]: %2").arg(QTime::currentTime().toString()).arg(msg));
    if (pStatusHistoryList->size() > MAX_STATUS_HISTORY_ENTRIES)
        pStatusHistoryList->removeFirst();

    QString history;
    for (int i = 0; i < pStatusHistoryList->size(); i++)
    {
        if (!history.isEmpty())
            history += "\n";
        history.append(pStatusHistoryList->at(i));
    }
   
    statusLabel->setToolTip(history);
    
    //Elide text to avoid stupid looking shifting of permanent widgets in the status bar
    QFontMetrics fm(statusLabel->font());
    statusLabel->setText(fm.elidedText(msg, Qt::ElideMiddle, statusLabel->width()));
}

//Get the queue from the database
void ArpmanetDC::returnQueueList(QHash<QByteArray , QueueStruct> *queue)
{
    if (pQueueList)
        delete pQueueList;
    pQueueList = queue;
}

//Add a download to the queue
void ArpmanetDC::addDownloadToQueue(QueueStruct item)
{
    if (!pQueueList->contains(*item.tthRoot))
    {
        //Insert into global queue structure
        pQueueList->insert(*item.tthRoot, item);
        
        //Insert into database
        emit saveQueuedDownload(item);

        //Insert into queueWidget
        if (queueWidget)
        {
            //Check if queueWidget is open
            if (tabs->indexOf(queueWidget->widget()) != -1)
                queueWidget->addQueuedDownload(item);
        }
    }
}

//Remove download from the queue
void ArpmanetDC::deleteFromQueue(QByteArray tth)
{
    if (pQueueList->contains(tth))
    {
        //Remove from transfer manager queue
        pTransferManager->removeQueuedDownload((int)pQueueList->value(tth).priority, tth);

        //Remove from queue
        pQueueList->remove(tth);

        //Remove from database
        emit removeQueuedDownload(tth);
    }
}

//Change queue item priority
void ArpmanetDC::setQueuePriority(QByteArray tth, QueuePriority priority)
{
    if (pQueueList->contains(tth))
    {
        QueueStruct s = pQueueList->take(tth);

        //Set transfer manager priority
        pTransferManager->changeQueuedDownloadPriority((int)s.priority, (int)priority, *s.tthRoot);

        //Set priority in queue
        s.priority = priority;
        pQueueList->insert(tth, s);  

        //Set priority in database
        emit setQueuedDownloadPriority(tth, priority);
    }
}

void ArpmanetDC::convertHTMLLinks(QString &msg)
{
	//Replace html links with hrefs
	int currentIndex = 0;
	QString regex = "(href\\s*=\\s*['\"]?)?((www\\.|(http|https|ftp|news|file)+\\:\\/\\/)[&#95;._a-z0-9\\-]+\\.[a-z0-9\\/&#95;:@=.+?,##%&~\\-_]*[^.|\\'|\\# |!|\\(|?|,| |>|<|;|\\)])";
	QRegExp rx(regex, Qt::CaseInsensitive);
	
	int pos = 0;
	//Check for regex's and replace
	while ((pos = rx.indexIn(msg, pos)) != -1)
	{
		//Extract link
		QString link = msg.mid(pos, rx.matchedLength());
		if (link.left(5).compare("href=") == 0)
		{
			//Skip if already an href
			pos += 2*link.size();
			continue;
		}

		//Construct href
		QString hrefLink = tr("<a href=\"%1\">%1</a>").arg(link);

		//Replace link
		msg.remove(pos, link.size());
		msg.insert(pos, hrefLink);

		//Replace www. links with http://www
		msg.replace("href=\"www", "href=\"http://www");
		pos += hrefLink.size();
	}
}

void ArpmanetDC::convertMagnetLinks(QString &msg)
{
	//Replace magnet links with hrefs
	int currentIndex = 0;
	QString regex = "(magnet:\\?xt\\=urn:(?:tree:tiger|sha1):([a-z0-9]{32,39})([a-z0-9\\/&#95;:@=.+?,##%&~\\-_]*))";
	QRegExp rx(regex, Qt::CaseInsensitive);

	int pos = 0;
	//Check for regex's and replace
	while ((pos = rx.indexIn(msg, pos)) != -1)
	{
		//Extract link
		QString link = rx.cap(1);
		QString hash = rx.cap(2);
		QString extra = rx.cap(3);

		QString size, fileName;
		QString sizeStr;

		//Parse filename and size
		if (!extra.isEmpty())
		{
			//Parse size
			QString sizeRegEx = "&xl=([\\d]+)";
			QRegExp sRx(sizeRegEx, Qt::CaseInsensitive);

			int posS = 0;
			if ((posS = sRx.indexIn(extra, posS)) != -1)
				size = sRx.cap(1);

			//Convert to correct unit
			double sizeInt = size.toDouble();
            sizeStr = bytesToSize(size.toULongLong());

			//Parse filename
			QString fileNameRegEx = "&dn=([a-z0-9+\\-_.]*(?:[\\.]+[a-z0-9+\\-_]*))";
			QRegExp fRx(fileNameRegEx, Qt::CaseInsensitive);

			//Replace +'s with spaces
			if ((posS = fRx.indexIn(extra, posS)) != -1)
				fileName = fRx.cap(1).replace("+", " ");
		}

		if (link.left(5).compare("href=") == 0)
		{
			//Skip if already an href
			pos += 2*link.size();
			continue;
		}

		//Construct href
		QString hrefLink = tr("<a href=\"%1\">%2 (%3)</a>").arg(link).arg(fileName).arg(sizeStr);

		//Replace link
		msg.remove(pos, link.size());
		msg.insert(pos, hrefLink);

		pos += hrefLink.size();
	}
}

QHostAddress ArpmanetDC::getIPGuess()
{
    //For jokes, get the actual IP of the computer and use the first one for the dispatcher
	//QList<QHostAddress> ips;
	//QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
	foreach (QNetworkInterface interf, QNetworkInterface::allInterfaces())
	{
		//Only check the online interfaces capable of broadcasting that's not the local loopback
		if (interf.flags().testFlag(QNetworkInterface::IsRunning) 
			&& interf.flags().testFlag(QNetworkInterface::CanBroadcast) 
			&& !interf.flags().testFlag(QNetworkInterface::IsLoopBack)
			&& interf.flags().testFlag(QNetworkInterface::IsUp))
		{
			foreach (QNetworkAddressEntry entry, interf.addressEntries())
			{
				//Only add IPv4 addresses
				if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
				{
					//ips.append(entry.ip());
                    return entry.ip();
				}
			}
		}
	}

    return QHostAddress();
}

QString ArpmanetDC::getDefaultDownloadPath()
{
    return QDir::homePath();
}

QString ArpmanetDC::downloadPath()
{
    return pSettings->value("downloadPath");
}

QString ArpmanetDC::nick()
{
	return pSettings->value("nick");
}

QString ArpmanetDC::password()
{
	return pSettings->value("password");
}

sqlite3 *ArpmanetDC::database() const
{
	return db;
}
