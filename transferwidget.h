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

public slots:
    //Remove an entry from the list
    void removeTransferEntry(QByteArray tth, int type);

    //Return the status of transfers
    void returnGlobalTransferStatus(QList<TransferItemStatus> status);

signals:
    //Request the status of transfers
    void requestGlobalTransferStatus();

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
    QString bitmapString(quint8 transferID, quint8 updateID, int progress, QByteArray bitmap);
    int typeFromString(QString typeStr);
    QString segmentStatusString(SegmentStatusStruct s);

    quint8 currentUpdateID;
    quint8 transferID;

    QHash<QByteArray, quint8> pIDHash;

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