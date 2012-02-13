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
    QWidget *advancedWidget;
    QListWidget *protocolList;

    QSpinBox *shareUpdateIntervalSpinBox;
    QCheckBox *toggleAdvancedCheckBox;
    QLineEdit *hubAddressLineEdit, *hubPortLineEdit, *nickLineEdit, *passwordLineEdit, *ipLineEdit, *externalPortLineEdit, *downloadPathLineEdit;
    QPushButton *saveButton, *guessIPButton, *browseDownloadPathButton, *protocolUpButton, *protocolDownButton;

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