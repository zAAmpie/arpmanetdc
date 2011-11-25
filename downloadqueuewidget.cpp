#include "downloadqueuewidget.h"
#include "customtableitems.h"
#include "arpmanetdc.h"

DownloadQueueWidget::DownloadQueueWidget(QHash<QByteArray, QueueStruct> *queueList, ShareSearch *share, ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;
    pQueueList = queueList;
    pShare = share;

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
	queueTable->setShowGrid(true);
	queueTable->setGridStyle(Qt::DotLine);
	queueTable->verticalHeader()->hide();
	queueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	//queueTable->setItemDelegate(new HTMLDelegate(queueTable));
	queueTable->setContextMenuPolicy(Qt::CustomContextMenu);

	//Model
	queueModel = new QStandardItemModel(0, 5);
	queueModel->setHeaderData(0, Qt::Horizontal, tr("Filename"));
	queueModel->setHeaderData(1, Qt::Horizontal, tr("Path"));
	queueModel->setHeaderData(2, Qt::Horizontal, tr("Size"));
	queueModel->setHeaderData(3, Qt::Horizontal, tr("Priority"));
	queueModel->setHeaderData(4, Qt::Horizontal, tr("TTH"));

	//Set model
	queueTable->setModel(queueModel);
	//queueTable->hideColumn(4);
	queueTable->setSortingEnabled(true);
    queueTable->horizontalHeader()->setHighlightSections(false);
    queueTable->horizontalHeader()->setStretchLastSection(true);
    queueTable->setColumnWidth(0, 300);

	//===== Actions =====
	setPriorityLowAction = new QAction(QIcon(":/ArpmanetDC/Resources/LowPriorityIcon.png"), tr("Low priority"), this);
	setPriorityNormalAction = new QAction(QIcon(":/ArpmanetDC/Resources/QueueIcon.png"), tr("Normal priority"), this);
	setPriorityHighAction = new QAction(QIcon(":/ArpmanetDC/Resources/HighPriorityIcon.png"), tr("High priority"), this);
	deleteAction = new QAction(QIcon(":/ArpmanetDC/Resources/RemoveIcon.png"), tr("Delete"), this);
	searchForAlternatesAction = new QAction(QIcon(":/ArpmanetDC/Resources/SearchIcon.png"), tr("Search for alternates"), this);

    //===== Menus =====
    setPriorityMenu = new QMenu(tr("Set priority"), pParent);
	setPriorityMenu->addAction(setPriorityHighAction);
	setPriorityMenu->addAction(setPriorityNormalAction);
	setPriorityMenu->addAction(setPriorityLowAction);

	queueMenu = new QMenu(pParent);
	//queueMenu->addAction(searchForAlternatesAction);
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
	connect(searchForAlternatesAction, SIGNAL(triggered()), this, SLOT(searchForAlternatesActionPressed()));
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
	    QString base32TTH = queueModel->data(queueModel->index(selectedIndex.row(), 4)).toString();

        QByteArray tthRoot;
        tthRoot.append(base32TTH);
        base32Decode(tthRoot);  
        
	    pParent->setQueuePriority(tthRoot, LowQueuePriority);
        queueModel->item(selectedIndex.row(), 3)->setText("Low");
    }
}

void DownloadQueueWidget::setPriorityNormalActionPressed()
{
	for (int i = 0; i < queueTable->selectionModel()->selectedRows().size(); i++)
    {
        //Get selected files
	    QModelIndex selectedIndex = queueTable->selectionModel()->selectedRows().at(i);//userListTable->selectionModel()->selection().indexes().first();
	    //Get TTH of file
	    QString base32TTH = queueModel->data(queueModel->index(selectedIndex.row(), 4)).toString();

        QByteArray tthRoot;
        tthRoot.append(base32TTH);
        base32Decode(tthRoot); 
        
	    pParent->setQueuePriority(tthRoot, NormalQueuePriority);
        queueModel->item(selectedIndex.row(), 3)->setText("Normal");
    }
}

void DownloadQueueWidget::setPriorityHighActionPressed()
{
	for (int i = 0; i < queueTable->selectionModel()->selectedRows().size(); i++)
    {
        //Get selected files
	    QModelIndex selectedIndex = queueTable->selectionModel()->selectedRows().at(i);//userListTable->selectionModel()->selection().indexes().first();
	    //Get TTH of file
	    QString base32TTH = queueModel->data(queueModel->index(selectedIndex.row(), 4)).toString();

        QByteArray tthRoot;
        tthRoot.append(base32TTH);
        base32Decode(tthRoot);   
        
	    pParent->setQueuePriority(tthRoot, HighQueuePriority);
        queueModel->item(selectedIndex.row(), 3)->setText("High");
    }
}

