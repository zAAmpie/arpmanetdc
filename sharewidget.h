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

#ifndef SHAREWIDGET_H
#define SHAREWIDGET_H

#include <QtGui>
#include "checkableproxymodel.h"
#include "sharesearch.h"

class ArpmanetDC;

//Class encapsulating all widgets/signals for search tab
class ShareWidget : public QObject
{
	Q_OBJECT

public:
	ShareWidget(ShareSearch *share, ArpmanetDC *parent);
	~ShareWidget();

	//Get the encapsulating widget
	QWidget *widget();


private slots:
	//Slots
	void selectedItemsChanged();

	void saveSharePressed();

	void refreshButtonPressed();

	void changeRoot(QString path);

    void pathLoaded(QString path);

signals:
	//Signals
	void saveButtonPressed();

    //Private signal - update shares
    void updateShares(QList<QDir> *list);

	//Update shares without saving
	void updateShares();

private:
	//Functions
	void createWidgets();
	void placeWidgets();
	void connectWidgets();

	//Objects
	QWidget *pWidget;
	ArpmanetDC *pParent;
	ShareSearch *pShare;

	//Params
	QList<QString> pSharesList;

    //Loading paths
    QList<QString> pLoadingPaths;
    bool finishedLoading;

	//GUI elements
	QTreeView *fileTree;
	QFileSystemModel *fileModel;
	CheckableProxyModel *checkProxyModel;

    QLabel *busyLabel;

	QPushButton *saveButton, *refreshButton;

};

#endif