#include "downloadfinishedwidget.h"
#include "customtableitems.h"
#include "arpmanetdc.h"

DownloadFinishedWidget::DownloadFinishedWidget(ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;

	createWidgets();
	placeWidgets();
	connectWidgets();

	//Load list from database
	loadFinishedDownloads();
}

DownloadFinishedWidget::~DownloadFinishedWidget()
{
	//Destructor
}

void DownloadFinishedWidget::createWidgets()
{
	//Table View
	finishedTable = new QTableView();
	finishedTable->setShowGrid(true);
	finishedTable->setGridStyle(Qt::DotLine);
	finishedTable->verticalHeader()->hide();
	finishedTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	finishedTable->setItemDelegate(new HTMLDelegate(finishedTable));
	finishedTable->setContextMenuPolicy(Qt::CustomContextMenu);

	//Model
	finishedModel = new QStandardItemModel(0, 5);
	finishedModel->setHeaderData(0, Qt::Horizontal, tr("Filename"));
	finishedModel->setHeaderData(1, Qt::Horizontal, tr("Path"));
	finishedModel->setHeaderData(2, Qt::Horizontal, tr("Size"));
	finishedModel->setHeaderData(3, Qt::Horizontal, tr("TTH"));
	finishedModel->setHeaderData(4, Qt::Horizontal, tr("Downloaded date"));

	//Set model
	finishedTable->setModel(finishedModel);
	//queueTable->hideColumn(3);
	finishedTable->setSortingEnabled(true);

	//===== Actions =====
	openAction = new QAction(QIcon(":/ArpmanetDC/Resources/QueueIcon.png"), tr("Open file"), this);
	clearAction = new QAction(QIcon(":/ArpmanetDC/Resources/RemoveIcon.png"), tr("Clear list"), this);
}

void DownloadFinishedWidget::placeWidgets()
{
	pWidget = finishedTable;
}

void DownloadFinishedWidget::connectWidgets()
{
	connect(finishedTable, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showFinishedTableContextMenu(const QPoint&)));

	connect(openAction, SIGNAL(triggered()), this, SLOT(openActionPressed()));
	connect(clearAction, SIGNAL(triggered()), this, SLOT(clearActionPressed()));
}

void DownloadFinishedWidget::showFinishedTableContextMenu(const QPoint &point)
{	
	//Show menu on right-click
	if (finishedTable->selectionModel()->selectedRows().isEmpty())
		return;

	QPoint globalPos = finishedTable->viewport()->mapToGlobal(point);

	QMenu *finishedMenu = new QMenu(pParent);
	finishedMenu->addAction(openAction);
	finishedMenu->addAction(clearAction);

	finishedMenu->popup(globalPos);
}

//Actions
void DownloadFinishedWidget::clearActionPressed()
{
	pFinishedList->clear();
	finishedModel->removeRows(0, finishedModel->rowCount());

	emit clearFinishedList();
}

void DownloadFinishedWidget::openActionPressed()
{
	//Get selected files
	QModelIndex selectedIndex = finishedTable->selectionModel()->selectedRows().first();
	//Get path of first file in the list (there should really only be one)
	QString filePath = finishedModel->data(finishedModel->index(selectedIndex.row(), 1)).toString();

	//"Start" the file
	QProcess::startDetached(filePath);
}

void DownloadFinishedWidget::returnFinishedList(QList<FinishedDownloadStruct> *list)
{
	//List returned
	setFinishedList(list);

	//Remove all rows
	finishedModel->removeRows(0, finishedModel->rowCount());

	//Populate model
	for (int i = 0; i < pFinishedList->size(); i++)
	{
		finishedModel->appendRow(new QStandardItem());
		finishedModel->setItem(i, 0, new QStandardItem(pFinishedList->at(i).fileName));
		finishedModel->setItem(i, 1, new QStandardItem(pFinishedList->at(i).filePath));
		finishedModel->setItem(i, 2, new QStandardItem(tr("%1").arg(pFinishedList->at(i).fileSize)));
		finishedModel->setItem(i, 4, new QStandardItem(pFinishedList->at(i).tthRoot->toBase64().data()));
		finishedModel->setItem(i, 5, new QStandardItem(pFinishedList->at(i).downloadedDate));
	}
}

//Add an entry
void DownloadFinishedWidget::addFinishedDownload(FinishedDownloadStruct file)
{
	finishedModel->appendRow(new QStandardItem());
	finishedModel->setItem(finishedModel->rowCount()-1, 0, new QStandardItem(file.fileName));
	finishedModel->setItem(finishedModel->rowCount()-1, 1, new QStandardItem(file.filePath));
	finishedModel->setItem(finishedModel->rowCount()-1, 2, new QStandardItem(tr("%1").arg(file.fileSize)));
	finishedModel->setItem(finishedModel->rowCount()-1, 4, new QStandardItem(file.tthRoot->toBase64().data()));
	finishedModel->setItem(finishedModel->rowCount()-1, 5, new QStandardItem(file.downloadedDate));
}

void DownloadFinishedWidget::loadFinishedDownloads()
{
	emit requestFinishedList();
}


QWidget *DownloadFinishedWidget::widget()
{
	//TODO: Return widget containing all search widgets
	return pWidget;
}

QList<FinishedDownloadStruct> *DownloadFinishedWidget::finishedList()
{
	return pFinishedList;
}

void DownloadFinishedWidget::setFinishedList(QList<FinishedDownloadStruct> *list)
{
	if (pFinishedList != list)
		delete pFinishedList;
	pFinishedList = list;
}