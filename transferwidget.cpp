#include "transferwidget.h"
#include "transfermanager.h"
#include "arpmanetdc.h"

TransferWidget::TransferWidget(TransferManager *transferManager, ArpmanetDC *parent)
{
    //Constructor
    pParent = parent;
    pTransferManager = transferManager;
    pTransferList = new QHash<QByteArray, TransferItemStatus>();

    connect(this, SIGNAL(requestGlobalTransferStatus()), pTransferManager, SLOT(requestGlobalTransferStatus()), Qt::QueuedConnection);
    connect(pTransferManager, SIGNAL(returnGlobalTransferStatus(QList<TransferItemStatus>)), this, SLOT(returnGlobalTransferStatus(QList<TransferItemStatus>)), Qt::QueuedConnection);

    createWidgets();
    placeWidgets();
    connectWidgets();

    currentUpdateID = 0;
    transferID = 0;

    //Update status every 300ms
    updateStatusTimer = new QTimer();
    connect(updateStatusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    updateStatusTimer->start(300);
}

TransferWidget::~TransferWidget()
{
    //Destructor
}

void TransferWidget::createWidgets()
{
    //===== Transfer list =====
    //Model
    transferListModel = new QStandardItemModel(0,10);
    transferListModel->setHeaderData(0, Qt::Horizontal, tr("Type"));
    transferListModel->setHeaderData(1, Qt::Horizontal, tr("Progress"));
    transferListModel->setHeaderData(2, Qt::Horizontal, tr("Speed"));
    transferListModel->setHeaderData(3, Qt::Horizontal, tr("Filename"));
    transferListModel->setHeaderData(4, Qt::Horizontal, tr("Size"));
    transferListModel->setHeaderData(5, Qt::Horizontal, tr("Status"));
    transferListModel->setHeaderData(6, Qt::Horizontal, tr("Time"));
    transferListModel->setHeaderData(7, Qt::Horizontal, tr("Segments"));
    transferListModel->setHeaderData(8, Qt::Horizontal, tr("TTH Root"));
    transferListModel->setHeaderData(9, Qt::Horizontal, tr("Host"));

    //Table
    transferListTable = new QTableView((QWidget *)pParent);
    transferListTable->setContextMenuPolicy(Qt::CustomContextMenu);
    transferListTable->setSortingEnabled(true);
    transferListTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    transferListTable->horizontalHeader()->setStretchLastSection(true);

    //Link table and model
    transferListTable->setModel(transferListModel);

    //Sizing
    transferListTable->horizontalHeader()->setResizeMode(QHeaderView::Interactive);

    //Style
    transferListTable->setShowGrid(false);
    transferListTable->setGridStyle(Qt::DotLine);
    transferListTable->verticalHeader()->hide();
    transferListTable->horizontalHeader()->setHighlightSections(false);
    transferListTable->setColumnWidth(0, 75);
    transferListTable->setColumnWidth(1, 150);
    transferListTable->setColumnWidth(2, 75);
    transferListTable->setColumnWidth(3, 300);
    transferListTable->setColumnWidth(4, 75);
    transferListTable->setWordWrap(false);
    //transferListTable->setItemDelegate(new HTMLDelegate(transferListTable));
    //transferListTable->setItemDelegateForColumn(1, new ProgressDelegate());
    transferListTable->setItemDelegateForColumn(1, new BitmapDelegate());
    
    transferListTable->hideColumn(9);

    //Action
    deleteAction = new QAction(QIcon(":/ArpmanetDC/Resources/RemoveIcon.png"), tr("Stop transfer"), this);

    //Menu
    transferListMenu = new QMenu((QWidget *)pParent);
    transferListMenu->addAction(deleteAction);
}

void TransferWidget::placeWidgets()
{
    pWidget = transferListTable;
}

void TransferWidget::connectWidgets()
{
    //Context menus
    connect(transferListTable, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showTransferListContextMenu(const QPoint&)));

    //Actions
    connect(deleteAction, SIGNAL(triggered()), this, SLOT(deleteActionPressed()));
}