void DownloadQueueWidget::deleteActionPressed()
{
	for (int i = 0; i < queueTable->selectionModel()->selectedRows().size(); i++)
    {
        //Get selected files
	    QModelIndex selectedIndex = queueTable->selectionModel()->selectedRows().at(i);//userListTable->selectionModel()->selection().indexes().first();
	    //Get TTH of file
	    QString base32TTH = queueModel->data(queueModel->index(selectedIndex.row(), 4)).toString();

        //Remove that row
        queueModel->removeRow(selectedIndex.row());

        QByteArray tthRoot;
        tthRoot.append(base32TTH);
        base32Decode(tthRoot);   
        
        //TODO: Remove entry from list

	    pParent->deleteFromQueue(tthRoot);
    }
}

void DownloadQueueWidget::searchForAlternatesActionPressed()
{
    //Not implemented - seems kinda silly to use the download queue to search for alternates? Will remove later
    return;

	for (int i = 0; i < queueTable->selectionModel()->selectedRows().size(); i++)
    {
        //Get selected files
	    QModelIndex selectedIndex = queueTable->selectionModel()->selectedRows().at(i);//userListTable->selectionModel()->selection().indexes().first();
	    //Get TTH of file
	    QString base32TTH = queueModel->data(queueModel->index(selectedIndex.row(), 4)).toString();

        QByteArray *tthRoot = new QByteArray();
        tthRoot->append(base32TTH);
        base32Decode(*tthRoot);  

        //pParent->searchForAlternates(tthRoot);
    }
}

//Queue list has been received
void DownloadQueueWidget::loadQueueList()
{
	//Remove all rows
	queueModel->removeRows(0, queueModel->rowCount());

    QList<QueueStruct> list = pQueueList->values();
	for (int i = 0; i < list.size(); i++)
	{
		queueModel->appendRow(new QStandardItem());
        queueModel->setItem(i, 0, new CStandardItem(CStandardItem::CaseInsensitiveTextType, list.at(i).fileName));
		queueModel->setItem(i, 1, new CStandardItem(CStandardItem::CaseInsensitiveTextType, list.at(i).filePath));
        queueModel->setItem(i, 2, new CStandardItem(CStandardItem::SizeType, bytesToSize(list.at(i).fileSize)));

		QString priorityStr;
		switch (list.at(i).priority)
		{
		case LowQueuePriority:
			priorityStr = "Low";
			break;
		case NormalQueuePriority:
			priorityStr = "Normal";
			break;
		case HighQueuePriority:
			priorityStr = "High";
			break;
		}

        queueModel->setItem(i, 3, new CStandardItem(CStandardItem::PriorityType, priorityStr));
        QByteArray tthBase32(list.at(i).tthRoot->data());
        base32Encode(tthBase32);

		queueModel->setItem(i, 4, new QStandardItem(tthBase32.data()));
	}
}

void DownloadQueueWidget::addQueuedDownload(QueueStruct file)
{
	queueModel->appendRow(new QStandardItem());
	queueModel->setItem(queueModel->rowCount()-1, 0, new CStandardItem(CStandardItem::CaseInsensitiveTextType, file.fileName));
    queueModel->setItem(queueModel->rowCount()-1, 1, new CStandardItem(CStandardItem::CaseInsensitiveTextType, file.filePath));
    queueModel->setItem(queueModel->rowCount()-1, 2, new CStandardItem(CStandardItem::SizeType, bytesToSize(file.fileSize)));

	QString priorityStr;
	switch (file.priority)
	{
	case LowQueuePriority:
		priorityStr = "Low";
		break;
	case NormalQueuePriority:
		priorityStr = "Normal";
		break;
	case HighQueuePriority:
		priorityStr = "High";
		break;
	}

    queueModel->setItem(queueModel->rowCount()-1, 3, new CStandardItem(CStandardItem::PriorityType, priorityStr));
    QByteArray tthBase32(file.tthRoot->data());
    base32Encode(tthBase32);
	queueModel->setItem(queueModel->rowCount()-1, 4, new QStandardItem(tthBase32.data()));
}

QWidget *DownloadQueueWidget::widget()
{
	//TODO: Return widget containing all search widgets
	return pWidget;
}
