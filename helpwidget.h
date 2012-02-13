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

#ifndef HELPWIDGET_H
#define HELPWIDGET_H

#include <QtGui>

class ArpmanetDC;

//Displays help
class HelpWidget : public QObject
{
    Q_OBJECT

public:
    HelpWidget(ArpmanetDC *parent);
    ~HelpWidget();

    //Get the encapsulating widget
    QWidget *widget();

    void updateProgress(qint64 done, qint64 total);

private slots:
    void checkForUpdate();

signals:
    void ftpCheckForUpdate();

private:
    //Functions
    void createWidgets();
    void placeWidgets();
    void connectWidgets();

    void addQA(QString question, QString answer);

    //Objects
    QWidget *pWidget;
    ArpmanetDC *pParent;

    QTextBrowser *browser;
    QProgressBar *progress;
    QLabel *updateLabel;
    QPushButton *updateButton;
};

#endif