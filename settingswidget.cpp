#include "settingswidget.h"
#include "arpmanetdc.h"

SettingsWidget::SettingsWidget(QHash<QString, QString> *settings, ArpmanetDC *parent)
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
	hubAddressLineEdit = new QLineEdit(pSettings->value("hubAddress"),(QWidget *)pParent);
	//hubAddressLineEdit->setValidator(new IPValidator(this));                              //Determine if IP validator is needed, most likely a hostname will suffice
	
	hubPortLineEdit = new QLineEdit(pSettings->value("hubPort"), (QWidget *)pParent);
	hubPortLineEdit->setValidator(new QIntValidator(0, 65535, this));

	nickLineEdit = new QLineEdit(pSettings->value("nick"), (QWidget *)pParent);
	
	passwordLineEdit = new QLineEdit(pSettings->value("password"), (QWidget *)pParent);
	passwordLineEdit->setEchoMode(QLineEdit::Password);
	
	ipLineEdit = new QLineEdit(pSettings->value("externalIP"), (QWidget *)pParent);
	ipLineEdit->setValidator(new IPValidator(this));

	externalPortLineEdit = new QLineEdit(pSettings->value("externalPort"), (QWidget *)pParent);
	externalPortLineEdit->setValidator(new QIntValidator(0, 65535, this));

	saveButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/CheckIcon.png"), tr("Save changes"), (QWidget *)pParent);
    guessIPButton = new QPushButton(tr("Guess External IP"), (QWidget *)pParent);
}

void SettingsWidget::placeWidgets()
{
	QFormLayout *flayout = new QFormLayout();
	flayout->addRow(new QLabel("<b>User information</b>"));
    flayout->addRow(new QLabel("Nickname:"), nickLineEdit);
	flayout->addRow(new QLabel("Password:"), passwordLineEdit);
    flayout->addRow(new QLabel("<b>Hub information</b>"));
    flayout->addRow(new QLabel("Hub address:"), hubAddressLineEdit);
	flayout->addRow(new QLabel("Hub port:"), hubPortLineEdit);
    flayout->addRow(new QLabel("<br/><font color=\"red\"><b>Warning: Advanced users only. Leave these at default settings unless you know exactly what you're doing.</b></font>"));
	flayout->addRow(new QLabel("<font color=\"red\">External IP:</font>"), ipLineEdit);
	flayout->addRow(new QLabel("<font color=\"red\">External port:</font>"), externalPortLineEdit);

    QHBoxLayout *guessLayout = new QHBoxLayout();
    guessLayout->addWidget(guessIPButton);
	guessLayout->addStretch(1);

	QHBoxLayout *hlayout = new QHBoxLayout();
	hlayout->addStretch(1);
	hlayout->addWidget(saveButton);

	QVBoxLayout *layout = new QVBoxLayout();
	layout->addLayout(flayout);
    layout->addLayout(guessLayout);
    layout->addStretch(1);
	layout->addLayout(hlayout);

	pWidget = new QWidget();
	pWidget->setLayout(layout);
}

void SettingsWidget::connectWidgets()
{
	connect(saveButton, SIGNAL(clicked()), this, SLOT(savePressed()));
    connect(guessIPButton, SIGNAL(clicked()), this, SLOT(guessIPPressed()));
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
        (*pSettings)["hubAddress"] = hubAddressLineEdit->text();
		(*pSettings)["hubPort"] = hubPortLineEdit->text();
        (*pSettings)["nick"] = nickLineEdit->text();
		(*pSettings)["password"] = passwordLineEdit->text();
        (*pSettings)["externalIP"] = ipLineEdit->text();
        (*pSettings)["externalPort"] = externalPortLineEdit->text();

		emit settingsSaved();
	}
}

void SettingsWidget::guessIPPressed()
{
    QString ip = pParent->getIPGuess().toString();
    ipLineEdit->setText(ip);
}

QWidget *SettingsWidget::widget()
{
	//TODO: Return widget containing all search widgets
	return pWidget;
}

QHash<QString, QString> *SettingsWidget::settings() const
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