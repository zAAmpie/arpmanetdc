#ifndef DOWNLOADQUEUEWIDGET_H
#define DOWNLOADQUEUEWIDGET_H

#include <QtGui>

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

	//GUI
	QTableView *queueTable;
	QStandardItemModel *queueModel;

    QMenu *queueMenu, *setPriorityMenu;

	QAction *setPriorityLowAction, *setPriorityNormalAction, *setPriorityHighAction;
	QAction *deleteAction;
};

#endif