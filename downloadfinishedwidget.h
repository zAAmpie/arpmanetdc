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

	//Actions
	void clearActionPressed();
	void openActionPressed();

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
	QAction *openAction, *clearAction;
};

#endif