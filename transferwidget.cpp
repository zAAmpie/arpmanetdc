#include "transferwidget.h"
#include "transfermanager.h"
#include "arpmanetdc.h"

TransferWidget::TransferWidget(TransferManager *transferManager, ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;
    pTransferManager = transferManager;

	createWidgets();
	placeWidgets();
	connectWidgets();

    //Update status every second
    updateStatusTimer = new QTimer();
    connect(updateStatusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    updateStatusTimer->start(1000);
}

TransferWidget::~TransferWidget()
{
	//Destructor
}

void TransferWidget::createWidgets()
{
	//===== Transfer list =====
	//Model
	transferListModel = new QStandardItemModel(0,6);
	transferListModel->setHeaderData(0, Qt::Horizontal, tr("Type"));
    transferListModel->setHeaderData(1, Qt::Horizontal, tr("Progress"));
    transferListModel->setHeaderData(2, Qt::Horizontal, tr("Speed"));
    transferListModel->setHeaderData(3, Qt::Horizontal, tr("Filename"));
    transferListModel->setHeaderData(4, Qt::Horizontal, tr("Status"));
    transferListModel->setHeaderData(5, Qt::Horizontal, tr("TTH Root"));

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
	//transferListTable->setItemDelegate(new HTMLDelegate(transferListTable));

    //Action
    deleteAction = new QAction(QIcon(":/ArpmanetDC/Resources/RemoveIcon.png"), tr("Delete transfer"), this);

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
    //TODO: signal?
}

//Update status
void TransferWidget::updateStatus()
{
    //Get status from transfer manager
    QList<TransferItemStatus> status = pTransferManager->getGlobalTransferStatus();

    for (int i = 0; i < status.size(); i++)
    {
        //Base32 encode the tth
        QByteArray base32TTH(status.at(i).TTH);
        base32Encode(base32TTH);

        //Check if entry exists
        QList<QStandardItem *> findResults = transferListModel->findItems(base32TTH.data(), Qt::MatchExactly, 5);

        //If new transfer - add
        if (findResults.isEmpty())
        {
            QList<QStandardItem *> row;
            row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, typeString(status.at(i).transferStatus)));
            row.append(new CStandardItem(CStandardItem::IntegerType, tr("%1").arg(status.at(i).transferProgress)));
            row.append(new CStandardItem(CStandardItem::RateType, bytesToRate(status.at(i).transferRate)));
            row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, status.at(i).filePathName));
            row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, stateString(status.at(i).transferStatus)));
            row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, base32TTH.data()));
            transferListModel->appendRow(row);
        }
        //If existing transfer - update
        else
        {
            QStandardItem *item;
            item = transferListModel->itemFromIndex(transferListModel->index(findResults.first()->row(), 0));
            item->setText(typeString(status.at(i).transferStatus));
            item = transferListModel->itemFromIndex(transferListModel->index(findResults.first()->row(), 1));
            item->setText(tr("%1").arg(status.at(i).transferProgress));
            item = transferListModel->itemFromIndex(transferListModel->index(findResults.first()->row(), 2));
            item->setText(bytesToRate(status.at(i).transferRate));
            item = transferListModel->itemFromIndex(transferListModel->index(findResults.first()->row(), 3));
            item->setText(status.at(i).filePathName);
            item = transferListModel->itemFromIndex(transferListModel->index(findResults.first()->row(), 4));
            item->setText(stateString(status.at(i).transferStatus));
            item = transferListModel->itemFromIndex(transferListModel->index(findResults.first()->row(), 5));
            item->setText(base32TTH.data());
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

QWidget *TransferWidget::widget()
{
	return pWidget;
}