//Show right-click menu on transfer list
void TransferWidget::showTransferListContextMenu(const QPoint &pos)
{
    if (transferListTable->selectionModel()->selectedRows().size() == 0)
        return;

    QPoint globalPos = transferListTable->viewport()->mapToGlobal(pos);

    transferListMenu->popup(globalPos);
}

void TransferWidget::deleteActionPressed()
{
    QList<QModelIndex> selectedRows = transferListTable->selectionModel()->selectedRows();

    //Remove rows from parent queue
    for (int i = 0; i < transferListTable->selectionModel()->selectedRows().size(); i++)
    {
        //Get selected files
        QModelIndex selectedIndex = transferListTable->selectionModel()->selectedRows().at(i);//userListTable->selectionModel()->selection().indexes().first();
        //Get TTH of file
        QString base32TTH = transferListModel->data(transferListModel->index(selectedIndex.row(), 8)).toString();
        
        QByteArray tthRoot;
        tthRoot.append(base32TTH);
        base32Decode(tthRoot);   

        //Get type of file
        QString typeStr = transferListModel->data(transferListModel->index(selectedIndex.row(), 0)).toString();

        //Get host address
        QHostAddress hostAddr = QHostAddress(transferListModel->data(transferListModel->index(selectedIndex.row(), 9)).toString());

        pParent->removeTransfer(tthRoot, typeFromString(typeStr), hostAddr);
    }

    //Remove rows from model
    while (!transferListTable->selectionModel()->selectedRows().isEmpty())
    {
        transferListModel->removeRow(transferListTable->selectionModel()->selectedRows().first().row());
    }    
}

//Update status
void TransferWidget::updateStatus()
{
    //Get status from transfer manager
    emit requestGlobalTransferStatus();
}

