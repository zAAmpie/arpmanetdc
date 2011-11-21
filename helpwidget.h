#ifndef HELPWIDGET_H
#define HELPWIDGET_H

#include <QtGui>

class ArpmanetDC;
struct SettingsStruct;

//Displays help
class HelpWidget : public QObject
{
	Q_OBJECT

public:
	HelpWidget(SettingsStruct *settings, ArpmanetDC *parent);
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

	SettingsStruct *pSettings;
};

#endif