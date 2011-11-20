#include "arpmanetdc.h"

ArpmanetDC::ArpmanetDC(QWidget *parent, Qt::WFlags flags)
	: QMainWindow(parent, flags)
{
	//QApplication::setStyle(new QCleanlooksStyle());

	pNick = DEFAULT_NICK;
	pPassword = DEFAULT_PASSWORD;
	pHubIP = DEFAULT_HUB_ADDRESS;
	pHubPort = DEFAULT_HUB_PORT;
	mainChatLines = 0;

	//Create and connect Hub Connection
	pHub = new HubConnection(this);

	connect(pHub, SIGNAL(receivedChatMessage(QString)), this, SLOT(appendChatLine(QString)));
	connect(pHub, SIGNAL(receivedMyINFO(QString, QString, QString)), this, SLOT(userListInfoReceived(QString, QString, QString)));
	connect(pHub, SIGNAL(receivedNickList(QStringList)), this, SLOT(userListNickListReceived(QStringList)));
	connect(pHub, SIGNAL(userLoggedOut(QString)), this, SLOT(userListUserLoggedOut(QString)));	
	connect(pHub, SIGNAL(receivedPrivateMessage(QString, QString)), this, SLOT(receivedPrivateMessage(QString, QString)));

	pHub->setHubAddress(pHubIP);
	pHub->setHubPort(pHubPort);
	pHub->setNick(pNick);
	pHub->setPassword(pPassword);
	pHub->connectHub();

	//For jokes, get the actual IP of the computer and use the first one for the dispatcher
	QList<QHostAddress> ips;
	QList<QString> ipsStr;
	QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
	foreach (QNetworkInterface interf, interfaces)
	{
		if (interf.flags().testFlag(QNetworkInterface::IsRunning) 
			&& interf.flags().testFlag(QNetworkInterface::CanBroadcast) 
			&& !interf.flags().testFlag(QNetworkInterface::IsLoopBack)
			&& interf.flags().testFlag(QNetworkInterface::IsUp))
		{
			foreach (QNetworkAddressEntry entry, interf.addressEntries())
			{
				if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
				{
					ips.append(entry.ip());
					ipsStr.append(entry.ip().toString());
				}
			}
		}
	}

	//Create and connect Dispatcher connection
	quint16 port = DISPATCHER_PORT;
	pDispatcher = new Dispatcher(ips.first(), port);

	//Set up thread for database / ShareSearch
	dbThread = new ExecThread();

	pShare = new ShareSearch(MAX_SEARCH_RESULTS, this);
	connect(pShare, SIGNAL(fileHashed(QString)), this, SLOT(fileHashed(QString)), Qt::QueuedConnection);
	connect(pShare, SIGNAL(directoryParsed(QString)), this, SLOT(directoryParsed(QString)), Qt::QueuedConnection);
	connect(pShare, SIGNAL(hashingDone(int)), this, SLOT(hashingDone(int)), Qt::QueuedConnection);
	connect(pShare, SIGNAL(parsingDone()), this, SLOT(parsingDone()), Qt::QueuedConnection);
	connect(this, SIGNAL(updateShares()), pShare, SLOT(updateShares()), Qt::QueuedConnection);

	pShare->moveToThread(dbThread);
	dbThread->start();

	//Set database pointer to zero at start
	db = 0;

	//GUI setup
	createWidgets();
	placeWidgets();
	connectWidgets();	

	setupDatabase();

	//Icon generation
	userIcon = new QPixmap();
	*userIcon = QPixmap(":/ArpmanetDC/Resources/UserIcon.png").scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	userFirewallIcon = new QPixmap();
	*userFirewallIcon = QPixmap(":/ArpmanetDC/Resources/FirewallIcon.png").scaled(16,16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

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
	dbThread->exit(0);
	if (dbThread->wait())
		delete dbThread;
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

	//Create FileShares table - list of all files hashed
	queries.append("CREATE TABLE FileShares (rowID INTEGER PRIMARY KEY, tth TEXT, fileName TEXT, fileSize INTEGER, filePath TEXT, lastModified TEXT, shareDirID INTEGER, active INTEGER, FOREIGN KEY(shareDirID) REFERENCES SharePaths(rowID), UNIQUE(filePath));");
	queries.append("CREATE INDEX IDX_SEARCH on FileShares(searchFileName);");

	//Create 1MB TTH table - list of the 1MB bucket TTHs for every fileshare
	queries.append("CREATE TABLE OneMBTTH (rowID INTEGER PRIMARY KEY, oneMBtth TEXT, tth TEXT, offset INTEGER, fileShareID INTEGER, FOREIGN KEY(fileShareID) REFERENCES FileShares(rowID));");
	queries.append("CREATE INDEX IDX_TTH on OneMBTTH(tth);");

	//Create SharePaths table - list of all the folders/files chosen in ShareWidget
	queries.append("CREATE TABLE SharePaths (rowID INTEGER PRIMARY KEY, path TEXT, UNIQUE(path));");

	//Create TTHSources table - list of all sources for a transfer
	queries.append("CREATE TABLE TTHSources (rowID INTEGER PRIMARY KEY, tthRoot TEXT, source TEXT, UNIQUE(tthRoot, source));");
	
	//Create QueuedDownloads table - saves all queued downloads to restart transfers after restart
	queries.append("CREATE TABLE QueuedDownloads (rowID INTEGER PRIMARY KEY, fileName TEXT, filePath TEXT, fileSize INTEGER, priority INTEGER, tthRoot TEXT, UNIQUE(filePath));");

	//Create FinishedDownloads table - saves download paths that were downloaded for list
	queries.append("CREATE TABLE FinishedDownloads (rowID INTEGER PRIMARY KEY, fileName TEXT, filePath TEXT, downloadedDate TEXT, UNIQUE(filePath));");

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

	//Update shares
	emit updateShares();

	return true;
}

void ArpmanetDC::createWidgets()
{
	//Labels
	userHubCountLabel = new QLabel(tr("Hub Users"));
	additionalInfoLabel = new QLabel(tr("Additional Info"));

	statusLabel = new QLabel(tr("Status"));
	shareSizeLabel = new QLabel(tr("Share: 0 bytes"));

	//Progress bar
	hashingProgressBar = new QProgressBar(this);
	hashingProgressBar->setRange(0,1);
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

	//===== Transfer list =====
	//Model
	transferListModel = new QStandardItemModel(0,1);
	transferListModel->setHeaderData(0, Qt::Horizontal, tr("Transfer"));

	//Table
	transferListTable = new QTableView(this);
	transferListTable->setContextMenuPolicy(Qt::CustomContextMenu);

	//Link table and model
	transferListTable->setModel(transferListModel);

	//Sizing
	transferListTable->horizontalHeader()->setResizeMode(QHeaderView::Interactive);

	//Style
	transferListTable->setShowGrid(true);
	userListTable->setGridStyle(Qt::DotLine);
	transferListTable->verticalHeader()->hide();
	transferListTable->setItemDelegate(new HTMLDelegate(transferListTable));
		
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
	
	tabs->addTab(tabWidget, QIcon(":/ArpmanetDC/Resources/ServerIcon.png"), tr("Arpmanet Chat"));
	tabs->setTabPosition(QTabWidget::North);
	tabTextColorNormal = tabs->tabBar()->tabTextColor(tabs->indexOf(tabWidget));

	//Place horizontal widgets
	splitterHorizontal->addWidget(tabs);
	splitterHorizontal->addWidget(transferListTable);
	
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
	connect(transferListTable, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showTransferListContextMenu(const QPoint&)));

	//Send chat message when enter is pressed
	connect(chatLineEdit, SIGNAL(returnPressed()), this, SLOT(sendChatMessage()));
	
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
	SearchWidget *sWidget = new SearchWidget(this);
	connect(sWidget, SIGNAL(search(quint64, QString, SearchWidget *)), this, SLOT(searchButtonPressed(quint64, QString, SearchWidget *)));
	searchWidgetHash.insert(sWidget->widget(), sWidget);

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
	QList<QueueStruct> *list = new QList<QueueStruct>();
	for (int i = 0; i < 10; i++)
	{
		QueueStruct q;
		q.fileName = tr("File #%1").arg(i+1);
		q.filePath = tr("H:/%1").arg(q.fileName);
		q.fileSize = i+1;
		q.priority = NormalQueuePriority;
		q.tthRoot = new QByteArray("12345");
		list->append(q);
	}

	queueWidget = new DownloadQueueWidget(list, this);
	//connect(shareWidget, SIGNAL(saveButtonPressed()), this, SLOT(shareSaveButtonPressed()));
	
	tabs->addTab(queueWidget->widget(), QIcon(":/ArpmanetDC/Resources/QueueIcon.png"), tr("Download Queue"));

	tabs->setCurrentIndex(tabs->indexOf(queueWidget->widget()));
}

