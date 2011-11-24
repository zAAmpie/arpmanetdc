#ifndef HELPWIDGET_H
#define HELPWIDGET_H

#include <QtGui>

class ArpmanetDC;

//Displays help
class HelpWidget : public QObject
{
	Q_OBJECT

public:
	HelpWidget(ArpmanetDC *parent);
	~HelpWidget();

	//Get the encapsulating widget
	QWidget *widget();

private:
	//Functions
	void createWidgets();
	void placeWidgets();
	void connectWidgets();

	//Objects
	QWidget *pWidget;
	ArpmanetDC *pParent;
};

#endif