#include "settingswidget.h"
#include "arpmanetdc.h"

SettingsWidget::SettingsWidget(SettingsStruct *settings, ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;
	pSettings = settings;

	createWidgets();
	placeWidgets();
	connectWidgets();
}

SettingsWidget::~SettingsWidget()
{
	//Destructor
}

void SettingsWidget::createWidgets()
{
	hubAddressLineEdit = new QLineEdit(pSettings->hubAddress,(QWidget *)pParent);
	//hubAddressLineEdit->setInputMask("009.009.009.009;0");
	hubAddressLineEdit->setValidator(new IPValidator(this));
	
	hubPortLineEdit = new QLineEdit(tr("%1").arg(pSettings->hubPort), (QWidget *)pParent);
	hubPortLineEdit->setValidator(new QIntValidator(0, 65535, this));

	nickLineEdit = new QLineEdit(pSettings->nick, (QWidget *)pParent);
	
	passwordLineEdit = new QLineEdit(pSettings->password, (QWidget *)pParent);
	passwordLineEdit->setEchoMode(QLineEdit::Password);
	
	ipLineEdit = new QLineEdit(pSettings->externalIP, (QWidget *)pParent);
	//ipLineEdit->setInputMask("009.009.009.009;0");
	ipLineEdit->setValidator(new IPValidator(this));

	externalPortLineEdit = new QLineEdit(tr("%1").arg(pSettings->externalPort), (QWidget *)pParent);
	externalPortLineEdit->setValidator(new QIntValidator(0, 65535, this));

	saveButton = new QPushButton(tr("Save changes"), (QWidget *)pParent);
}

void SettingsWidget::placeWidgets()
{
	QFormLayout *flayout = new QFormLayout();
	flayout->addRow(new QLabel("Hub address:"), hubAddressLineEdit);
	flayout->addRow(new QLabel("Hub port:"), hubPortLineEdit);
	flayout->addRow(new QLabel("Nickname:"), nickLineEdit);
	flayout->addRow(new QLabel("Password:"), passwordLineEdit);
	flayout->addRow(new QLabel("External IP:"), ipLineEdit);
	flayout->addRow(new QLabel("External port:"), externalPortLineEdit);

	QHBoxLayout *hlayout = new QHBoxLayout();
	hlayout->addStretch(1);
	hlayout->addWidget(saveButton);

	QVBoxLayout *layout = new QVBoxLayout();
	layout->addLayout(flayout);
	layout->addLayout(hlayout);

	pWidget = new QWidget();
	pWidget->setLayout(layout);
}

void SettingsWidget::connectWidgets()
{
	connect(saveButton, SIGNAL(clicked()), this, SLOT(savePressed()));
}

void SettingsWidget::savePressed()
{
	QString missingStr = "";

	if (hubAddressLineEdit->text().isEmpty())
		missingStr.append("Hub address<br/>");
	if (hubPortLineEdit->text() == "0")
		missingStr.append("Hub port<br/>");
	if (nickLineEdit->text().isEmpty())
		missingStr.append("Nickname<br/>");
	if (passwordLineEdit->text().isEmpty())
		missingStr.append("Password<br/>");
	if (ipLineEdit->text().isEmpty())
		missingStr.append("External IP<br/>");
	if (externalPortLineEdit->text() == "0")
		missingStr.append("External Port<br/>");

	if (!missingStr.isEmpty())
		QMessageBox::warning((QWidget *)pParent, tr("ArpmanetDC"), tr("<p><b>Information missing:</b></p><p>%1</p><p>Please enter the above fields and try again.</p>").arg(missingStr));
	else
	{
		pSettings->hubAddress = hubAddressLineEdit->text();
		pSettings->hubPort = hubPortLineEdit->text().toUShort();
		pSettings->nick = nickLineEdit->text();
		pSettings->password = passwordLineEdit->text();
		pSettings->externalIP = ipLineEdit->text();
		pSettings->externalPort = externalPortLineEdit->text().toUShort();

		emit settingsSaved();
	}
}

QWidget *SettingsWidget::widget()
{
	//TODO: Return widget containing all search widgets
	return pWidget;
}

SettingsStruct *SettingsWidget::settings()
{
	return pSettings;
}

//Fix an IPv4 address
void IPValidator::fixup(QString &input) const
{
	QStringList octets = input.split(".");
	QString output = "";
	for (int i = 0; i < octets.size(); i++)
	{
		//Ensure no empty octets exist
		if (octets.at(i).size() == 0)
			octets[i] = "0";
	}

	//Ensure enough octets exist
	while (octets.size() < 4)
		octets.append("0");

	//Rebuild address
	for (int i = 0; i < octets.size(); i++)
	{
		output.append(octets.at(i));
		if (i < octets.size() - 1)
			output.append(".");
	}
	input = output;
}

//Validate an IPv4 address
QValidator::State IPValidator::validate(QString &input, int &pos) const
{
	//Split the address into octets
	QStringList octets = input.split(".");

	//If more than 4 octects = invalid
	if (octets.size() > 4)
		return QValidator::Invalid;

	//Iterate through all octets
	for (int i = 0; i < octets.size(); i++)
	{
		//Don't mind a full stop at the end while being entered
		if (octets.at(i).isEmpty() && i == octets.size() - 1)
			continue;

		//Don't allow stuff like 0000000001
		if (octets.at(i).size() > 3)
			return QValidator::Invalid;

		//Convert octet to integer
		bool ok;
		int val = octets.at(i).toInt(&ok);
		//If not a number = invalid
		if (!ok)
			return QValidator::Invalid;
		if (ok)
		{
			//If out of range = invalid
			if (val > 255 || val < 0)
				return QValidator::Invalid;
		}
	}
	
	//Check if less than 4 octets or last octet is empty
	if (octets.size() < 4 || octets.last().isEmpty())
		return QValidator::Intermediate;
	return QValidator::Acceptable;
}