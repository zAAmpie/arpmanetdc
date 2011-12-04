#include "transferwidget.h"
#include "transfermanager.h"
#include "arpmanetdc.h"

TransferWidget::TransferWidget(TransferManager *transferManager, ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;
    pTransferManager = transferManager;
    pTransferList = new QHash<QByteArray, TransferItemStatus>();

	createWidgets();
	placeWidgets();
	connectWidgets();

    //Update status every second
    updateStatusTimer = new QTimer();
    connect(updateStatusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    updateStatusTimer->start(300);
}

TransferWidget::~TransferWidget()
{
	//Destructor
}

void TransferWidget::createWidgets()
{
	//===== Transfer list =====
	//Model
	transferListModel = new QStandardItemModel(0,8);
	transferListModel->setHeaderData(0, Qt::Horizontal, tr("Type"));
    transferListModel->setHeaderData(1, Qt::Horizontal, tr("Progress"));
    transferListModel->setHeaderData(2, Qt::Horizontal, tr("Speed"));
    transferListModel->setHeaderData(3, Qt::Horizontal, tr("Filename"));
    transferListModel->setHeaderData(4, Qt::Horizontal, tr("Size"));
    transferListModel->setHeaderData(5, Qt::Horizontal, tr("Status"));
    transferListModel->setHeaderData(6, Qt::Horizontal, tr("TTH Root"));
    transferListModel->setHeaderData(7, Qt::Horizontal, tr("Host"));

	//Table
	transferListTable = new QTableView((QWidget *)pParent);
	transferListTable->setContextMenuPolicy(Qt::CustomContextMenu);
    transferListTable->setSortingEnabled(true);
    transferListTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    transferListTable->horizontalHeader()->setStretchLastSection(true);

	//Link table and model
	transferListTable->setModel(transferListModel);

	//Sizing
	transferListTable->horizontalHeader()->setResizeMode(QHeaderView::Interactive);

	//Style
	transferListTable->setShowGrid(false);
	transferListTable->setGridStyle(Qt::DotLine);
	transferListTable->verticalHeader()->hide();
    transferListTable->horizontalHeader()->setHighlightSections(false);
    transferListTable->setColumnWidth(0, 75);
    transferListTable->setColumnWidth(2, 75);
    transferListTable->setColumnWidth(3, 300);
    transferListTable->setColumnWidth(4, 75);
    transferListTable->setWordWrap(false);
    //transferListTable->setItemDelegate(new HTMLDelegate(transferListTable));
    transferListTable->setItemDelegateForColumn(1, new ProgressDelegate());

    //Action
    deleteAction = new QAction(QIcon(":/ArpmanetDC/Resources/RemoveIcon.png"), tr("Stop transfer"), this);

    //Menu
    transferListMenu = new QMenu((QWidget *)pParent);
	transferListMenu->addAction(deleteAction);
}

void TransferWidget::placeWidgets()
{
    pWidget = transferListTable;
}

void TransferWidget::connectWidgets()
{
    //Context menus
    connect(transferListTable, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showTransferListContextMenu(const QPoint&)));

    //Actions
    connect(deleteAction, SIGNAL(triggered()), this, SLOT(deleteActionPressed()));
}

//Show right-click menu on transfer list
void TransferWidget::showTransferListContextMenu(const QPoint &pos)
{
    if (transferListTable->selectionModel()->selectedRows().size() == 0)
        return;

	QPoint globalPos = transferListTable->viewport()->mapToGlobal(pos);

	transferListMenu->popup(globalPos);
}

void TransferWidget::deleteActionPressed()
{
    QList<QModelIndex> selectedRows = transferListTable->selectionModel()->selectedRows();

    //Remove rows from parent queue
	for (int i = 0; i < transferListTable->selectionModel()->selectedRows().size(); i++)
    {
        //Get selected files
	    QModelIndex selectedIndex = transferListTable->selectionModel()->selectedRows().at(i);//userListTable->selectionModel()->selection().indexes().first();
	    //Get TTH of file
	    QString base32TTH = transferListModel->data(transferListModel->index(selectedIndex.row(), 6)).toString();
        
        QByteArray tthRoot;
        tthRoot.append(base32TTH);
        base32Decode(tthRoot);   

        //Get type of file
        QString typeStr = transferListModel->data(transferListModel->index(selectedIndex.row(), 0)).toString();

        //Get host address
        QHostAddress hostAddr = QHostAddress(transferListModel->data(transferListModel->index(selectedIndex.row(), 7)).toString());

	    pParent->removeTransfer(tthRoot, typeFromString(typeStr), hostAddr);
    }

    //Remove rows from model
    while (!transferListTable->selectionModel()->selectedRows().isEmpty())
    {
        transferListModel->removeRow(transferListTable->selectionModel()->selectedRows().first().row());
    }    
}

