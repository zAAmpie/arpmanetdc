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
	queueTable->setShowGrid(false);
	queueTable->setGridStyle(Qt::DotLine);
	queueTable->verticalHeader()->hide();
	queueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	//queueTable->setItemDelegate(new HTMLDelegate(queueTable));
	queueTable->setContextMenuPolicy(Qt::CustomContextMenu);

	//Model
	queueModel = new QStandardItemModel(0, 6);
	queueModel->setHeaderData(0, Qt::Horizontal, tr("Filename"));
	queueModel->setHeaderData(1, Qt::Horizontal, tr("Path"));
	queueModel->setHeaderData(2, Qt::Horizontal, tr("Size"));
	queueModel->setHeaderData(3, Qt::Horizontal, tr("Priority"));
	queueModel->setHeaderData(4, Qt::Horizontal, tr("TTH"));
    queueModel->setHeaderData(5, Qt::Horizontal, tr("Host IP"));

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

	    pParent->deleteFromQueue(tthRoot);
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
		QList<QStandardItem *> row;
        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, list.at(i).fileName));
        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, list.at(i).filePath));
        row.append(new CStandardItem(CStandardItem::SizeType, bytesToSize(list.at(i).fileSize)));

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
        row.append(new CStandardItem(CStandardItem::PriorityType, priorityStr));
        
        QByteArray tthBase32(list.at(i).tthRoot->data(), list.at(i).tthRoot->size());
        base32Encode(tthBase32);
        row.append(new QStandardItem(tthBase32.data()));
        row.append(new QStandardItem(list.at(i).fileHost.toString()));
		
        queueModel->appendRow(row);
    }

    resizeRowsToContents(queueTable);
    queueTable->sortByColumn(0, Qt::AscendingOrder);
}

void DownloadQueueWidget::addQueuedDownload(QueueStruct file)
{
	QList<QStandardItem *> row;
    row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, file.fileName));
    row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, file.filePath));
    row.append(new CStandardItem(CStandardItem::SizeType, bytesToSize(file.fileSize)));

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
    row.append(new CStandardItem(CStandardItem::PriorityType, priorityStr));

    QByteArray tthBase32(file.tthRoot->data(), file.tthRoot->size());
    base32Encode(tthBase32);
    row.append(new QStandardItem(tthBase32.data()));
    row.append(new QStandardItem(file.fileHost.toString()));
	
    queueModel->appendRow(row); 

    resizeRowsToContents(queueTable);
}

QWidget *DownloadQueueWidget::widget()
{
	return pWidget;
}
