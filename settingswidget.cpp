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
    pWidget = new QWidget();

    //========== BASIC SETTINGS ==========

    hubAddressLineEdit = new QLineEdit(pSettings->value("hubAddress"),pWidget);
    
    hubPortLineEdit = new QLineEdit(pSettings->value("hubPort"), pWidget);
    hubPortLineEdit->setValidator(new QIntValidator(0, 65535, this));

    nickLineEdit = new QLineEdit(pSettings->value("nick"), pWidget);
    
    passwordLineEdit = new QLineEdit(pSettings->value("password"), pWidget);
    passwordLineEdit->setEchoMode(QLineEdit::Password);
    
    downloadPathLineEdit = new QLineEdit(pSettings->value("downloadPath"), pWidget);

    browseDownloadPathButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/FolderIcon.png"), tr("Browse"), pWidget);

    shareUpdateIntervalSpinBox = new QSpinBox(pWidget);
    shareUpdateIntervalSpinBox->setRange(0, 10080); //Maximum is every week
    int value = pSettings->value("autoUpdateShareInterval").toInt()/60000;
    shareUpdateIntervalSpinBox->setValue(value);
    if (value != 1)
        shareUpdateIntervalSpinBox->setSuffix(" minutes");
    else
        shareUpdateIntervalSpinBox->setSuffix(" minute");
    shareUpdateIntervalSpinBox->setSpecialValueText("Disabled");

    //========== ADVANCED SETTINGS ==========

    ipLineEdit = new QLineEdit(pSettings->value("externalIP"), pWidget);
    ipLineEdit->setValidator(new IPValidator(this));

    externalPortLineEdit = new QLineEdit(pSettings->value("externalPort"), pWidget);
    externalPortLineEdit->setValidator(new QIntValidator(0, 65535, this));

    protocolList = new QListWidget(pWidget);
    protocolList->setMaximumHeight(150);
    protocolList->setMaximumWidth(100);
    protocolList->setSelectionMode(QAbstractItemView::SingleSelection);

    guessIPButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/GuessIcon.png"), tr("Guess External IP"), pWidget);
    
    protocolUpButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/GreenUpIcon.png"), tr("Up"), pWidget);
    protocolDownButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/RedDownIcon.png"), tr("Down"), pWidget);

    //Enqueue supported protocols
    QString protocolHint = pSettings->value("protocolHint");
    QStringList unsupportedProtocols = QString(UNSUPPORTED_TRANSFER_PROTOCOLS).split(";");
    foreach (QChar prot, protocolHint)
    {
        QListWidgetItem *item = new QListWidgetItem(PROTOCOL_MAP.key(prot.toAscii()), protocolList); 
        if (unsupportedProtocols.contains(PROTOCOL_MAP.key(prot.toAscii())))
        {
            //Items not supported are shown in gray
            item->setForeground(QBrush(Qt::gray));
            item->setFlags(Qt::NoItemFlags);
        }
    }

    //========== MISC ==========
    saveButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/CheckIcon.png"), tr("Save changes"), pWidget);
    toggleAdvancedCheckBox = new QCheckBox(tr("Show advanced settings"), pWidget);
    if (pSettings->value("showAdvanced") == "1")
        toggleAdvancedCheckBox->setChecked(true);
    else
        toggleAdvancedCheckBox->setChecked(false);
    advancedWidget = new QWidget(pWidget);
}

void SettingsWidget::placeWidgets()
{
    QHBoxLayout *downloadPathLayout = new QHBoxLayout;
    downloadPathLayout->addWidget(downloadPathLineEdit);
    downloadPathLayout->addWidget(browseDownloadPathButton);

    QHBoxLayout *autoUpdateLayout = new QHBoxLayout;
    autoUpdateLayout->addWidget(shareUpdateIntervalSpinBox);

    QFormLayout *flayout = new QFormLayout();
    flayout->addRow(new QLabel("<b>User information</b>"));
    flayout->addRow(new QLabel("Nickname:"), nickLineEdit);
    flayout->addRow(new QLabel("Password:"), passwordLineEdit);
    flayout->addRow(new QLabel("<b>Download path</b>"));
    flayout->addRow(new QLabel("Download path:"), downloadPathLayout);
    flayout->addRow(new QLabel("<b>Hub information</b>"));
    flayout->addRow(new QLabel("Hub address:"), hubAddressLineEdit);
    flayout->addRow(new QLabel("Hub port:"), hubPortLineEdit);
    flayout->addRow(new QLabel("<b>Shares</b>"));
    flayout->addRow(new QLabel("Share update interval:"), autoUpdateLayout);

    QHBoxLayout *guessLayout = new QHBoxLayout();
    guessLayout->addWidget(ipLineEdit);
    guessLayout->addWidget(guessIPButton);    

    QVBoxLayout *protocolButtonLayout = new QVBoxLayout();
    protocolButtonLayout->addStretch(1);
    protocolButtonLayout->addWidget(protocolUpButton);
    protocolButtonLayout->addWidget(protocolDownButton);
    protocolButtonLayout->addStretch(1);

    QHBoxLayout *protocolLayout = new QHBoxLayout();
    protocolLayout->addWidget(protocolList);
    protocolLayout->addLayout(protocolButtonLayout);
    protocolLayout->addStretch(1);

    QFormLayout *flayoutR = new QFormLayout();
    flayoutR->addRow(new QLabel("<font color=\"red\"><b>Warning: Advanced users only!</b></font>"));
    flayoutR->addRow(new QLabel("<font color=\"red\">External IP:</font>"), guessLayout);
    flayoutR->addRow(new QLabel("<font color=\"red\">External port:</font>"), externalPortLineEdit);
    flayoutR->addRow(new QLabel("<font color=\"red\">Transfer protocol preferences</font>"), protocolLayout);
    flayoutR->setContentsMargins(0,0,0,0);

    advancedWidget->setLayout(flayoutR);

    QHBoxLayout *formLayouts = new QHBoxLayout();
    formLayouts->addLayout(flayout);
    formLayouts->addSpacing(20);
    formLayouts->addWidget(advancedWidget);  

    QHBoxLayout *hlayout = new QHBoxLayout();
    hlayout->addWidget(toggleAdvancedCheckBox);
    hlayout->addStretch(1);
    hlayout->addWidget(saveButton);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->addLayout(formLayouts);
    layout->addStretch(1);
    layout->addLayout(hlayout);

    pWidget->setLayout(layout);
    
    if (!toggleAdvancedCheckBox->isChecked())
        advancedWidget->hide();    
}