//Update status
void TransferWidget::updateStatus()
{
    //Get status from transfer manager
    QList<TransferItemStatus> status = pTransferManager->getGlobalTransferStatus();

    //Update local list
    pTransferList->clear();
    foreach (TransferItemStatus s, status)
        pTransferList->insert(s.TTH, s);

    for (int i = 0; i < status.size(); i++)
    {
        TransferItemStatus s = status.at(i);  

        //Base32 encode the tth
        QByteArray base32TTH(s.TTH);
        base32Encode(base32TTH);

        QueueStruct q = pParent->queueEntry(s.TTH);

        //Check if entry exists
        QList<QStandardItem *> findResults = transferListModel->findItems(base32TTH.data(), Qt::MatchExactly, 6);

        //If new transfer - add
        if (findResults.isEmpty())
        {
            QList<QStandardItem *> row;
            row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, typeString(s.transferType)));
            row.append(new CStandardItem(CStandardItem::ProgressType, progressString(s.transferType, s.transferProgress)));
            row.append(new CStandardItem(CStandardItem::RateType, bytesToRate(s.transferRate)));
            row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, s.filePathName.remove(pParent->downloadPath(), Qt::CaseInsensitive)));
            row.append(new CStandardItem(CStandardItem::SizeType, bytesToSize(q.fileSize)));
            row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, stateString(s.transferStatus)));
            row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, base32TTH.data()));
            row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, q.fileHost.toString()));
            transferListModel->appendRow(row);
        }
        //If existing transfer - update
        else
        {
            QStandardItem *item;
            item = transferListModel->itemFromIndex(transferListModel->index(findResults.first()->row(), 0));
            item->setText(typeString(s.transferType));
            item = transferListModel->itemFromIndex(transferListModel->index(findResults.first()->row(), 1));
            item->setText(progressString(s.transferType, s.transferProgress));
            item = transferListModel->itemFromIndex(transferListModel->index(findResults.first()->row(), 2));
            item->setText(bytesToRate(s.transferRate));
            item = transferListModel->itemFromIndex(transferListModel->index(findResults.first()->row(), 3));
            item->setText(s.filePathName.remove(pParent->downloadPath(), Qt::CaseInsensitive));
            item = transferListModel->itemFromIndex(transferListModel->index(findResults.first()->row(), 4));
            item->setText(bytesToSize(q.fileSize));
            item = transferListModel->itemFromIndex(transferListModel->index(findResults.first()->row(), 5));
            item->setText(stateString(s.transferStatus));
            item = transferListModel->itemFromIndex(transferListModel->index(findResults.first()->row(), 6));
            item->setText(base32TTH.data());
            item = transferListModel->itemFromIndex(transferListModel->index(findResults.first()->row(), 7));
            item->setText(q.fileHost.toString());
        }
    }

    //Adjust row height
    resizeRowsToContents(transferListTable);

    //Sort table
    int column = transferListTable->horizontalHeader()->sortIndicatorSection();
    Qt::SortOrder order = transferListTable->horizontalHeader()->sortIndicatorOrder();
    transferListTable->sortByColumn(column, order);
}

QString TransferWidget::typeString(int type)
{
     //Determine transfer type
    switch (type)
    {
        case TRANSFER_TYPE_DOWNLOAD:
            return "Download";
        case TRANSFER_TYPE_UPLOAD:
            return "Upload";
    }
    return "";
}

int TransferWidget::typeFromString(QString typeStr)
{
    //Determine transfer type
    if (typeStr == "Download")
        return TRANSFER_TYPE_DOWNLOAD;
    if (typeStr == "Upload")
        return TRANSFER_TYPE_UPLOAD;
    return -1;
}

QString TransferWidget::stateString(int state)
{
    //Determine status
    switch (state)
    {
        case TRANSFER_STATE_PAUSED:
            return "Paused";
        case TRANSFER_STATE_INITIALIZING:
            return "Initializing";
        case TRANSFER_STATE_RUNNING:
            return "Running";
        case TRANSFER_STATE_STALLED:
            return "Stalled";
        case TRANSFER_STATE_ABORTING:
            return "Aborting";
        case TRANSFER_STATE_FINISHED:
            return "Finished";
    }
    return "";
}

QString TransferWidget::progressString(int type, int progress)
{
    if (type == TRANSFER_TYPE_DOWNLOAD)
        return tr("D%1").arg(progress);
    else if (type == TRANSFER_TYPE_UPLOAD)
        return tr("U%1").arg(progress);

    return "";
}

QWidget *TransferWidget::widget()
{
	return pWidget;
}

QHash<QByteArray, TransferItemStatus> *TransferWidget::transferList() const
{
    return pTransferList;
}

bool TransferWidget::isBusy(QByteArray tth)
{
    return pTransferList->contains(tth);
}