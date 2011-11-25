#ifndef DownloadFinishedWidget_H
#define DownloadFinishedWidget_H

#include <QtGui>

class ArpmanetDC;

struct FinishedDownloadStruct
{
	QString fileName;
	QString filePath;
	qint64 fileSize;
	QByteArray *tthRoot;
	QString downloadedDate;
};

//Class encapsulating all widgets/signals for search tab
class DownloadFinishedWidget : public QObject
{
	Q_OBJECT

public:
	DownloadFinishedWidget(ArpmanetDC *parent);
	~DownloadFinishedWidget();

	//Get the encapsulating widget
	QWidget *widget();

	QList<FinishedDownloadStruct> *finishedList();
	void setFinishedList(QList<FinishedDownloadStruct> *list);

public slots:
	//Return requested list
	void returnFinishedList(QList<FinishedDownloadStruct> *list);

	//Add an entry
	void addFinishedDownload(FinishedDownloadStruct file);

private slots:
	//Slots
	void showFinishedTableContextMenu(const QPoint &);

	//Actions
	void clearActionPressed();
	void openActionPressed();

signals:
	//Signals for the finished downloads list from the database
	void requestFinishedList();
	//Signals to clear the database list
	void clearFinishedList();
	
private:
	//Functions
	void createWidgets();
	void placeWidgets();
	void connectWidgets();

	//Load list from database
	void loadFinishedDownloads();

	//Objects
	QWidget *pWidget;
	ArpmanetDC *pParent;

	QList<FinishedDownloadStruct> *pFinishedList;

	//GUI
	QTableView *finishedTable;
	QStandardItemModel *finishedModel;

    QMenu *finishedMenu;
	QAction *openAction, *clearAction;
};

#endif