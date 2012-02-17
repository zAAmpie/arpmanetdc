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
    //Up pressed
    void protocolUpPressed();
    //Down pressed
    void protocolDownPressed();

    //Display advanced settings toggled
    void advancedCheckBoxToggled(int state);

    //Changed the value of the spinbox
    void shareUpdateIntervalSpinBoxValueChanged(int value);

    //Changed the list widget item
    void changePage(QListWidgetItem *current, QListWidgetItem *previous);

signals:
    //Signalled when settings were saved
    void settingsSaved();

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