#include "searchwidget.h"
#include "QCryptographicHash"
#include "arpmanetdc.h"
#include "transfermanager.h"

quint64 SearchWidget::staticID = 0;

SearchWidget::SearchWidget(QCompleter *completer, ResourceExtractor *mappedIconList, TransferManager *transferManager, ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;
    pIconList = mappedIconList;
    pTransferManager = transferManager;
    pCompleter = completer;

	createWidgets();
	placeWidgets();
	connectWidgets();

	pID = staticID++;

    sortDue = false;
    sortTimer = new QTimer();
    connect(sortTimer, SIGNAL(timeout()), this, SLOT(sortTimeout()));
    sortTimer->start(500); //Sort every 500msec if necessary
}

SearchWidget::SearchWidget(QCompleter *completer, ResourceExtractor *mappedIconList, TransferManager *transferManager, QString startupSearchString, ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;
    pIconList = mappedIconList;
    pTransferManager = transferManager;
    pCompleter = completer;

	createWidgets();
	placeWidgets();
	connectWidgets();

	pID = staticID++;

    sortDue = false;
    sortTimer = new QTimer();
    connect(sortTimer, SIGNAL(timeout()), this, SLOT(sortTimeout()));
    sortTimer->start(500); //Sort every 500msec if necessary

    //Search for startup string
    searchLineEdit->setText(startupSearchString);
}

SearchWidget::~SearchWidget()
{
	//Destructor
}

void SearchWidget::createWidgets()
{
    resultNumberLabel = new QLabel("");

	searchLineEdit = new QLineEdit();
    searchLineEdit->setCompleter(pCompleter);
    searchLineEdit->setPlaceholderText("Type here to search");
    searchLineEdit->setMinimumWidth(200);

    majorVersionLineEdit = new QLineEdit("0");
    majorVersionLineEdit->setMaximumWidth(40);
    majorVersionLineEdit->setValidator(new QIntValidator(0, 65535, 0));

    minorVersionLineEdit = new QLineEdit("0");
    minorVersionLineEdit->setMaximumWidth(40);
    minorVersionLineEdit->setValidator(new QIntValidator(0, 65535, 0));

	searchButton = new QPushButton(QIcon(tr(":/ArpmanetDC/Resources/SearchIcon.png")),tr("Search"));
	searchButton->setIconSize(QSize(16,16));

	searchProgress = new TextProgressBar(tr("Searching"));
	searchProgress->setStyle(new QPlastiqueStyle());
	searchProgress->setValue(0);
	searchProgress->setRange(0,0);
	searchProgress->setVisible(false);

	resultsModel = new QStandardItemModel(0, 11);
	//Display
	resultsModel->setHeaderData(0, Qt::Horizontal, tr("Filename"));
	resultsModel->setHeaderData(1, Qt::Horizontal, tr("Hits"));
	resultsModel->setHeaderData(2, Qt::Horizontal, tr("Size"));
	resultsModel->setHeaderData(3, Qt::Horizontal, tr("Type"));
    resultsModel->setHeaderData(4, Qt::Horizontal, tr("Path"));
	
	//Used for connection
	resultsModel->setHeaderData(5, Qt::Horizontal, tr("Exact size"));
	resultsModel->setHeaderData(6, Qt::Horizontal, tr("MajorVersion"));
    resultsModel->setHeaderData(7, Qt::Horizontal, tr("MinorVersion"));
	resultsModel->setHeaderData(8, Qt::Horizontal, tr("TTH root"));
    resultsModel->setHeaderData(9, Qt::Horizontal, tr("Sender IP"));
    resultsModel->setHeaderData(10, Qt::Horizontal, tr("Sender CID"));

	resultsTable = new QTreeView();
	resultsTable->setModel(resultsModel);
    resultsTable->setSortingEnabled(true);
    resultsTable->setUniformRowHeights(true);
    resultsTable->setColumnWidth(0, 500);
    resultsTable->sortByColumn(1, Qt::DescendingOrder);
    resultsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    resultsTable->header()->setHighlightSections(false);
    resultsTable->setExpandsOnDoubleClick(false);

    parentItem = resultsModel->invisibleRootItem();

	//resultsTable->setShowGrid(true);
	//resultsTable->setGridStyle(Qt::DotLine);
	//resultsTable->verticalHeader()->hide();
	//resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	//resultsTable->setItemDelegate(new HTMLDelegate(resultsTable));

	//resultsTable->hideColumn(5);
	//resultsTable->hideColumn(6);
	//resultsTable->hideColumn(7);
    //resultsTable->hideColumn(8);

    downloadAction = new QAction(QIcon(":/ArpmanetDC/Resources/DownloadIcon.png"), tr("Download"), this);
    downloadToAction = new QAction(QIcon(":/ArpmanetDC/Resources/DownloadIcon.png"), tr("Download to folder..."), this);
    calculateMagnetAction = new QAction(QIcon(":/ArpmanetDC/Resources/MagnetIcon.png"), tr("Copy magnet link"), this);

    resultsMenu = new QMenu((QWidget *)pParent);
	resultsMenu->addAction(downloadAction);
    resultsMenu->addAction(downloadToAction);
    resultsMenu->addAction(calculateMagnetAction);

	pWidget = new QWidget();
}

