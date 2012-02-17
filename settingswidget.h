/* This file is part of ArpmanetDC. Copyright (C) 2012
 * Source code can be found at http://code.google.com/p/arpmanetdc/
 * 
 * ArpmanetDC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ArpmanetDC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with ArpmanetDC.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include <QtGui>

struct UserCommandStruct //Used to store/return users commands from the db
{
    QString command;
    int parameterCount;
    QString output;
};

class ArpmanetDC;

//Class encapsulating all widgets/signals for search tab
class SettingsWidget : public QObject
{
    Q_OBJECT

public:
    SettingsWidget(QHash<QString, QString> *settings, QHash<QString, UserCommandStruct> *commands, ArpmanetDC *parent);
    ~SettingsWidget();

    //Get the encapsulating widget
    QWidget *widget();

    QHash<QString, QString> *settings() const;

public slots:
    //Slots
    void returnUserCommands(QHash<QString, UserCommandStruct> *commands);

private slots:
    //Pressed save button
    void savePressed();

    //===== GENERAL OPTIONS =====
    //Pressed download path browse button
    void browseDownloadPathPressed();

    //===== ADVANCED OPTIONS =====
    //Pressed guess IP button
    void guessIPPressed();
    //Up pressed
    void protocolUpPressed();
    //Down pressed
    void protocolDownPressed();

    //===== USER COMMANDS =====
    //Pressed add user command
    void addUserCommandButtonPressed();
    //Pressed rename
    void renameUserCommandButtonPressed();
    //Pressed remove
    void removeUserCommandButtonPressed();
    //Switched user commands
    void switchedUserCommands(const QString &name);
    //We should save the current user command info
    void updateUserCommand();

    //Display advanced settings toggled
    void advancedCheckBoxToggled(int state);

    //Changed the value of the spinbox
    void shareUpdateIntervalSpinBoxValueChanged(int value);

    //Changed the list widget item
    void changePage(QListWidgetItem *current, QListWidgetItem *previous);

signals:
    //Signalled when settings were saved
    void settingsSaved();

    //Signal to get user commands
    void requestUserCommands();

    //Signal to save user commands
    void saveUserCommands(QHash<QString, UserCommandStruct> *commands);

private:
    //Functions
    void createWidgets();
    void placeWidgets();
    void connectWidgets();

    QWidget *createGeneralSettingsPage();
    QWidget *createAdvancedSettingsPage();
    QWidget *createUserCommandsPage();

    void createPageIcons();

    //Objects
    QWidget *pWidget;
    ArpmanetDC *pParent;

    QHash<QString, QString> *pSettings;
    QHash<QString, UserCommandStruct> *pUserCommands;

    //GUI pages
    QWidget *advancedWidget;
    QWidget *generalWidget;
    QWidget *userCommandWidget;

    //Main widgets
    QCheckBox *toggleAdvancedCheckBox;
    QPushButton *saveButton;
    QListWidget *contentsWidget;
    QStackedWidget *pagesWidget;

    //Content list widgets
    QListWidgetItem *advancedPageButton, *generalPageButton, *userCommandsPageButton;

    //General settings widgets
    QSpinBox *shareUpdateIntervalSpinBox;
    QLineEdit *hubAddressLineEdit, *hubPortLineEdit, *nickLineEdit, *passwordLineEdit, *downloadPathLineEdit;
    QPushButton *browseDownloadPathButton;

    //Advanced settings widgets
    QLineEdit *ipLineEdit, *externalPortLineEdit;
    QListWidget *protocolList;
    QPushButton *guessIPButton, *protocolUpButton, *protocolDownButton;

    //User command settings widgets
    QComboBox *userCommandCombo;
    QPushButton *addUserCommandButton, *renameUserCommandButton, *removeUserCommandButton;
    QLineEdit *userCommandNameLineEdit, *userCommandOutputLineEdit;
    QSpinBox *userCommandParameterCount;
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