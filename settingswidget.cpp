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

    generalWidget = createGeneralSettingsPage();

    //========== ADVANCED SETTINGS ==========

    advancedWidget = createAdvancedSettingsPage();

    //========== USER COMMANDS ==========

    userCommandWidget = createUserCommandsPage();

    //========== MISC ==========
    saveButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/CheckIcon.png"), tr("Save changes"), pWidget);
    toggleAdvancedCheckBox = new QCheckBox(tr("Show advanced settings"), pWidget);
    if (pSettings->value("showAdvanced") == "1")
        toggleAdvancedCheckBox->setChecked(true);
    else
        toggleAdvancedCheckBox->setChecked(false);
}


//Create the page widget for general settings
QWidget *SettingsWidget::createGeneralSettingsPage()
{
    QWidget *pageWidget = new QWidget;

    //User settings
    QGroupBox *userGroup = new QGroupBox(tr("User settings"));
    
    nickLineEdit = new QLineEdit(pSettings->value("nick"), pWidget);
    
    passwordLineEdit = new QLineEdit(pSettings->value("password"), pWidget);
    passwordLineEdit->setEchoMode(QLineEdit::Password);

    QFormLayout *userLayout = new QFormLayout;
    userLayout->addRow(tr("Nickname:"), nickLineEdit);
    userLayout->addRow(tr("Password"), passwordLineEdit);
    userGroup->setLayout(userLayout);

    //Hub settings
    QGroupBox *hubGroup = new QGroupBox(tr("Hub settings"));
    
    hubAddressLineEdit = new QLineEdit(pSettings->value("hubAddress"),pWidget);
    
    hubPortLineEdit = new QLineEdit(pSettings->value("hubPort"), pWidget);
    hubPortLineEdit->setValidator(new QIntValidator(0, 65535, this));

    QFormLayout *hubLayout = new QFormLayout;
    hubLayout->addRow(tr("Hub address:"), hubAddressLineEdit);
    hubLayout->addRow(tr("Hub port:"), hubPortLineEdit);
    hubGroup->setLayout(hubLayout);

    //Sharing/Download settings    
    QGroupBox *sharingGroup = new QGroupBox(tr("Sharing/Download settings"));

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

    QHBoxLayout *downloadPathLayout = new QHBoxLayout;
    downloadPathLayout->addWidget(downloadPathLineEdit);
    downloadPathLayout->addWidget(browseDownloadPathButton);

    QFormLayout *sharingLayout = new QFormLayout;
    sharingLayout->addRow(tr("Download path:"), downloadPathLayout);
    sharingLayout->addRow(tr("Share update interval:"), shareUpdateIntervalSpinBox);
    sharingGroup->setLayout(sharingLayout);

    //Main page layout
    QVBoxLayout *pageLayout = new QVBoxLayout;
    pageLayout->addWidget(userGroup);
    pageLayout->addWidget(hubGroup);
    pageLayout->addWidget(sharingGroup);
    pageLayout->addStretch(1);

    //Set widget to final layout and return
    pageWidget->setLayout(pageLayout);
    return pageWidget;
}

//Create the page widget for advanced settings
QWidget *SettingsWidget::createAdvancedSettingsPage()
{
    QWidget *pageWidget = new QWidget;

    //Dispatcher settings
    QGroupBox *dispatchGroup = new QGroupBox(tr("Dispatcher settings"));

    ipLineEdit = new QLineEdit(pSettings->value("externalIP"), pWidget);
    ipLineEdit->setValidator(new IPValidator(this));
    
    guessIPButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/GuessIcon.png"), tr("Guess External IP"), pWidget);

    externalPortLineEdit = new QLineEdit(pSettings->value("externalPort"), pWidget);
    externalPortLineEdit->setValidator(new QIntValidator(0, 65535, this));
    
    QHBoxLayout *guessLayout = new QHBoxLayout();
    guessLayout->addWidget(ipLineEdit);
    guessLayout->addWidget(guessIPButton);  

    QFormLayout *dispatcherLayout = new QFormLayout;
    dispatcherLayout->addRow(tr("External IP:"), guessLayout);
    dispatcherLayout->addRow(tr("External port:"), externalPortLineEdit);
    dispatchGroup->setLayout(dispatcherLayout);

    //Protocol settings
    QGroupBox *protocolGroup = new QGroupBox(tr("Protocol settings"));

    protocolList = new QListWidget(pWidget);
    protocolList->setMaximumHeight(150);
    protocolList->setMaximumWidth(100);
    protocolList->setSelectionMode(QAbstractItemView::SingleSelection);
    
    protocolUpButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/GreenUpIcon.png"), tr("Up"), pWidget);
    protocolDownButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/RedDownIcon.png"), tr("Down"), pWidget);

    QVBoxLayout *protocolButtonLayout = new QVBoxLayout();
    protocolButtonLayout->addStretch(1);
    protocolButtonLayout->addWidget(protocolUpButton);
    protocolButtonLayout->addWidget(protocolDownButton);
    protocolButtonLayout->addStretch(1);

    QHBoxLayout *protocolLayout = new QHBoxLayout();
    protocolLayout->addWidget(protocolList);
    protocolLayout->addLayout(protocolButtonLayout);
    protocolLayout->addStretch(1);
    protocolGroup->setLayout(protocolLayout);

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

    //Main page layout
    QVBoxLayout *pageLayout = new QVBoxLayout;
    pageLayout->addWidget(dispatchGroup);
    pageLayout->addWidget(protocolGroup);
    pageLayout->addStretch(1);

    //Set widget to final layout and return
    pageWidget->setLayout(pageLayout);
    return pageWidget;
}

