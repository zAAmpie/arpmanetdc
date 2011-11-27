#ifndef SEARCHWIDGET_H
#define SEARCHWIDGET_H

#include <QtGui>
#include "customtableitems.h"
#include "sharesearch.h"
#include "util.h"
#include "resourceextractor.h"

class ArpmanetDC;
class TransferManager;

//Class encapsulating all widgets/signals for search tab
class SearchWidget : public QObject
{
	Q_OBJECT

public:
	SearchWidget(ResourceExtractor *mappedIconList, TransferManager *transferManager, ArpmanetDC *parent);
    //Overloaded constructor to automatically search for a string
    SearchWidget(ResourceExtractor *mappedIconList, TransferManager *transferManager, QString startupSearchString, ArpmanetDC *parent);
	~SearchWidget();

	//Get the encapsulating widget
	QWidget *widget();
	quint64 id();

public slots:
	//Populate search results
	void addSearchResult(QHostAddress sender, QByteArray cid, QByteArray result);

    //Search button pressed
    void searchPressed();
private slots:
    //Right-click menu
    void showContextMenu(const QPoint&);

    //Actions
    void downloadActionPressed();
    void downloadToActionPressed();

    //Sort results
    void sortTimeout();

    //Stop the progress thingy after x seconds
    void stopProgress();

signals:
	//Search for string
	void search(quint64 id, QString searchStr, QByteArray searchPacket, SearchWidget *sWidget);

private:
	//Functions
	void createWidgets();
	void placeWidgets();
	void connectWidgets();

	QByteArray idGenerator();

	//Objects
	QWidget *pWidget;
	ArpmanetDC *pParent;
    ResourceExtractor *pIconList;
    TransferManager *pTransferManager;

    QTimer *sortTimer;
    bool sortDue;

	//Parameters
	quint64 pID;
	static quint64 staticID;


	//===== Main GUI elements =====

    //Menu
    QMenu *resultsMenu;

    //Actions
    QAction *downloadAction, *downloadToAction;

	//Search
	QLineEdit *searchLineEdit;
    QLineEdit *majorVersionLineEdit, *minorVersionLineEdit;

	QPushButton *searchButton;
	QProgressBar *searchProgress;

	//Results
	QTreeView *resultsTable;
	QStandardItemModel *resultsModel;
    QStandardItem *parentItem;

};

#endif