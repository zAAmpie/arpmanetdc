#include "transferwidget.h"
#include "arpmanetdc.h"

TransferWidget::TransferWidget(ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;

	createWidgets();
	placeWidgets();
	connectWidgets();
}

TransferWidget::~TransferWidget()
{
	//Destructor
}

void TransferWidget::createWidgets()
{
	//===== Transfer list =====
	//Model
	transferListModel = new QStandardItemModel(0,1);
	transferListModel->setHeaderData(0, Qt::Horizontal, tr("Transfer"));

	//Table
	transferListTable = new QTableView((QWidget *)pParent);
	transferListTable->setContextMenuPolicy(Qt::CustomContextMenu);

	//Link table and model
	transferListTable->setModel(transferListModel);

	//Sizing
	transferListTable->horizontalHeader()->setResizeMode(QHeaderView::Interactive);

	//Style
	transferListTable->setShowGrid(true);
	transferListTable->setGridStyle(Qt::DotLine);
	transferListTable->verticalHeader()->hide();
	transferListTable->setItemDelegate(new HTMLDelegate(transferListTable));

    //Action
    deleteAction = new QAction(QIcon(":/ArpmanetDC/Resources/RemoveIcon.png"), tr("Delete transfer"), this);
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

	QMenu *transferListMenu = new QMenu((QWidget *)pParent);
	transferListMenu->addAction(deleteAction);

	transferListMenu->popup(globalPos);
}

void TransferWidget::deleteActionPressed()
{
    //TODO: signal?
}

QWidget *TransferWidget::widget()
{
	return pWidget;
}