void SearchWidget::placeWidgets()
{
	//TODO: Place all widgets
	QHBoxLayout *hlayout = new QHBoxLayout;
	hlayout->addSpacing(5);
	hlayout->addWidget(new QLabel(tr("Search for")));
	hlayout->addWidget(searchLineEdit);
    hlayout->addSpacing(20);
    hlayout->addWidget(new QLabel(tr("Season")));
    hlayout->addWidget(majorVersionLineEdit);
    hlayout->addWidget(new QLabel(tr("Episode")));
    hlayout->addWidget(minorVersionLineEdit);
	hlayout->addWidget(searchButton);
    hlayout->addWidget(resultNumberLabel);
	hlayout->addStretch(1);
	hlayout->addWidget(searchProgress);

	QVBoxLayout *vlayout = new QVBoxLayout;
	vlayout->addLayout(hlayout);
	vlayout->addWidget(resultsTable);
	vlayout->setContentsMargins(0,5,0,0);

	pWidget->setLayout(vlayout);
}

void SearchWidget::connectWidgets()
{
	connect(resultsTable, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showContextMenu(const QPoint&)));
    connect(resultsTable, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(resultDoubleClicked(QModelIndex)));

    connect(downloadAction, SIGNAL(triggered()), this, SLOT(downloadActionPressed()));
    connect(downloadToAction, SIGNAL(triggered()), this, SLOT(downloadToActionPressed()));
    connect(calculateMagnetAction, SIGNAL(triggered()), this, SLOT(calculateMagnetActionPressed()));

	connect(searchButton, SIGNAL(clicked()), this, SLOT(searchPressed()));
	connect(searchLineEdit, SIGNAL(returnPressed()), this, SLOT(searchPressed()));
    connect(majorVersionLineEdit, SIGNAL(returnPressed()), this, SLOT(searchPressed()));
    connect(minorVersionLineEdit, SIGNAL(returnPressed()), this, SLOT(searchPressed()));
}

void SearchWidget::downloadActionPressed()
{
    //TODO: Download to default path
    QString path = pParent->downloadPath();
    if (path.right(1).compare("/") != 0)
        path.append("/");

    for (int i = 0; i < resultsTable->selectionModel()->selectedRows().size(); i++)
    {
        //Get a result
	    QModelIndex selectedIndex = resultsTable->selectionModel()->selectedRows().at(i);
                
        //Get the selected item in column 0 and its parent
        QStandardItem *selItem = resultsModel->itemFromIndex(selectedIndex);
        QStandardItem *parent = selItem->parent();
        //If the item has a parent -> use the data from the parent
        if (parent)
            selectedIndex = parent->index();

	    //Get TTH and filename of the result
	    QString tthBase32 = resultsModel->data(resultsModel->index(selectedIndex.row(), 8)).toString();
        QString fileName = resultsModel->data(resultsModel->index(selectedIndex.row(), 0)).toString();
        quint64 fileSize = resultsModel->data(resultsModel->index(selectedIndex.row(), 5)).toULongLong();
        QHostAddress senderIP = QHostAddress(resultsModel->data(resultsModel->index(selectedIndex.row(), 9)).toString());

        //Convert TTH back to binary from Base32
        QByteArray tthRoot;
        tthRoot.append(tthBase32);
        base32Decode(tthRoot);

        //Add to download queue
        QueueStruct item;
        item.fileName = fileName;
        item.filePath = path;
        item.fileSize = fileSize;
        item.fileHost = senderIP;
        item.priority = NormalQueuePriority;
        item.tthRoot = new QByteArray(tthRoot);
        pParent->addDownloadToQueue(item);

        QString finalPath = path + fileName;
        qDebug() << "SearchWidget::downloadActionPressed() queueDownload: " << senderIP;
        emit queueDownload((int)NormalQueuePriority, tthRoot, finalPath, fileSize, senderIP);
    }
}

