#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include <QtGui>

class ArpmanetDC;

//Class encapsulating all widgets/signals for search tab
class SettingsWidget : public QObject
{
	Q_OBJECT

public:
	SettingsWidget(QHash<QString, QString> *settings, ArpmanetDC *parent);
	~SettingsWidget();

	//Get the encapsulating widget
	QWidget *widget();

	QHash<QString, QString> *settings() const;

public slots:
	//Slots

private slots:
	//Pressed save button
	void savePressed();
    //Pressed guess IP button
    void guessIPPressed();
    //Pressed download path browse button
    void browseDownloadPathPressed();

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

	QHash<QString, QString> *pSettings;

	//GUI
	QLineEdit *hubAddressLineEdit, *hubPortLineEdit, *nickLineEdit, *passwordLineEdit, *ipLineEdit, *externalPortLineEdit, *downloadPathLineEdit;
	QPushButton *saveButton, *guessIPButton, *browseDownloadPathButton;

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