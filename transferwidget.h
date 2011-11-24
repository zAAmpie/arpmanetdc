#ifndef TRANSFERWIDGET_H
#define TRANSFERWIDGET_H

#include <QtGui>

class ArpmanetDC;

//Displays help
class TransferWidget : public QObject
{
	Q_OBJECT

public:
	TransferWidget(ArpmanetDC *parent);
	~TransferWidget();

	//Get the encapsulating widget
	QWidget *widget();

private slots:
    //Right-click menu
    void showTransferListContextMenu(const QPoint&);

    //Actions
    void deleteActionPressed();

private:
	//Functions
	void createWidgets();
	void placeWidgets();
	void connectWidgets();

	//Objects
	QWidget *pWidget;
	ArpmanetDC *pParent;

    //Actions
    QAction *deleteAction;

    //Tables and models
	QTableView *transferListTable;
	QStandardItemModel *transferListModel;
	QSortFilterProxyModel *transferSortProxy;
};

#endif