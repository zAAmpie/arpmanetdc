#include "downloadqueuewidget.h"
#include "customtableitems.h"
#include "arpmanetdc.h"

DownloadQueueWidget::DownloadQueueWidget(QHash<QByteArray, QueueStruct> *queueList, ShareSearch *share, ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;
    pQueueList = queueList;
    pShare = share;

    lowPriorityIcon = QIcon(":/ArpmanetDC/Resources/LowPriorityIcon.png");
    normalPriorityIcon = QIcon(":/ArpmanetDC/Resources/QueueIcon.png");
    highPriorityIcon = QIcon(":/ArpmanetDC/Resources/HighPriorityIcon.png");
    queuedIcon = QIcon(":/ArpmanetDC/Resources/QueueIcon.png");
    busyIcon = QIcon(":/ArpmanetDC/Resources/CheckIcon.png");

	createWidgets();
	placeWidgets();
	connectWidgets();

    loadQueueList();
}

DownloadQueueWidget::~DownloadQueueWidget()
{
	//Destructor
}

void DownloadQueueWidget::createWidgets()
{
	//Table View
	queueTable = new QTableView();
	queueTable->setShowGrid(false);
	queueTable->setGridStyle(Qt::DotLine);
	queueTable->verticalHeader()->hide();
	queueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	//queueTable->setItemDelegate(new HTMLDelegate(queueTable));
	queueTable->setContextMenuPolicy(Qt::CustomContextMenu);

	//Model
	queueModel = new QStandardItemModel(0, 7);
	queueModel->setHeaderData(0, Qt::Horizontal, tr("Filename"));
	queueModel->setHeaderData(1, Qt::Horizontal, tr("Path"));
    queueModel->setHeaderData(2, Qt::Horizontal, tr("Status"));
	queueModel->setHeaderData(3, Qt::Horizontal, tr("Size"));
	queueModel->setHeaderData(4, Qt::Horizontal, tr("Priority"));
	queueModel->setHeaderData(5, Qt::Horizontal, tr("TTH"));
    queueModel->setHeaderData(6, Qt::Horizontal, tr("Host IP"));

	//Set model
	queueTable->setModel(queueModel);
	//queueTable->hideColumn(4);
	queueTable->setSortingEnabled(true);
    queueTable->horizontalHeader()->setHighlightSections(false);
    queueTable->horizontalHeader()->setStretchLastSection(true);
    queueTable->setColumnWidth(0, 300);
    queueTable->setColumnWidth(1, 200);
    queueTable->setColumnWidth(3, 75);

	//===== Actions =====
	setPriorityLowAction = new QAction(lowPriorityIcon, tr("Low priority"), this);
	setPriorityNormalAction = new QAction(normalPriorityIcon, tr("Normal priority"), this);
	setPriorityHighAction = new QAction(highPriorityIcon, tr("High priority"), this);
	deleteAction = new QAction(QIcon(":/ArpmanetDC/Resources/RemoveIcon.png"), tr("Delete"), this);

    //===== Menus =====
    setPriorityMenu = new QMenu(tr("Set priority"), pParent);
	setPriorityMenu->addAction(setPriorityHighAction);
	setPriorityMenu->addAction(setPriorityNormalAction);
	setPriorityMenu->addAction(setPriorityLowAction);

	queueMenu = new QMenu(pParent);
	queueMenu->addAction(deleteAction);
	queueMenu->addSeparator();
	queueMenu->addMenu(setPriorityMenu);
}

void DownloadQueueWidget::placeWidgets()
{
	pWidget = queueTable;
}

void DownloadQueueWidget::connectWidgets()
{
	connect(queueTable, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showQueueTableContextMenu(const QPoint&)));

	connect(setPriorityLowAction, SIGNAL(triggered()), this, SLOT(setPriorityLowActionPressed()));
	connect(setPriorityNormalAction, SIGNAL(triggered()), this, SLOT(setPriorityNormalActionPressed()));
	connect(setPriorityHighAction, SIGNAL(triggered()), this, SLOT(setPriorityHighActionPressed()));
	connect(deleteAction, SIGNAL(triggered()), this, SLOT(deleteActionPressed()));
}

