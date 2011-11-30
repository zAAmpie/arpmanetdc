#ifndef TRANSFERWIDGET_H
#define TRANSFERWIDGET_H

#include <QtGui>
#include "transfermanager.h"

class ArpmanetDC;

//Displays help
class TransferWidget : public QObject
{
	Q_OBJECT

public:
	TransferWidget(TransferManager *transferManager, ArpmanetDC *parent);
	~TransferWidget();

	//Get the encapsulating widget
	QWidget *widget();

    QHash<QByteArray, TransferItemStatus> *transferList() const;
    bool isBusy(QByteArray tth);

private slots:
    //Right-click menu
    void showTransferListContextMenu(const QPoint&);

    //Actions
    void deleteActionPressed();

    //Update status
    void updateStatus();

private:
	//Functions
	void createWidgets();
	void placeWidgets();
	void connectWidgets();

    //String conversion functions
    QString typeString(int type);
    QString stateString(int state);
    QString progressString(int state, int progress);

	//Objects
	QWidget *pWidget;
	ArpmanetDC *pParent;
    TransferManager *pTransferManager;

    //List of transfers
    QHash<QByteArray, TransferItemStatus> *pTransferList;

    //Update timer
    QTimer *updateStatusTimer;

    //Menu
    QMenu *transferListMenu;

    //Actions
    QAction *deleteAction;

    //Tables and models
	QTableView *transferListTable;
	QStandardItemModel *transferListModel;
	QSortFilterProxyModel *transferSortProxy;
};

#endif