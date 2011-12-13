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

#ifndef DOWNLOADQUEUEWIDGET_H
#define DOWNLOADQUEUEWIDGET_H

#include <QtGui>
#include <QHostAddress>

class ArpmanetDC;
class ShareSearch;

enum QueuePriority {LowQueuePriority=3, NormalQueuePriority=2, HighQueuePriority=1};

struct QueueStruct
{
	QString fileName;
	QString filePath;
	qint64 fileSize;
	QueuePriority priority;
	QByteArray *tthRoot;
    QHostAddress fileHost;
    bool operator==(const QueueStruct &other) const { return tthRoot == other.tthRoot; }
};

//Class encapsulating all widgets/signals for search tab
class DownloadQueueWidget : public QObject
{
	Q_OBJECT

public:
	DownloadQueueWidget(QHash<QByteArray, QueueStruct> *queueList, ShareSearch *share, ArpmanetDC *parent);
	~DownloadQueueWidget();

	//Get the encapsulating widget
	QWidget *widget();

public slots:
    //Public slots
    
    //Add a new queued download
	void addQueuedDownload(QueueStruct file);

    //Remove from queue
    void removeQueuedDownload(QByteArray tth);

    //Set a download busy (i.e. the download started)
    void setQueuedDownloadBusy(QByteArray tth);

private slots:
	//Slots
	void showQueueTableContextMenu(const QPoint &);

	//Actions
	void setPriorityLowActionPressed();
	void setPriorityNormalActionPressed();
	void setPriorityHighActionPressed();
	void deleteActionPressed();

signals:
	//Signals
	void setPriority(QByteArray tthRoot, QueuePriority priority);
	void deleteFromQueue(QByteArray tthRoot);

	//Signals for the queue list from the database
	void requestQueueList();

private:
	//Functions
	void createWidgets();
	void placeWidgets();
	void connectWidgets();

    //Queue list has been received
	void loadQueueList();

	//Objects
	QWidget *pWidget;
	ArpmanetDC *pParent;
    ShareSearch *pShare;

	QHash<QByteArray, QueueStruct> *pQueueList; //Link to main GUIs queue

    //Icons
    QIcon lowPriorityIcon, normalPriorityIcon, highPriorityIcon, queuedIcon, busyIcon;
	//GUI
	QTableView *queueTable;
	QStandardItemModel *queueModel;

    QMenu *queueMenu, *setPriorityMenu;

	QAction *setPriorityLowAction, *setPriorityNormalAction, *setPriorityHighAction;
	QAction *deleteAction;
};

#endif
