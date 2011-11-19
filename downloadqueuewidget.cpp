#include "downloadqueuewidget.h"
#include "customtableitems.h"
#include "arpmanetdc.h"

DownloadQueueWidget::DownloadQueueWidget(QList<QueueStruct> *queueList, ArpmanetDC *parent)
{
	//Constructor
	pQueueList = queueList;
	pParent = parent;

	createWidgets();
	placeWidgets();
	connectWidgets();

	//Populate model
	//queueModel->clear();
	for (int i = 0; i < pQueueList->size(); i++)
	{
		queueModel->appendRow(new QStandardItem());
		queueModel->setItem(i, 0, new QStandardItem(pQueueList->at(i).fileName));
		queueModel->setItem(i, 1, new QStandardItem(pQueueList->at(i).filePath));
		queueModel->setItem(i, 2, new QStandardItem(tr("%1").arg(pQueueList->at(i).fileSize)));

		QString priorityStr;
		switch (pQueueList->at(i).priority)
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

		queueModel->setItem(i, 3, new QStandardItem(priorityStr));
		queueModel->setItem(i, 4, new QStandardItem(pQueueList->at(i).tthRoot->toBase64().data()));
	}
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
	queueTable->setItemDelegate(new HTMLDelegate(queueTable));
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

	//===== Actions =====
	setPriorityLowAction = new QAction(QIcon(":/ArpmanetDC/Resources/LowPriorityIcon.png"), tr("Low priority"), this);
	setPriorityNormalAction = new QAction(QIcon(":/ArpmanetDC/Resources/QueueIcon.png"), tr("Normal priority"), this);
	setPriorityHighAction = new QAction(QIcon(":/ArpmanetDC/Resources/HighPriorityIcon.png"), tr("High priority"), this);
	deleteAction = new QAction(QIcon(":/ArpmanetDC/Resources/RemoveIcon.png"), tr("Delete"), this);
	searchForAlternatesAction = new QAction(QIcon(":/ArpmanetDC/Resources/SearchIcon.png"), tr("Search for alternates"), this);
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

	QMenu *setPriorityMenu = new QMenu(tr("Set priority"), pParent);
	setPriorityMenu->addAction(setPriorityHighAction);
	setPriorityMenu->addAction(setPriorityNormalAction);
	setPriorityMenu->addAction(setPriorityLowAction);

	QMenu *queueMenu = new QMenu(pParent);
	queueMenu->addAction(searchForAlternatesAction);
	queueMenu->addAction(deleteAction);
	queueMenu->addSeparator();
	queueMenu->addMenu(setPriorityMenu);

	queueMenu->popup(globalPos);
}

//Actions
void DownloadQueueWidget::setPriorityLowActionPressed()
{
	QByteArray *tthRoot = new QByteArray();

	emit setPriority(tthRoot, LowQueuePriority);
}

void DownloadQueueWidget::setPriorityNormalActionPressed()
{
	QByteArray *tthRoot = new QByteArray();

	emit setPriority(tthRoot, NormalQueuePriority);
}

void DownloadQueueWidget::setPriorityHighActionPressed()
{
	QByteArray *tthRoot = new QByteArray();

	emit setPriority(tthRoot, HighQueuePriority);
}

void DownloadQueueWidget::deleteActionPressed()
{
	QByteArray *tthRoot = new QByteArray();

	emit deleteFromQueue(tthRoot);
}

void DownloadQueueWidget::searchForAlternatesActionPressed()
{
	QByteArray *tthRoot = new QByteArray();

	emit searchForAlternates(tthRoot);
}

QWidget *DownloadQueueWidget::widget()
{
	//TODO: Return widget containing all search widgets
	return pWidget;
}

QList<QueueStruct> *DownloadQueueWidget::queueList()
{
	return pQueueList;
}

void DownloadQueueWidget::setQueueList(QList<QueueStruct> *list)
{
	delete pQueueList;
	pQueueList = list;
}