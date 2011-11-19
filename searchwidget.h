#ifndef SEARCHWIDGET_H
#define SEARCHWIDGET_H

#include <QtGui>
#include "customtableitems.h"

class ArpmanetDC;

//Class encapsulating all widgets/signals for search tab
class SearchWidget : public QObject
{
	Q_OBJECT

public:
	SearchWidget(ArpmanetDC *parent);
	~SearchWidget();

	//Get the encapsulating widget
	QWidget *widget();
	quint64 id();

public slots:
	//Populate search results
	void addSearchResult(QString);

private slots:
	//Search button pressed
	void searchPressed();

signals:
	//Search for string
	void search(quint64 id, QString search, SearchWidget *sWidget);

private:
	//Functions
	void createWidgets();
	void placeWidgets();
	void connectWidgets();

	QByteArray idGenerator();

	//Objects
	QWidget *pWidget;
	ArpmanetDC *pParent;

	//Parameters
	quint64 pID;
	static quint64 staticID;


	//===== Main GUI elements =====

	//Search
	QLineEdit *searchLineEdit;
	QPushButton *searchButton;
	QProgressBar *searchProgress;

	//Results
	QTableView *resultsTable;
	QStandardItemModel *resultsModel;

};

#endif