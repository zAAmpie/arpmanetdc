#ifndef SEARCHWIDGET_H
#define SEARCHWIDGET_H

#include <QtGui>
#include "customtableitems.h"
#include "sharesearch.h"
#include "util.h"
#include "resourceextractor.h"

class ArpmanetDC;

//Class encapsulating all widgets/signals for search tab
class SearchWidget : public QObject
{
	Q_OBJECT

public:
	SearchWidget(ResourceExtractor *mappedIconList, ArpmanetDC *parent);
	~SearchWidget();

	//Get the encapsulating widget
	QWidget *widget();
	quint64 id();

public slots:
	//Populate search results
	void addSearchResult(QHostAddress sender, QByteArray cid, QByteArray result);

private slots:
	//Search button pressed
	void searchPressed();

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

    QIcon fileIcon(const QString &filename);

	//Objects
	QWidget *pWidget;
	ArpmanetDC *pParent;
    ResourceExtractor *pIconList;

    QTimer *sortTimer;
    bool sortDue;

	//Parameters
	quint64 pID;
	static quint64 staticID;


	//===== Main GUI elements =====

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