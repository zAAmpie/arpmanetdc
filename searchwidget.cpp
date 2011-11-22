#include "searchwidget.h"
#include "QCryptographicHash"
#include "arpmanetdc.h"

quint64 SearchWidget::staticID = 0;

SearchWidget::SearchWidget(ResourceExtractor *mappedIconList, ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;
    pIconList = mappedIconList;

	createWidgets();
	placeWidgets();
	connectWidgets();

	pID = staticID++;

    sortDue = false;
    sortTimer = new QTimer();
    connect(sortTimer, SIGNAL(timeout()), this, SLOT(sortTimeout()));
    sortTimer->start(500); //Sort every 500msec if necessary
}

SearchWidget::~SearchWidget()
{
	//Destructor
}

void SearchWidget::createWidgets()
{
	searchLineEdit = new QLineEdit();

    majorVersionLineEdit = new QLineEdit("0");
    majorVersionLineEdit->setValidator(new QIntValidator(0, 65535, 0));

    minorVersionLineEdit = new QLineEdit("0");
    minorVersionLineEdit->setValidator(new QIntValidator(0, 65535, 0));

	searchButton = new QPushButton(QIcon(tr(":/ArpmanetDC/Resources/SearchIcon.png")),tr("Search"));
	searchButton->setIconSize(QSize(16,16));

	searchProgress = new QProgressBar();
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
	//TODO: Connect all widgets
	connect(searchButton, SIGNAL(clicked()), this, SLOT(searchPressed()));
	connect(searchLineEdit, SIGNAL(returnPressed()), this, SLOT(searchPressed()));
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
    double sizeInt = res.fileSize;
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
	QString sizeStr = tr("%1 %2").arg(sizeInt, 0, 'f', 2).arg(sizeUnit);

    QFileInfo fi(res.fileName);
    QByteArray base32TTH(res.tthRoot);
    base32Encode(base32TTH);

    //Get parent matching base32TTH
    QList<QStandardItem *> results = resultsModel->findItems(base32TTH.data(), Qt::MatchExactly, 8);

    //Create new row
    QList<QStandardItem *> row;
    row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, res.fileName, pIconList->getIconFromName(fi.suffix())));
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

        sortDue = true;
    }

}

void SearchWidget::searchPressed()
{
    //Clear model
	resultsModel->removeRows(0, resultsModel->rowCount());

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

        sortDue = false;
    }
}

void SearchWidget::stopProgress()
{
    searchProgress->setVisible(false);
}

QIcon SearchWidget::fileIcon(const QString &filename)
{
    QFileInfo info(filename);
    const QString ext=info.suffix().toLower();

    QIcon icon;
    icon = QApplication::style()->standardIcon(QStyle::SP_FileIcon);

    return icon;
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