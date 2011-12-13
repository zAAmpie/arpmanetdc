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

    loadList();
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
    finishedTable->setWordWrap(false);
	finishedTable->verticalHeader()->hide();
	finishedTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    //finishedTable->setItemDelegate(new HTMLDelegate(finishedTable));
	finishedTable->setContextMenuPolicy(Qt::CustomContextMenu);
    finishedTable->horizontalHeader()->setHighlightSections(false);
    finishedTable->horizontalHeader()->setStretchLastSection(true);

	//Model
	finishedModel = new QStandardItemModel(0, 5);
	finishedModel->setHeaderData(0, Qt::Horizontal, tr("Filename"));
	finishedModel->setHeaderData(1, Qt::Horizontal, tr("Path"));
	finishedModel->setHeaderData(2, Qt::Horizontal, tr("Size"));
    finishedModel->setHeaderData(3, Qt::Horizontal, tr("Downloaded date"));
	finishedModel->setHeaderData(4, Qt::Horizontal, tr("TTH"));
	
	//Set model
	finishedTable->setModel(finishedModel);
	//queueTable->hideColumn(3);
	finishedTable->setSortingEnabled(true);
    finishedTable->setColumnWidth(0, 300);
    finishedTable->setColumnWidth(1, 200);
    finishedTable->setColumnWidth(2, 75);
    finishedTable->setColumnWidth(3, 150);

	//===== Actions =====
	openAction = new QAction(QIcon(":/ArpmanetDC/Resources/QueueIcon.png"), tr("Open file"), this);
	clearAction = new QAction(QIcon(":/ArpmanetDC/Resources/RemoveIcon.png"), tr("Clear list"), this);
    deleteAction = new QAction(QIcon(":/ArpmanetDC/Resources/RemoveIcon.png"), tr("Delete entry"), this);

    finishedMenu = new QMenu(pParent);
	finishedMenu->addAction(openAction);
    finishedMenu->addAction(deleteAction);
    finishedMenu->addSeparator();
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
    connect(deleteAction, SIGNAL(triggered()), this, SLOT(deleteActionPressed()));
}

void DownloadFinishedWidget::loadList()
{
	//Remove all rows
	finishedModel->removeRows(0, finishedModel->rowCount());

	//Populate model
	foreach (FinishedDownloadStruct file, *pFinishedList)
	{
        QList<QStandardItem *> row;
        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, file.fileName));
        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, file.filePath));
        row.append(new CStandardItem(CStandardItem::SizeType, bytesToSize(file.fileSize)));

        CStandardItem *cell = new CStandardItem(CStandardItem::DateType, QDateTime::fromString(file.downloadedDate,"dd-MM-yyyy HH:mm:ss:zzz").toString("dd/MM/yyyy HH:mm:ss"));
        cell->setFormat("dd/MM/yyyy HH:mm:ss");
        row.append(cell);

        QByteArray tth = *file.tthRoot;
        base32Encode(tth);

        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, tth.data()));
        finishedModel->appendRow(row);
	}

    resizeRowsToContents(finishedTable);
}

void DownloadFinishedWidget::showFinishedTableContextMenu(const QPoint &point)
{	
	//Show menu on right-click
	openAction->setVisible(!finishedTable->selectionModel()->selectedRows().isEmpty());
    deleteAction->setVisible(!finishedTable->selectionModel()->selectedRows().isEmpty());

    //Disable clear action when no items are in the model
    clearAction->setEnabled(finishedModel->rowCount() != 0);

	QPoint globalPos = finishedTable->viewport()->mapToGlobal(point);

	finishedMenu->popup(globalPos);
}

//Actions
void DownloadFinishedWidget::clearActionPressed()
{
	finishedModel->removeRows(0, finishedModel->rowCount());

    pParent->clearFinishedDownloadList();
	//emit clearFinishedList();
}