void SearchWidget::downloadToActionPressed()
{
    //TODO: Download to specific folder
    QString path = QFileDialog::getExistingDirectory((QWidget *)pParent, tr("Select download path"), pParent->downloadPath()).replace("\\","/");
    if (path.right(1).compare("/") != 0)
        path.append("/");

    for (int i = 0; i < resultsTable->selectionModel()->selectedRows().size(); i++)
    {
        //Get the selected row index
	    QModelIndex selectedIndex = resultsTable->selectionModel()->selectedRows().at(i);

        //Get the selected item in column 0 and its parent
        QStandardItem *selItem = resultsModel->itemFromIndex(selectedIndex);
        QStandardItem *parent = selItem->parent();
        //If the item has a parent -> use the data from the parent
        if (parent)
            selectedIndex = parent->index();

	    //Get TTH and filename of the result
	    QString tthBase32 = resultsModel->data(resultsModel->index(selectedIndex.row(), 8)).toString();
        QString fileName = resultsModel->data(resultsModel->index(selectedIndex.row(), 0)).toString();
        quint64 fileSize = resultsModel->data(resultsModel->index(selectedIndex.row(), 5)).toULongLong();
        QHostAddress senderIP = QHostAddress(resultsModel->data(resultsModel->index(selectedIndex.row(), 9)).toString());

        //Convert TTH back to binary from Base32
        QByteArray tthRoot;
        tthRoot.append(tthBase32);
        base32Decode(tthRoot);

        //Add to download queue
        QueueStruct item;
        item.fileName = fileName;
        item.filePath = path;
        item.fileSize = fileSize;
        item.fileHost = senderIP;
        item.priority = NormalQueuePriority;
        item.tthRoot = new QByteArray(tthRoot);
        pParent->addDownloadToQueue(item);

        //I'm totally guessing the protocol here??? How should I distinguish?
        QString finalPath = path + fileName;
        emit queueDownload((int)NormalQueuePriority, tthRoot, finalPath, fileSize, senderIP);
    }
}

void SearchWidget::calculateMagnetActionPressed()
{
    //Get a result
	QModelIndex selectedIndex = resultsTable->selectionModel()->selectedRows().first();
                
    //Get the selected item in column 0 and its parent
    QStandardItem *selItem = resultsModel->itemFromIndex(selectedIndex);
    QStandardItem *parent = selItem->parent();
    //If the item has a parent -> use the data from the parent
    if (parent)
        selectedIndex = parent->index();

	//Get TTH and filename of the result
	QString tthBase32 = resultsModel->data(resultsModel->index(selectedIndex.row(), 8)).toString();
    QString fileName = resultsModel->data(resultsModel->index(selectedIndex.row(), 0)).toString();
    quint64 fileSize = resultsModel->data(resultsModel->index(selectedIndex.row(), 5)).toULongLong();

    //Construct magnet link
    QString magnetLink = tr("magnet:?xt=urn:tree:tiger:%1&xl=%2&dn=%3").arg(tthBase32).arg(fileSize).arg(fileName.replace(" ", "+"));
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(magnetLink);
}

