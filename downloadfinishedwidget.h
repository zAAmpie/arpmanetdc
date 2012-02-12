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

#ifndef DownloadFinishedWidget_H
#define DownloadFinishedWidget_H

#include <QtGui>

class ArpmanetDC;

struct FinishedDownloadStruct
{
	QString fileName;
	QString filePath;
	qint64 fileSize;
	QByteArray tthRoot;
	QString downloadedDate;
};

//Class encapsulating all widgets/signals for search tab
class DownloadFinishedWidget : public QObject
{
	Q_OBJECT

public:
	DownloadFinishedWidget(QHash<QByteArray, FinishedDownloadStruct> *finishedList, ArpmanetDC *parent);
	~DownloadFinishedWidget();

	//Get the encapsulating widget
	QWidget *widget();

public slots:
	//Add an entry
	void addFinishedDownload(FinishedDownloadStruct file);

private slots:
	//Slots
	void showFinishedTableContextMenu(const QPoint &);
    void downloadDoubleClicked(QModelIndex index);

	//Actions
	void clearActionPressed();
	void openActionPressed();
    void deleteActionPressed();

signals:
	//Signals to clear the database list
	//void clearFinishedList();
	
private:
	//Functions
	void createWidgets();
	void placeWidgets();
	void connectWidgets();

    //Return requested list
	void loadList();

	//Objects
	QWidget *pWidget;
	ArpmanetDC *pParent;

	QHash<QByteArray, FinishedDownloadStruct> *pFinishedList;

	//GUI
	QTableView *finishedTable;
	QStandardItemModel *finishedModel;

    QMenu *finishedMenu;
	QAction *openAction, *clearAction, *deleteAction;
};

#endif