//Return the status of transfers
void TransferWidget::returnGlobalTransferStatus(QList<TransferItemStatus> status)
{
    //Update local list
    pTransferList->clear();
    BitmapDelegate::clearRenderedPixmaps();
    foreach (TransferItemStatus s, status)
        pTransferList->insert(s.TTH, s);

    //Update counters
    qint64 pTotalDownloadPerSecond = 0;
    qint64 pTotalUploadPerSecond = 0;

    //Clear model
    //transferListModel->removeRows(0, transferListModel->rowCount());

    for (int i = 0; i < status.size(); i++)
    {
        TransferItemStatus s = status.at(i); 

        //Base32 encode the tth
        QByteArray base32TTH(s.TTH);
        base32Encode(base32TTH);

        QueueStruct q = pParent->queueEntry(s.TTH);

        //Determine the transfer rate per second from the interval period
        qint64 actualTransferRate = s.transferRate;
        actualTransferRate = (actualTransferRate * 1000) /  (qint64)updateStatusTimer->interval();

        //Get the download host from the queue, upload hosts are contained within the struct
        if (s.transferType == TRANSFER_TYPE_DOWNLOAD)
        {
            s.host = q.fileHost;
            pTotalDownloadPerSecond += actualTransferRate;
        }
        else
        {
            pTotalUploadPerSecond += actualTransferRate;
        }
        
        //Check if entry exists
        QList<QStandardItem *> findResults = transferListModel->findItems(base32TTH.data(), Qt::MatchExactly, 8);

        int row = -1;

        //Get the appropriate row
        for (int k = 0; k < findResults.size(); k++)
        {
            //If type and host matches, get first entry.
            if ((transferListModel->itemFromIndex(transferListModel->index(findResults.at(k)->row(), 0))->text() == typeString(s.transferType))
                && (transferListModel->itemFromIndex(transferListModel->index(findResults.at(k)->row(), 9))->text() == s.host.toString()))
            {
                row = findResults.at(k)->row();
                break;
            }
        }

        //If new transfer - add
        if (row == -1)
        {
            QString progressTooltip;
            if (s.transferType == TRANSFER_TYPE_DOWNLOAD)
                progressTooltip = tr("<b>Progress bar colours - downloads</b><br/><br/><font color=\"#FFFFFF\">&#x2588;</font> (White) - Not downloaded<br/><font color=\"lightGreen\">&#x2588;</font> (Light green) - Allocated for download<br/><font color=\"#4FBD36\">&#x2588;</font> (Green) - Finished downloading<br/><font color=\"magenta\">&#x2588;</font> (Magenta) - Busy hashing segment<br/><font color=\"#FFF000\">&#x2588;</font> (Yellow) - Busy writing to disk");
            else
                progressTooltip = tr("<b>Progress bar colours - uploads</b><br/><br/><font color=\"#FFFFFF\">&#x2588;</font> (White) - Not uploaded<br/><font color=\"#2595D6\">&#x2588;</font> (Blue) - Uploaded<br/>");

            pIDHash.insert(s.TTH, transferID++);
            QList<QStandardItem *> row;
            row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, typeString(s.transferType)));
            row.append(new CStandardItem(CStandardItem::BitmapType, bitmapString(transferID, currentUpdateID++, s.transferProgress, s.segmentBitmap)));
            row.last()->setToolTip(progressTooltip);
            row.append(new CStandardItem(CStandardItem::RateType, bytesToRate(actualTransferRate)));
            row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, s.filePathName.remove(ArpmanetDC::settingsManager()->getSetting(SettingsManager::DOWNLOAD_PATH), Qt::CaseInsensitive)));
            row.append(new CStandardItem(CStandardItem::SizeType, bytesToSize(q.fileSize)));
            row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, stateString(s.transferStatus)));
            row.append(new CStandardItem(CStandardItem::TimeDurationType, timeFromInt(s.uptime)));
            row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, segmentStatusString(s.segmentStatuses)));
            row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, base32TTH.data()));
            row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, s.host.toString()));
            transferListModel->appendRow(row);
        }
        //If existing transfer - update
        else
        {
            transferListModel->itemFromIndex(transferListModel->index(row, 0))->setText(typeString(s.transferType));
            //transferListModel->itemFromIndex(transferListModel->index(row, 1))->setText(progressString(s.transferType, s.transferProgress));
            transferListModel->itemFromIndex(transferListModel->index(row, 1))->setText(bitmapString(pIDHash.value(s.TTH), currentUpdateID++, s.transferProgress, s.segmentBitmap));
            transferListModel->itemFromIndex(transferListModel->index(row, 2))->setText(bytesToRate(actualTransferRate));
            transferListModel->itemFromIndex(transferListModel->index(row, 3))->setText(s.filePathName.remove(ArpmanetDC::settingsManager()->getSetting(SettingsManager::DOWNLOAD_PATH), Qt::CaseInsensitive));
            transferListModel->itemFromIndex(transferListModel->index(row, 4))->setText(bytesToSize(q.fileSize));
            transferListModel->itemFromIndex(transferListModel->index(row, 5))->setText(stateString(s.transferStatus));
            transferListModel->itemFromIndex(transferListModel->index(row, 6))->setText(timeFromInt(s.uptime));
            transferListModel->itemFromIndex(transferListModel->index(row, 7))->setText(segmentStatusString(s.segmentStatuses));
            transferListModel->itemFromIndex(transferListModel->index(row, 8))->setText(base32TTH.data());
            transferListModel->itemFromIndex(transferListModel->index(row, 9))->setText(s.host.toString());
        }                    
    }

    //Check if the model has residual entries
    for (int i = 0; i < transferListModel->rowCount(); i++)
    {
        QByteArray tth;
        tth.append(transferListModel->item(i, 8)->text());
        base32Decode(tth);

        if (!pTransferList->contains(tth))
        {
            transferListModel->removeRow(i);
            pIDHash.remove(tth);
            i--;
        }
    }

    //Adjust row height
    resizeRowsToContents(transferListTable);

    //Sort table
    int column = transferListTable->horizontalHeader()->sortIndicatorSection();
    Qt::SortOrder order = transferListTable->horizontalHeader()->sortIndicatorOrder();
    transferListTable->sortByColumn(column, order);

    //Set parent counters
    pParent->setDownloadPerSecond(pTotalDownloadPerSecond);
    pParent->setUploadPerSecond(pTotalUploadPerSecond);
}