QByteArray SearchWidget::idGenerator()
{
	//Generate hash from nick/password/time
	QByteArray hash = QByteArray().append(pParent->nick() + pParent->password() + QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss:zzz"));

	QCryptographicHash *cryptHash = new QCryptographicHash(QCryptographicHash::Sha1);
	cryptHash->addData(hash);

	return cryptHash->result();
}

void SearchWidget::addSearchResult(QHostAddress sender, QByteArray cid, QByteArray result)
{
    SearchStruct res;

    //Get data from packet

    //Packet reply structure
    //===========================================
    //fileName          String (variable size)
    //relativePath      String (variable size)
    //fileSize          quint64
    //majorVersion      qint16
    //minorVersion      qint16
    //tthRoot           QByteArray (variable size)

    res.fileName = getStringFromByteArray(&result);
    res.relativePath = getStringFromByteArray(&result);
    res.fileSize = getQuint64FromByteArray(&result);
    res.majorVersion = getQint16FromByteArray(&result);
    res.minorVersion = getQint16FromByteArray(&result);
    res.tthRoot = result;

    //Convert to correct unit
    QString sizeStr = bytesToSize(res.fileSize);

    QFileInfo fi(res.fileName);
    QByteArray base32TTH(res.tthRoot);
    base32Encode(base32TTH);

    //Get parent matching base32TTH
    QList<QStandardItem *> results = resultsModel->findItems(base32TTH.data(), Qt::MatchExactly, 8);

    //Create new row
    QList<QStandardItem *> row;
    QString ext = fi.suffix();
    row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, res.fileName, pIconList->getIconFromName(ext)));
    if (results.isEmpty())
        row.append(new CStandardItem(CStandardItem::IntegerType, "1"));
    else
        row.append(new CStandardItem(CStandardItem::IntegerType, ""));
    row.append(new CStandardItem(CStandardItem::SizeType, sizeStr));
    row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, fi.suffix()));
    row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, res.relativePath));
    row.append(new CStandardItem(CStandardItem::IntegerType, tr("%1").arg(res.fileSize)));
    row.append(new CStandardItem(CStandardItem::IntegerType, tr("%1").arg(res.majorVersion)));
    row.append(new CStandardItem(CStandardItem::IntegerType, tr("%1").arg(res.minorVersion)));
    row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, base32TTH.data()));
    row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, sender.toString()));
    row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, cid.data()));

    //If new entry
    
    if (results.isEmpty())
    {
        //Add new row
        parentItem->appendRow(row);
        totalResultCount++;
        uniqueResultCount++;
        sortDue = true;
    }
    else
    {
        //Add children to the first column item
        QStandardItem *item = resultsModel->itemFromIndex(resultsModel->index(results.first()->row(), 0));
        item->appendRow(row);

        //Append hit counter for parent
        QStandardItem *hitItem = resultsModel->itemFromIndex(resultsModel->index(results.first()->row(), 1));
        hitItem->setText(tr("%1").arg(hitItem->text().toLongLong()+1));

        totalResultCount++;
        sortDue = true;
    }

}

void SearchWidget::searchPressed()
{
    //Clear model
	resultsModel->removeRows(0, resultsModel->rowCount());
    totalResultCount = 0;
    uniqueResultCount = 0;

	if (!searchLineEdit->text().isEmpty())
	{
        qint16 majorVersion = -1;
        qint16 minorVersion = -1;

        if (!majorVersionLineEdit->text().isEmpty())
            majorVersion = majorVersionLineEdit->text().toShort();
        if (!minorVersionLineEdit->text().isEmpty())
            minorVersion = minorVersionLineEdit->text().toShort();
        
        minorVersion = minorVersion == 0 ? -1 : minorVersion;
        majorVersion = majorVersion == 0 ? -1 : majorVersion;

        //Build search packet

            
        //Packet query structure
        //========================================
        //majorVersion      qint16
        //minorVersion      qint16
        //searchStr         QString (variable size)
        
        QByteArray packet;
        
        packet.append(qint16ToByteArray(majorVersion));
        packet.append(qint16ToByteArray(minorVersion));
        packet.append(stringToByteArray(searchLineEdit->text()));

        emit search(pID, searchLineEdit->text(), packet, this);
		searchProgress->setVisible(true);

        QTimer::singleShot(10000, this, SLOT(stopProgress()));
	}
}

void SearchWidget::sortTimeout()
{
    if (sortDue)
    {
        int column = resultsTable->header()->sortIndicatorSection();
        Qt::SortOrder order = resultsTable->header()->sortIndicatorOrder();
        resultsTable->sortByColumn(column, order);

        resultNumberLabel->setText(tr("Returned %1 unique and %2 total results").arg(uniqueResultCount).arg(totalResultCount));
        sortDue = false;
    }
}

void SearchWidget::stopProgress()
{
    searchProgress->setVisible(false);
}

//Show right-click menu on transfer list
void SearchWidget::showContextMenu(const QPoint &pos)
{
    if (resultsTable->selectionModel()->selectedRows().size() == 0)
        return;
    else if (resultsTable->selectionModel()->selectedRows().size() == 1)
        calculateMagnetAction->setVisible(true);
    else
        calculateMagnetAction->setVisible(false);

	QPoint globalPos = resultsTable->viewport()->mapToGlobal(pos);

	resultsMenu->popup(globalPos);
}

void SearchWidget::resultDoubleClicked(QModelIndex index)
{
    //Invoke download action - since you can't double click on something without selecting it as well
    downloadActionPressed();
}

//===== GET FUNCTIONS =====

quint64 SearchWidget::id()
{
	return pID;
}

QWidget *SearchWidget::widget()
{
	return pWidget;
}
