#include "downloadfinishedwidget.h"
#include "customtableitems.h"
#include "arpmanetdc.h"

DownloadFinishedWidget::DownloadFinishedWidget(QHash<QByteArray, FinishedDownloadStruct> *finishedList, ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;
    pFinishedList = finishedList;

	createWidgets();
	placeWidgets();
	connectWidgets();
}

DownloadFinishedWidget::~DownloadFinishedWidget()
{
	//Destructor
}

void DownloadFinishedWidget::createWidgets()
{
	//Table View
	finishedTable = new QTableView();
	finishedTable->setShowGrid(false);
	finishedTable->setGridStyle(Qt::DotLine);
	finishedTable->verticalHeader()->hide();
	finishedTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	finishedTable->setItemDelegate(new HTMLDelegate(finishedTable));
	finishedTable->setContextMenuPolicy(Qt::CustomContextMenu);
    finishedTable->horizontalHeader()->setHighlightSections(false);

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

    finishedMenu = new QMenu(pParent);
	finishedMenu->addAction(openAction);
	finishedMenu->addAction(clearAction);
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

void DownloadFinishedWidget::loadList()
{
	//Remove all rows
	finishedModel->removeRows(0, finishedModel->rowCount());

	//Populate model
	foreach (FinishedDownloadStruct file, *pFinishedList)
	{
        QList<CStandardItem *> row;
        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, file.fileName));
        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, file.filePath));
        row.append(new CStandardItem(CStandardItem::SizeType, bytesToSize(file.fileSize)));

        QByteArray tth = *file.tthRoot;
        base32Encode(tth);

        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, tth.data()));
        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, file.downloadedDate));
	}

    resizeRowsToContents(finishedTable);
}

void DownloadFinishedWidget::showFinishedTableContextMenu(const QPoint &point)
{	
	//Show menu on right-click
	if (finishedTable->selectionModel()->selectedRows().isEmpty())
		return;

	QPoint globalPos = finishedTable->viewport()->mapToGlobal(point);

	finishedMenu->popup(globalPos);
}

//Actions
void DownloadFinishedWidget::clearActionPressed()
{
	finishedModel->clear();

    pParent->clearFinishedDownloadList();
	//emit clearFinishedList();
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



//Add an entry
void DownloadFinishedWidget::addFinishedDownload(FinishedDownloadStruct file)
{
	finishedModel->appendRow(new QStandardItem());
	finishedModel->setItem(finishedModel->rowCount()-1, 0, new QStandardItem(file.fileName));
	finishedModel->setItem(finishedModel->rowCount()-1, 1, new QStandardItem(file.filePath));
	finishedModel->setItem(finishedModel->rowCount()-1, 2, new QStandardItem(tr("%1").arg(file.fileSize)));
	finishedModel->setItem(finishedModel->rowCount()-1, 4, new QStandardItem(file.tthRoot->toBase64().data()));
	finishedModel->setItem(finishedModel->rowCount()-1, 5, new QStandardItem(file.downloadedDate));

    //Adjust row height
    resizeRowsToContents(finishedTable);
}

QWidget *DownloadFinishedWidget::widget()
{
	//TODO: Return widget containing all search widgets
	return pWidget;
}
