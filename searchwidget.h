/* This file is part of ArpmanetDC. Copyright (C) 2012
 * Source code can be found at http://code.google.com/p/arpmanetdc/
 * 
 * ArpmanetDC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ArpmanetDC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with ArpmanetDC.  If not, see <http://www.gnu.org/licenses/>.
 */

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
	SearchWidget(QCompleter *completer, ResourceExtractor *mappedIconList, TransferManager *transferManager, ArpmanetDC *parent);
    //Overloaded constructor to automatically search for a string
    SearchWidget(QCompleter *completer, ResourceExtractor *mappedIconList, TransferManager *transferManager, QString startupSearchString, ArpmanetDC *parent);
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
    void calculateMagnetActionPressed();
    
    //User double clicked on a result
    void resultDoubleClicked(QModelIndex index);

    //Sort results
    void sortTimeout();

    //Stop the progress thingy after x seconds
    void stopProgress();

signals:
	//Search for string
	void search(quint64 id, QString searchStr, QByteArray searchPacket, SearchWidget *sWidget);

    //Queue download
    void queueDownload(int priority, QByteArray tth, QString finalPath, quint64 fileSize, QHostAddress senderIP);

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

    QCompleter *pCompleter;

    QTimer *sortTimer;
    bool sortDue;

	//Parameters
	quint64 pID;
	static quint64 staticID;
    QByteArray ownCID;

    quint32 totalResultCount;
    quint32 uniqueResultCount;

	//===== Main GUI elements =====

    //Menu
    QMenu *resultsMenu;

    //Actions
    QAction *downloadAction, *downloadToAction, *calculateMagnetAction;

	//Search
	QLineEdit *searchLineEdit;
    QLineEdit *majorVersionLineEdit, *minorVersionLineEdit;

	QPushButton *searchButton;
	QProgressBar *searchProgress;

    //Results
    QLabel *resultNumberLabel;
	QTreeView *resultsTable;
	QStandardItemModel *resultsModel;
    QStandardItem *parentItem;

    QMultiHash<QString, QString> resultsHash;

};

#endif