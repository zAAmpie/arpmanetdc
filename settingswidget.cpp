#include "settingswidget.h"
#include "arpmanetdc.h"

SettingsWidget::SettingsWidget(QHash<QString, QString> *settings, QHash<QString, UserCommandStruct> *commands, ArpmanetDC *parent)
{
    //Constructor
    pParent = parent;
    pSettings = settings;
    pUserCommands = commands;

    createWidgets();
    placeWidgets();
    connectWidgets();

    connect(this, SIGNAL(saveUserCommands(QHash<QString, UserCommandStruct>*)), parent->shareSearchObject(), SLOT(saveUserCommands(QHash<QString, UserCommandStruct>*)), Qt::QueuedConnection);
    connect(this, SIGNAL(requestUserCommands()), pParent, SIGNAL(requestUserCommands()));

    //Get user command list
    emit requestUserCommands();
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

    //User commands - doh
    QGroupBox *userGroup = new QGroupBox(tr("User commands"));

    userCommandCombo = new QComboBox;
    userCommandCombo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    
    addUserCommandButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/AddIcon.png"), tr("Add"));
    addUserCommandButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    renameUserCommandButton = new QPushButton(tr("Rename"));
    renameUserCommandButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    renameUserCommandButton->setEnabled(false);
    removeUserCommandButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/DeleteIcon.png"), tr("Delete"));
    removeUserCommandButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    removeUserCommandButton->setEnabled(false);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(userCommandCombo);
    buttonLayout->addWidget(addUserCommandButton);
    buttonLayout->addWidget(renameUserCommandButton);
    buttonLayout->addWidget(removeUserCommandButton);

    userCommandNameLineEdit = new QLineEdit();
    userCommandNameLineEdit->setEnabled(false);

    userCommandOutputLineEdit = new QLineEdit();
    userCommandOutputLineEdit->setEnabled(false);
    
    userCommandParameterCount = new QSpinBox();
    userCommandParameterCount->setRange(0, 9);
    userCommandParameterCount->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    userCommandParameterCount->setSpecialValueText(tr("None"));
    userCommandParameterCount->setMinimumWidth(50);
    userCommandParameterCount->setEnabled(false);

    QGridLayout *editLayout = new QGridLayout;
    editLayout->addWidget(new QLabel(tr("Command name:")), 0, 0);
    editLayout->addWidget(userCommandNameLineEdit, 0, 1);
    editLayout->addWidget(new QLabel(tr("Parameter count:")), 0, 2);
    editLayout->addWidget(userCommandParameterCount, 0, 3);
    editLayout->addWidget(new QLabel(tr("Output string:")), 1, 0);
    editLayout->addWidget(userCommandOutputLineEdit, 1, 1, 1, 3);

    QTextEdit *textEdit = new QTextEdit();
    textEdit->setReadOnly(true);
    textEdit->setWordWrapMode(QTextOption::WordWrap);
    textEdit->setHtml(tr("<b>Using user commands:</b><br/>User commands can be used as shortcuts for frequently used commands or expressions. They are invoked with double forward slashes followed by the command ie. //commandName<br/><br/>To add a user command:<br/>1) Click Add and choose a descriptive name for the command<br/>2) Type a single word without spaces to use as the shortcut (ex. \"logoutmessage\")<br/>3) Set the number of parameters you want to include in your command. For each parameter, a placeholder (ex. \"%1\", \"%2\" etc.) will appear in your output string and these placeholders will be replaced when you issue your command<br/>4) Type the string you want to show when issuing the command. Placeholders will automatically be added to this string (ex. \"/me is asking %1 whether he likes to %2\")<br/>5) When finished editing the user commands, click Save changes to save all user commands<br/><br/><b>Example</b><br/>Command name: annoy<br/>Parameter count: 2<br/>Output string: /me is trying to annoy %1 by being %2<br/><br/>To invoke the user command, type \"//annoy myFellowMan sarcastic\" in chat to produce the result: \"/me is trying to annoy myFellowMan by being sarcastic\""));
    
    editLayout->addWidget(textEdit,2,0,1,4);

    QVBoxLayout *groupLayout = new QVBoxLayout;
    groupLayout->addLayout(buttonLayout);
    groupLayout->addLayout(editLayout);
    userGroup->setLayout(groupLayout);

    //Main page layout
    QVBoxLayout *pageLayout = new QVBoxLayout;
    pageLayout->addWidget(userGroup);
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

    connect(browseDownloadPathButton, SIGNAL(clicked()), this, SLOT(browseDownloadPathPressed()));
    connect(shareUpdateIntervalSpinBox, SIGNAL(valueChanged(int)), this, SLOT(shareUpdateIntervalSpinBoxValueChanged(int)));

    connect(guessIPButton, SIGNAL(clicked()), this, SLOT(guessIPPressed()));
    connect(protocolUpButton, SIGNAL(clicked()), this, SLOT(protocolUpPressed()));
    connect(protocolDownButton, SIGNAL(clicked()), this, SLOT(protocolDownPressed()));

    connect(addUserCommandButton, SIGNAL(clicked()), this, SLOT(addUserCommandButtonPressed()));
    connect(renameUserCommandButton, SIGNAL(clicked()), this, SLOT(renameUserCommandButtonPressed()));
    connect(removeUserCommandButton, SIGNAL(clicked()), this, SLOT(removeUserCommandButtonPressed()));
    connect(userCommandCombo, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(switchedUserCommands(const QString &)));
    connect(userCommandNameLineEdit, SIGNAL(editingFinished()), this, SLOT(updateUserCommand()));
    connect(userCommandOutputLineEdit, SIGNAL(editingFinished()), this, SLOT(updateUserCommand()));
    connect(userCommandParameterCount, SIGNAL(valueChanged(int)), this, SLOT(updateUserCommand()));

    connect(toggleAdvancedCheckBox, SIGNAL(stateChanged(int)), this, SLOT(advancedCheckBoxToggled(int)));

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

//Return the user commands from the database
void SettingsWidget::returnUserCommands(QHash<QString, UserCommandStruct> *commands)
{
    if (!commands)
        return;

    if (commands->isEmpty())
        return;

    //Set local list
    pUserCommands = commands;

    //Populate user command combo box

    //Clear container list
    userCommandCombo->clear();

    QHashIterator<QString, UserCommandStruct> i(*pUserCommands);
    while (i.hasNext())
    {
        i.next();
        QString name = i.key();
        userCommandCombo->addItem(QIcon(":/ArpmanetDC/Resources/UserIcon.png"), name);
    }

    //Sort list of containers
    userCommandCombo->view()->model()->sort(0);

    //Disable entries if no containers exist
    if (userCommandCombo->count() == 0)
    {
        renameUserCommandButton->setEnabled(false);
        removeUserCommandButton->setEnabled(false);
        
        //Clear views
        userCommandNameLineEdit->clear();
        userCommandOutputLineEdit->clear();
        userCommandParameterCount->setValue(0);

        //Disable views
        userCommandNameLineEdit->setEnabled(false);
        userCommandOutputLineEdit->setEnabled(false);
        userCommandParameterCount->setEnabled(false);
    }
    else
    {
        renameUserCommandButton->setEnabled(true);
        removeUserCommandButton->setEnabled(true);
        
        //Enable views if they were disabled
        userCommandNameLineEdit->setEnabled(true);
        userCommandOutputLineEdit->setEnabled(true);
        userCommandParameterCount->setEnabled(true);
    }
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

        //Save user commands
        emit saveUserCommands(pUserCommands);

        //Alert main GUI
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

//Pressed add user command
void SettingsWidget::addUserCommandButtonPressed()
{
    QString name = QInputDialog::getText((QWidget *)pParent, tr("ArpmanetDC"), tr("Please enter a name for the user command"));
    if (!name.isEmpty() && !pUserCommands->contains(name))
    {
        //Add container to hash list
        UserCommandStruct u = {"", 0, ""};
        
        pUserCommands->insert(name, u);
        
        //Add container name to combo box
        userCommandCombo->addItem(QIcon(":/ArpmanetDC/Resources/UserIcon.png"), name);
        userCommandCombo->setCurrentIndex(userCommandCombo->count()-1);
                
        //Enable buttons
        renameUserCommandButton->setEnabled(true);
        removeUserCommandButton->setEnabled(true);
        
        //Enable views if they were disabled
        userCommandNameLineEdit->setEnabled(true);
        userCommandOutputLineEdit->setEnabled(true);
        userCommandParameterCount->setEnabled(true);
    }
    else if (!name.isEmpty())
        QMessageBox::information(pParent, tr("ArpmanetDC"), tr("A user command with that name already exists. Please try another name."));
}

//Pressed rename
void SettingsWidget::renameUserCommandButtonPressed()
{
    QString oldName = userCommandCombo->currentText();
    QString newName = QInputDialog::getText((QWidget *)pParent, tr("ArpmanetDC"), tr("Please enter a new name for user command <b>%1</b>").arg(oldName));
    if (!newName.isEmpty() && !pUserCommands->contains(newName))
    {
        //Replace user command
        UserCommandStruct u = pUserCommands->value(oldName);
        pUserCommands->remove(oldName);
        pUserCommands->insert(newName, u);
        
        //Add name to combo box
        userCommandCombo->removeItem(userCommandCombo->currentIndex());
        userCommandCombo->addItem(QIcon(":/ArpmanetDC/Resources/UserIcon.png"), newName);
        userCommandCombo->setCurrentIndex(userCommandCombo->count()-1);        
    }
    else if (!newName.isEmpty())
        QMessageBox::information(pParent, tr("ArpmanetDC"), tr("A user command with that name already exists. Please try another name."));
}

//Pressed remove
void SettingsWidget::removeUserCommandButtonPressed()
{
    if (userCommandCombo->count() == 0)
        return;

    QString name = userCommandCombo->currentText();
    if (QMessageBox::information((QWidget *)pParent, tr("ArpmanetDC"), tr("Are you sure you want to remove the user command <b>%1</b>?")
        .arg(name), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
    {
        //Remove from combobox
        userCommandCombo->removeItem(userCommandCombo->currentIndex());

        //Remove from hash
        pUserCommands->remove(name);

        //Clear views
        userCommandNameLineEdit->clear();
        userCommandOutputLineEdit->clear();
        userCommandParameterCount->setValue(0);

        //Disable views if no commands exist
        if (userCommandCombo->count() == 0)
        {
            renameUserCommandButton->setEnabled(false);
            removeUserCommandButton->setEnabled(false);

            userCommandNameLineEdit->setEnabled(false);
            userCommandOutputLineEdit->setEnabled(false);
            userCommandParameterCount->setEnabled(false);
        }
        else
        {
            switchedUserCommands(userCommandCombo->currentText());            
        }
    }
}

//Switched user commands
void SettingsWidget::switchedUserCommands(const QString &name)
{
    if (pUserCommands->contains(name))
    {
        //Populate fields with data
        UserCommandStruct u = pUserCommands->value(name);

        userCommandNameLineEdit->setText(u.command);
        userCommandOutputLineEdit->setText(u.output);
        userCommandParameterCount->setValue(u.parameterCount);
    }
}

//We should save the current user command info
void SettingsWidget::updateUserCommand()
{
    QString name = userCommandCombo->currentText();
    
    //Check that command name doesn't have any spaces
    QString commandName = userCommandNameLineEdit->text().trimmed();
    
    //Check that the command name isn't used by another user command
    QHashIterator<QString, UserCommandStruct> i(*pUserCommands);
    while (i.hasNext())
    {
        i.next();
        if (i.value().command == commandName && i.key() != name)
        {
            commandName.append("1");
            if (i.hasPrevious())
            {
                i.previous();
            }
        }
    }

    userCommandNameLineEdit->setText(commandName.left(commandName.indexOf(" ")));

    UserCommandStruct u;
    u.command = userCommandNameLineEdit->text();
    u.parameterCount = userCommandParameterCount->value();

    //Check if output contains all necessary parameters - ignore if it contains more than needed
    QString outputStr = userCommandOutputLineEdit->text();
    for (int i = 1; i <= u.parameterCount; ++i)
    {
        if (!outputStr.contains(tr("%%1").arg(i)))
        {
            outputStr.append(tr(" %%1").arg(i));
        }
    }

    u.output = outputStr;
    userCommandOutputLineEdit->setText(outputStr);

    //Update hash
    (*pUserCommands)[name] = u;
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