//Create the page widget for user command settings
QWidget *SettingsWidget::createUserCommandsPage()
{
    QWidget *pageWidget = new QWidget;

    //Main page layout
    QVBoxLayout *pageLayout = new QVBoxLayout;
    pageLayout->addWidget(new QLabel(tr("Under construction")));
    pageLayout->addStretch(1);

    //Set widget to final layout and return
    pageWidget->setLayout(pageLayout);
    return pageWidget;
}

//Create icons for pagesWidget
void SettingsWidget::createPageIcons()
{
    generalPageButton = new QListWidgetItem(contentsWidget);
    generalPageButton->setIcon(QIcon(":/ArpmanetDC/Resources/ServerIcon.png"));
    generalPageButton->setText(tr("General"));
    generalPageButton->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    generalPageButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    userCommandsPageButton = new QListWidgetItem(contentsWidget);
    userCommandsPageButton->setIcon(QIcon(":/ArpmanetDC/Resources/UserIcon.png"));
    userCommandsPageButton->setText(tr("User commands"));
    userCommandsPageButton->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    userCommandsPageButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    advancedPageButton = new QListWidgetItem(contentsWidget);
    advancedPageButton->setIcon(QIcon(":/ArpmanetDC/Resources/SettingsIcon.png"));
    advancedPageButton->setText(tr("Advanced"));
    advancedPageButton->setTextColor(Qt::red);
    advancedPageButton->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    advancedPageButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

void SettingsWidget::placeWidgets()
{
    //Create list of pages and the icons associated with it
    contentsWidget = new QListWidget;
    contentsWidget->setViewMode(QListView::ListMode);
    contentsWidget->setIconSize(QSize(32, 32));
    contentsWidget->setMovement(QListView::Static);
    contentsWidget->setMaximumWidth(200);
    //contentsWidget->setSpacing(12);

    createPageIcons();
    contentsWidget->setCurrentRow(0);

    //Create pages widget
    pagesWidget = new QStackedWidget;
    pagesWidget->addWidget(generalWidget);
    pagesWidget->addWidget(userCommandWidget);
    pagesWidget->addWidget(advancedWidget);
        
    //Create bottom button layout
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(toggleAdvancedCheckBox);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(saveButton);

    //Create pages layout
    QHBoxLayout *pagesLayout = new QHBoxLayout;
    pagesLayout->addWidget(contentsWidget);
    pagesLayout->addWidget(pagesWidget);

    //Main layout
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addLayout(pagesLayout);
    layout->addLayout(buttonLayout);

    pWidget->setLayout(layout);
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

    connect(contentsWidget, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)),
        this, SLOT(changePage(QListWidgetItem *, QListWidgetItem *)));
}

//Changed the list widget item
void SettingsWidget::changePage(QListWidgetItem *current, QListWidgetItem *previous)
{
    if (!current)
        current = previous;

    pagesWidget->setCurrentIndex(contentsWidget->row(current));
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
    {
        //Show advanced settings
        advancedPageButton->setHidden(false);
    }
    else
    {
        //Move away from advanced page
        if (contentsWidget->currentItem() == advancedPageButton)
            contentsWidget->setCurrentRow(0);

        //Hide advanced settings
        advancedPageButton->setHidden(true);
    }
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