void ArpmanetDC::downloadFinishedPressed()
{
	//TODO: Download finished widget
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
	pHub->setHubAddress(pHubIP);
	pHub->setHubPort(pHubPort);
	pHub->connectHub();
}

void ArpmanetDC::settingsActionPressed()
{
	//TODO: Settings was pressed
}

void ArpmanetDC::helpActionPressed()
{
	//TODO: Help was pressed
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
	QPoint globalPos = userListTable->viewport()->mapToGlobal(pos);

	QMenu *userListMenu = new QMenu(this);
	userListMenu->addAction(privateMessageAction);

	userListMenu->popup(globalPos);
}

//Show right-click menu on transfer list
void ArpmanetDC::showTransferListContextMenu(const QPoint &pos)
{
	QPoint globalPos = transferListTable->viewport()->mapToGlobal(pos);

	QMenu *transferListMenu = new QMenu(this);
	transferListMenu->addAction(privateMessageAction);

	transferListMenu->popup(globalPos);
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
		searchWidgetHash.value(tabs->widget(index))->deleteLater();
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

	tabs->removeTab(index);
}

//Delete notifications when switching to that tab
void ArpmanetDC::tabChanged(int index)
{
	if (tabs->tabBar()->tabTextColor(index) == tabTextColorNotify)
		tabs->tabBar()->setTabTextColor(index, tabTextColorNormal);
}