//Remove an entry from the list
void TransferWidget::removeTransferEntry(QByteArray tth, int type)
{
    //Base32 encode the tth
    QByteArray base32TTH(tth);
    base32Encode(base32TTH);

    QList<QStandardItem *> findResults = transferListModel->findItems(base32TTH.data(), Qt::MatchExactly, 8);

    while (!findResults.isEmpty())
    {
        QString typeStr = transferListModel->itemFromIndex(transferListModel->index(findResults.first()->row(), 0))->text();

        if (typeFromString(typeStr) == type)
        {
            transferListModel->removeRow(findResults.first()->row());
        }

        findResults = transferListModel->findItems(base32TTH.data(), Qt::MatchExactly, 8);
    }
}

QString TransferWidget::typeString(int type)
{
     //Determine transfer type
    switch (type)
    {
        case TRANSFER_TYPE_DOWNLOAD:
            return "Download";
        case TRANSFER_TYPE_UPLOAD:
            return "Upload";
    }
    return "";
}

int TransferWidget::typeFromString(QString typeStr)
{
    //Determine transfer type
    if (typeStr == "Download")
        return TRANSFER_TYPE_DOWNLOAD;
    if (typeStr == "Upload")
        return TRANSFER_TYPE_UPLOAD;
    return -1;
}

QString TransferWidget::stateString(int state)
{
    //Determine status
    switch (state)
    {
        case TRANSFER_STATE_PAUSED:
            return "Paused";
        case TRANSFER_STATE_INITIALIZING:
            return "Initializing";
        case TRANSFER_STATE_RUNNING:
            return "Running";
        case TRANSFER_STATE_STALLED:
            return "Stalled";
        case TRANSFER_STATE_IDLE:
            return "Idle";
        case TRANSFER_STATE_ABORTING:
            return "Aborting";
        case TRANSFER_STATE_FINISHED:
            return "Finished";
        case TRANSFER_STATE_IOWAIT:
            return "I/O Wait";
    }
    return "";
}

QString TransferWidget::segmentStatusString(SegmentStatusStruct s)
{
    return tr("%1-%2-%3-%4-%5").arg(s.running).arg(s.stalled).arg(s.finished).arg(s.initializing).arg(s.failed);
}

QString TransferWidget::progressString(int type, int progress)
{
    if (type == TRANSFER_TYPE_DOWNLOAD)
        return tr("D%1").arg(progress);
    else if (type == TRANSFER_TYPE_UPLOAD)
        return tr("U%1").arg(progress);

    return "";
}

QString TransferWidget::bitmapString(quint8 transferID, quint8 updateID, int progress, QByteArray bitmap)
{
    //Add progress to bitmap
    bitmap.prepend(quint8ToByteArray(transferID));
    bitmap.prepend(quint8ToByteArray(updateID));
    bitmap.prepend(quint8ToByteArray(progress));
    QString bitmapStr(bitmap.toBase64());
    return bitmapStr;
}

QWidget *TransferWidget::widget()
{
    return pWidget;
}

QHash<QByteArray, TransferItemStatus> *TransferWidget::transferList() const
{
    return pTransferList;
}

bool TransferWidget::isBusy(QByteArray tth)
{
    return pTransferList->contains(tth);
}
