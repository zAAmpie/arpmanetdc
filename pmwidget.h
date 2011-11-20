#ifndef PMWIDGET_H
#define PMWIDGET_H

#include <QtGui>
class ArpmanetDC;

//Class encapsulating all widgets/signals for search tab
class PMWidget : public QObject
{
	Q_OBJECT

public:
	PMWidget(QString otherNick, ArpmanetDC *parent);
	~PMWidget();

	//Get the encapsulating widget
	QWidget *widget();

	//Get functions
	QString otherNick();

public slots:
	//Slots for incoming PM message
	void receivePrivateMessage(QString message);

private slots:
	//Slot for when user presses return
	void sendMessage();

signals:
	//Signal for outgoing PM message
	void sendPrivateMessage(QString nick, QString message, PMWidget *pmWidget);

private:
	//Functions
	void createWidgets();
	void placeWidgets();
	void connectWidgets();

	//Objects
	QWidget *pWidget;
	ArpmanetDC *pParent;

	//Parameters
	QString pOtherNick;

	//GUI elements
	QTextBrowser *chatBrowser;
	QLineEdit *chatLineEdit;
};

#endif