void DownloadQueueWidget::showQueueTableContextMenu(const QPoint &point)
{	
	//Get selected users
	//QModelIndex selectedIndex = queueTable->selectionModel()->selectedRows().first();//userListTable->selectionModel()->selection().indexes().first();
	//Get nick of first user
	//QString  = userSortProxy->data(userSortProxy->index(selectedIndex.row(), 2)).toString();

	if (queueTable->selectionModel()->selectedRows().isEmpty())
		return;

	QPoint globalPos = queueTable->viewport()->mapToGlobal(point);

	queueMenu->popup(globalPos);
}

//Actions
void DownloadQueueWidget::setPriorityLowActionPressed()
{
    for (int i = 0; i < queueTable->selectionModel()->selectedRows().size(); i++)
    {
        //Get selected files
	    QModelIndex selectedIndex = queueTable->selectionModel()->selectedRows().at(i);//userListTable->selectionModel()->selection().indexes().first();
	    //Get TTH of file
	    QString base32TTH = queueModel->data(queueModel->index(selectedIndex.row(), 5)).toString();

        QByteArray tthRoot;
        tthRoot.append(base32TTH);
        base32Decode(tthRoot);  
        
	    pParent->setQueuePriority(tthRoot, LowQueuePriority);
        queueModel->item(selectedIndex.row(), 4)->setText("Low");
        queueModel->item(selectedIndex.row(), 4)->setIcon(lowPriorityIcon);
    }
}

void DownloadQueueWidget::setPriorityNormalActionPressed()
{
	for (int i = 0; i < queueTable->selectionModel()->selectedRows().size(); i++)
    {
        //Get selected files
	    QModelIndex selectedIndex = queueTable->selectionModel()->selectedRows().at(i);//userListTable->selectionModel()->selection().indexes().first();
	    //Get TTH of file
	    QString base32TTH = queueModel->data(queueModel->index(selectedIndex.row(), 5)).toString();

        QByteArray tthRoot;
        tthRoot.append(base32TTH);
        base32Decode(tthRoot); 
        
	    pParent->setQueuePriority(tthRoot, NormalQueuePriority);
        queueModel->item(selectedIndex.row(), 4)->setText("Normal");
        queueModel->item(selectedIndex.row(), 4)->setIcon(normalPriorityIcon);
    }
}

void DownloadQueueWidget::setPriorityHighActionPressed()
{
	for (int i = 0; i < queueTable->selectionModel()->selectedRows().size(); i++)
    {
        //Get selected files
	    QModelIndex selectedIndex = queueTable->selectionModel()->selectedRows().at(i);//userListTable->selectionModel()->selection().indexes().first();
	    //Get TTH of file
	    QString base32TTH = queueModel->data(queueModel->index(selectedIndex.row(), 5)).toString();

        QByteArray tthRoot;
        tthRoot.append(base32TTH);
        base32Decode(tthRoot);   
        
	    pParent->setQueuePriority(tthRoot, HighQueuePriority);
        queueModel->item(selectedIndex.row(), 4)->setText("High");
        queueModel->item(selectedIndex.row(), 4)->setIcon(highPriorityIcon);
    }
}

void DownloadQueueWidget::deleteActionPressed()
{
    QList<QModelIndex> selectedRows = queueTable->selectionModel()->selectedRows();

    //Remove rows from parent queue
	for (int i = 0; i < queueTable->selectionModel()->selectedRows().size(); i++)
    {
        //Get selected files
	    QModelIndex selectedIndex = queueTable->selectionModel()->selectedRows().at(i);//userListTable->selectionModel()->selection().indexes().first();
	    //Get TTH of file
	    QString base32TTH = queueModel->data(queueModel->index(selectedIndex.row(), 5)).toString();

        QByteArray tthRoot;
        tthRoot.append(base32TTH);
        base32Decode(tthRoot);   

	    pParent->deleteFromQueue(tthRoot);
    }

    //Remove rows from model
    while (!queueTable->selectionModel()->selectedRows().isEmpty())
    {
        queueModel->removeRow(queueTable->selectionModel()->selectedRows().first().row());
    }
}