void DownloadFinishedWidget::openActionPressed()
{
    while (!finishedTable->selectionModel()->selectedRows().isEmpty())
    {
        QModelIndex selectedIndex = finishedTable->selectionModel()->selectedRows().first();
	    
        //Get path of first file in the list
	    QString filePath = finishedModel->itemFromIndex(finishedModel->index(selectedIndex.row(), 1))->text();
        
        //Get filename
        QString fileName = finishedModel->itemFromIndex(finishedModel->index(selectedIndex.row(), 0))->text();

        QFileInfo fi(filePath + fileName);
        if (fi.exists() && fi.isFile())
        {
	        //"Start" the file
	        QProcess::startDetached(filePath + fileName);

            //Deselect to avoid infinite loop
            finishedTable->selectionModel()->select(QItemSelection(selectedIndex, finishedModel->index(selectedIndex.row(), 4)), QItemSelectionModel::Deselect);
        }
        else
        {
            //Show messagebox
            QMessageBox msgBox((QWidget*)pParent);
            msgBox.setWindowTitle(tr("ArpmanetDC"));
            msgBox.setText(tr("The following file does not exist at the target location:<p><b>%1</b></p><br/>It may have been removed or moved to another location").arg(filePath+fileName));
            msgBox.setIcon(QMessageBox::Information);
            QPushButton *deleteButton = msgBox.addButton(tr("Delete"), QMessageBox::ActionRole); //Delete entry
            QPushButton *quitButton = msgBox.addButton(tr("Quit"), QMessageBox::ResetRole); //Quit from loop
            QPushButton *cancelButton = msgBox.addButton(tr("Continue"), QMessageBox::RejectRole); //Cancel
            msgBox.setDefaultButton(cancelButton);
                        
            msgBox.exec();

            if (msgBox.clickedButton() == deleteButton)
            {
                //Delete entry
	            QString tth = finishedModel->itemFromIndex(finishedModel->index(selectedIndex.row(), 4))->text();

                QByteArray base32TTH;
                base32TTH.append(tth);
                base32Decode(base32TTH);
                
                //Remove entry from db
                pParent->deleteFinishedDownload(base32TTH);

                //Remove entry from model
                finishedModel->removeRow(selectedIndex.row());

            }
            else if (msgBox.clickedButton() == quitButton)
            {
                //Break from loop
                break;
            }
            else
            {
                //Otherwise deselect to avoid infinite loop
                finishedTable->selectionModel()->select(QItemSelection(selectedIndex, finishedModel->index(selectedIndex.row(), 4)), QItemSelectionModel::Deselect);
            }
        }
    }
}

void DownloadFinishedWidget::deleteActionPressed()
{
    while (!finishedTable->selectionModel()->selectedRows().isEmpty())
    {
        QModelIndex selectedIndex = finishedTable->selectionModel()->selectedRows().first();
	    
        //Get tth of first file in the list
	    QString tth = finishedModel->itemFromIndex(finishedModel->index(selectedIndex.row(), 4))->text();

        QByteArray base32TTH;
        base32TTH.append(tth);
        base32Decode(base32TTH);
        
        //Remove entry from db
        pParent->deleteFinishedDownload(base32TTH);

        //Remove entry from model
        finishedModel->removeRow(selectedIndex.row());
    }
}

//Add an entry
void DownloadFinishedWidget::addFinishedDownload(FinishedDownloadStruct file)
{
	QList<QStandardItem *> row;
    row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, file.fileName));
    row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, file.filePath));
    row.append(new CStandardItem(CStandardItem::SizeType, bytesToSize(file.fileSize)));

    CStandardItem *cell = new CStandardItem(CStandardItem::DateType, QDateTime::fromString(file.downloadedDate,"dd-MM-yyyy HH:mm:ss:zzz").toString("dd/MM/yyyy HH:mm:ss"));
    cell->setFormat("dd/MM/yyyy HH:mm:ss");
    row.append(cell);

    QByteArray tth = *file.tthRoot;
    base32Encode(tth);

    row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, tth.data()));
    finishedModel->appendRow(row);

    //Adjust row height
    resizeRowsToContents(finishedTable);
}

QWidget *DownloadFinishedWidget::widget()
{
	//TODO: Return widget containing all search widgets
	return pWidget;
}
