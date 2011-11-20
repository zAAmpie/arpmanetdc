#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include <QtGui>

class ArpmanetDC;

//Struct for holding all settings
struct SettingsStruct
{
	QString hubAddress;
	quint16 hubPort;
	
	QString nick;
	QString password;

	QString externalIP; //your own ip (or behind nat)
	quint16 externalPort;
};

//Class encapsulating all widgets/signals for search tab
class SettingsWidget : public QObject
{
	Q_OBJECT

public:
	SettingsWidget(SettingsStruct *settings, ArpmanetDC *parent);
	~SettingsWidget();

	//Get the encapsulating widget
	QWidget *widget();

	SettingsStruct *settings();

public slots:
	//Slots

private slots:
	//Pressed save button
	void savePressed();

signals:
	//Signalled when settings were saved
	void settingsSaved();

private:
	//Functions
	void createWidgets();
	void placeWidgets();
	void connectWidgets();

	//Objects
	QWidget *pWidget;
	ArpmanetDC *pParent;

	SettingsStruct *pSettings;

	//GUI
	QLineEdit *hubAddressLineEdit, *hubPortLineEdit, *nickLineEdit, *passwordLineEdit, *ipLineEdit, *externalPortLineEdit;
	QPushButton *saveButton;

};

//Custom validator class to deal with IP addresses :)
class IPValidator : public QValidator
{
	Q_OBJECT
public:
	IPValidator(QObject *parent = 0) : QValidator(parent) {};

	void fixup(QString &input) const;
	QValidator::State validate(QString &input, int &pos) const;
};

#endif