//Queue list has been received
void DownloadQueueWidget::loadQueueList()
{
	//Remove all rows
	queueModel->removeRows(0, queueModel->rowCount());

    QList<QueueStruct> list = pQueueList->values();
    if (list.isEmpty())
        return;

	for (int i = 0; i < list.size(); i++)
	{
        QueueStruct q = list.at(i);

       	QList<QStandardItem *> row;
        QFileInfo fi(q.fileName);
        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, q.fileName, pParent->resourceExtractorObject()->getIconFromName(fi.suffix())));
        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, q.filePath));

        if (pParent->transferWidgetObject()->isBusy(*q.tthRoot))
            row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, "Busy", busyIcon));
        else
            row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, "Queued", queuedIcon));

        row.append(new CStandardItem(CStandardItem::SizeType, bytesToSize(q.fileSize)));

		QString priorityStr;
        QIcon icon;
		switch (q.priority)
		{
		case LowQueuePriority:
			priorityStr = "Low";
            icon = lowPriorityIcon;
			break;
		case NormalQueuePriority:
			priorityStr = "Normal";
            icon = normalPriorityIcon;
			break;
		case HighQueuePriority:
			priorityStr = "High";
            icon = highPriorityIcon;
			break;
		}
        row.append(new CStandardItem(CStandardItem::PriorityType, priorityStr, icon));
        
        QByteArray tthBase32(q.tthRoot->data(), q.tthRoot->size());
        base32Encode(tthBase32);
        row.append(new QStandardItem(tthBase32.data()));
        row.append(new QStandardItem(q.fileHost.toString()));
		
        queueModel->appendRow(row);
    }

    resizeRowsToContents(queueTable);
    queueTable->sortByColumn(2, Qt::AscendingOrder);
}

void DownloadQueueWidget::addQueuedDownload(QueueStruct file)
{
	QList<QStandardItem *> row;
    QFileInfo fi(file.fileName);
       
    row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, file.fileName, pParent->resourceExtractorObject()->getIconFromName(fi.suffix())));
    row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, file.filePath));

    if (pParent->transferWidgetObject()->isBusy(*file.tthRoot))
        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, "Busy", busyIcon));
    else
        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, "Queued", queuedIcon));

    row.append(new CStandardItem(CStandardItem::SizeType, bytesToSize(file.fileSize)));

	QString priorityStr;
    QIcon icon;
	switch (file.priority)
	{
	case LowQueuePriority:
		priorityStr = "Low";
        icon = lowPriorityIcon;
		break;
	case NormalQueuePriority:
		priorityStr = "Normal";
        icon = normalPriorityIcon;
		break;
	case HighQueuePriority:
		priorityStr = "High";
        icon = highPriorityIcon;
		break;
	}
    row.append(new CStandardItem(CStandardItem::PriorityType, priorityStr, icon));

    QByteArray tthBase32(file.tthRoot->data(), file.tthRoot->size());
    base32Encode(tthBase32);
    row.append(new QStandardItem(tthBase32.data()));
    row.append(new QStandardItem(file.fileHost.toString()));
	
    queueModel->appendRow(row); 

    resizeRowsToContents(queueTable);
    queueTable->sortByColumn(queueTable->horizontalHeader()->sortIndicatorSection(), queueTable->horizontalHeader()->sortIndicatorOrder());
}

//Remove from queue
void DownloadQueueWidget::removeQueuedDownload(QByteArray tth)
{
    QByteArray base32TTH(tth);
    base32Encode(base32TTH);

    QList<QStandardItem *> findResults = queueModel->findItems(base32TTH.data(), Qt::MatchExactly, 5);

    if (findResults.isEmpty())
        return;
    
    int row = findResults.first()->row();
    queueModel->removeRow(row);
}

//Set a download busy (i.e. the download started)
void DownloadQueueWidget::setQueuedDownloadBusy(QByteArray tth)
{
    QByteArray base32TTH(tth);
    base32Encode(base32TTH);

    QList<QStandardItem *> findResults = queueModel->findItems(base32TTH.data(), Qt::MatchExactly, 5);

    if (findResults.isEmpty())
        return;
    
    int row = findResults.first()->row();
    queueModel->item(row, 2)->setIcon(busyIcon);
    queueModel->item(row, 2)->setText("Busy");
}

QWidget *DownloadQueueWidget::widget()
{
	return pWidget;
}