void ArpmanetDC::searchButtonPressed(quint64 id, QString search, SearchWidget *sWidget)
{
	//Search button was pressed on a search tab
	pDispatcher->initiateSearch(id, QByteArray().append(search));

	tabs->setTabText(tabs->indexOf(sWidget->widget()), tr("Search - %1").arg(search));
}

void ArpmanetDC::shareSaveButtonPressed()
{
	//Delete share tab
	if (shareWidget)
	{
		statusLabel->setText(tr("Directory parsing started"));
		tabs->removeTab(tabs->indexOf(shareWidget->widget()));
		shareWidget->deleteLater();
		shareWidget = 0;
		//Show hashing progress
		hashingProgressBar->setRange(0,0);
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
	statusLabel->setText(tr("Finished hashing: %1").arg(fileName));
	shareSizeLabel->setText(tr("Share: %1").arg(pShare->totalShareStr()));
	//QApplication::processEvents();
}

void ArpmanetDC::directoryParsed(QString path)
{
	//Show on GUI when directory has been parsed
	statusLabel->setText(tr("Finished parsing directory: %1").arg(path));
	//QApplication::processEvents();
}

void ArpmanetDC::hashingDone(int msecs)
{
	QString timeStr = tr("%1 seconds").arg((double)msecs / 1000.0, 0, 'f', 2);

	//Show on GUI when hashing is completed
	statusLabel->setText(tr("Shares updated in %1").arg(timeStr));
	shareSizeLabel->setText(tr("Share: %1").arg(pShare->totalShareStr(true)));
	hashingProgressBar->setRange(0,1);
}

void ArpmanetDC::parsingDone()
{
	//Show on GUI when directory parsing is completed
	statusLabel->setText(tr("Finished directory parsing. Starting hashing process..."));
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
	msg.replace(pNick, tr("<font color=\"red\">%1</font>").arg(pNick));

	//Convert plain text links to HTML links
	convertHTMLLinks(msg);

	//Convert plain text magnet links
	convertMagnetLinks(msg);

	//Delete the first line if buffer is full
	if (mainChatLines > MAX_MAINCHAT_LINES)
	{
		QTextCursor tcOriginal = mainChatTextEdit->textCursor();
		QTextCursor tc = mainChatTextEdit->textCursor();
		tc.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
		tc.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
		tc.removeSelectedText();
		tc.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
				
		mainChatTextEdit->setTextCursor(tc);
		mainChatLines--;
	}

	//Output chat line with current time
	mainChatTextEdit->append(tr("<b>[%1]</b> %2").arg(QTime::currentTime().toString()).arg(msg));
	mainChatLines++;

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
		userListTable->resizeRowsToContents();
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
			QString sizeUnit = "bytes";
			if (sizeInt > 1024.0)
			{
				sizeInt /= 1024.0;
				sizeUnit = "KiB";
				if (sizeInt > 1024.0)
				{
					sizeInt /= 1024.0;
					sizeUnit = "MiB";
					if (sizeInt > 1024.0)
					{
						sizeInt /= 1024.0;
						sizeUnit = "GiB";
					}
				}
			}
			sizeStr = tr("%1 %2").arg(sizeInt, 0, 'f', 2).arg(sizeUnit);

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

QString ArpmanetDC::nick()
{
	return pNick;
}

QString ArpmanetDC::password()
{
	return pPassword;
}

sqlite3 *ArpmanetDC::database() const
{
	return db;
}