void SettingsWidget::connectWidgets()
{
    connect(saveButton, SIGNAL(clicked()), this, SLOT(savePressed()));
    connect(guessIPButton, SIGNAL(clicked()), this, SLOT(guessIPPressed()));
    connect(browseDownloadPathButton, SIGNAL(clicked()), this, SLOT(browseDownloadPathPressed()));

    connect(protocolUpButton, SIGNAL(clicked()), this, SLOT(protocolUpPressed()));
    connect(protocolDownButton, SIGNAL(clicked()), this, SLOT(protocolDownPressed()));

    connect(toggleAdvancedCheckBox, SIGNAL(stateChanged(int)), this, SLOT(advancedCheckBoxToggled(int)));

    connect(shareUpdateIntervalSpinBox, SIGNAL(valueChanged(int)), this, SLOT(shareUpdateIntervalSpinBoxValueChanged(int)));
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
    if (downloadPathLineEdit->text() == "")
        missingStr.append("Download path<br/>");

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
        (*pSettings)["downloadPath"] = downloadPathLineEdit->text().replace("\\","/");
        (*pSettings)["autoUpdateShareInterval"] = tr("%1").arg(shareUpdateIntervalSpinBox->value()*60000);

        //Build protocols string
        QString protocolHint;
        
        for (int i = 0; i < protocolList->count(); i++)
        {
            QString itemText = protocolList->item(i)->text();
            protocolHint.append(PROTOCOL_MAP.value(itemText));
        }
        (*pSettings)["protocolHint"] = protocolHint;

        //Save checkbox state
        if (toggleAdvancedCheckBox->isChecked())
            (*pSettings)["showAdvanced"] = "1";
        else
            (*pSettings)["showAdvanced"] = "0";

        emit settingsSaved();
    }
}

void SettingsWidget::guessIPPressed()
{
    QString ip = pParent->getIPGuess().toString();
    ipLineEdit->setText(ip);
}

void SettingsWidget::browseDownloadPathPressed()
{
    QString downloadPath = QFileDialog::getExistingDirectory((QWidget *)pParent, tr("Select download path"), downloadPathLineEdit->text());
    if (!downloadPath.isEmpty())
        downloadPathLineEdit->setText(downloadPath.replace("\\","/"));
}

//Up pressed
void SettingsWidget::protocolUpPressed()
{
    QList<QListWidgetItem *> selectedItems = protocolList->selectedItems();

    if (selectedItems.isEmpty())
        return;

    //Calculate new index
    int index = protocolList->row(selectedItems.first())-1;
    protocolList->takeItem(index+1);

    //Check indexes to make sure they are valid
    if (index < 0)
        index++;
    
    //Add to list
    protocolList->insertItem(index, selectedItems.first());
    protocolList->setCurrentItem(selectedItems.first());
}

//Down pressed
void SettingsWidget::protocolDownPressed()
{
    QList<QListWidgetItem *> selectedItems = protocolList->selectedItems();
    int count = protocolList->count();

    if (selectedItems.isEmpty())
        return;

    //Calculate new index
    int index = protocolList->row(selectedItems.first())+1;
    protocolList->takeItem(index-1);

    //Check indexes to make sure they are valid
    if (index >= count)
        index--;
    
    //Add to list
    protocolList->insertItem(index, selectedItems.first());
    protocolList->setCurrentItem(selectedItems.first());
}

void SettingsWidget::shareUpdateIntervalSpinBoxValueChanged(int value)
{
    if (value == 1)
        shareUpdateIntervalSpinBox->setSuffix(" minute");
    else if (value > 1 && shareUpdateIntervalSpinBox->suffix() != " minutes")
        shareUpdateIntervalSpinBox->setSuffix(" minutes");
}

void SettingsWidget::advancedCheckBoxToggled(int state)
{
    if (state == Qt::Checked)
        advancedWidget->show();
    else
        advancedWidget->hide();
    QApplication::processEvents();
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