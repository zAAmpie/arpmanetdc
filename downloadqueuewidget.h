#ifndef DOWNLOADQUEUEWIDGET_H
#define DOWNLOADQUEUEWIDGET_H

#include <QtGui>

class ArpmanetDC;

enum QueuePriority {LowQueuePriority='L', NormalQueuePriority='N', HighQueuePriority='H'};

struct QueueStruct
{
	QString fileName;
	QString filePath;
	qint64 fileSize;
	QueuePriority priority;
	QByteArray *tthRoot;
};

//Class encapsulating all widgets/signals for search tab
class DownloadQueueWidget : public QObject
{
	Q_OBJECT

public:
	DownloadQueueWidget(QList<QueueStruct> *queueList, ArpmanetDC *parent);
	~DownloadQueueWidget();

	//Get the encapsulating widget
	QWidget *widget();

	QList<QueueStruct> *queueList();
	void setQueueList(QList<QueueStruct> *list);

private slots:
	//Slots
	void showQueueTableContextMenu(const QPoint &);

	//Actions
	void setPriorityLowActionPressed();
	void setPriorityNormalActionPressed();
	void setPriorityHighActionPressed();
	void deleteActionPressed();
	void searchForAlternatesActionPressed();

signals:
	//Signals
	void searchForAlternates(QByteArray *tthRoot);
	void setPriority(QByteArray *tthRoot, QueuePriority priority);
	void deleteFromQueue(QByteArray *tthRoot);

private:
	//Functions
	void createWidgets();
	void placeWidgets();
	void connectWidgets();

	//Objects
	QWidget *pWidget;
	ArpmanetDC *pParent;

	QList<QueueStruct> *pQueueList;

	//GUI
	QTableView *queueTable;
	QStandardItemModel *queueModel;

	QAction *setPriorityLowAction, *setPriorityNormalAction, *setPriorityHighAction;
	QAction *deleteAction, *searchForAlternatesAction;
};

#endif