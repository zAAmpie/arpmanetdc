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

#ifndef PMWIDGET_H
#define PMWIDGET_H

#include <QtGui>
class ArpmanetDC;

//Class encapsulating all widgets/signals for search tab
class PMWidget : public QObject
{
	Q_OBJECT

public:
	PMWidget(QString otherNick, ArpmanetDC *parent);
	~PMWidget();

	//Get the encapsulating widget
	QWidget *widget();

	//Get functions
	QString otherNick();

public slots:
	//Slots for incoming PM message
	void receivePrivateMessage(QString message);

    //User login changed
    void userLoginChanged(bool loggedIn);

private slots:
	//Slot for when user presses return
	void sendMessage();

signals:
	//Signal for outgoing PM message
	void sendPrivateMessage(QString nick, QString message, PMWidget *pmWidget);

private:
	//Functions
	void createWidgets();
	void placeWidgets();
	void connectWidgets();

	//Objects
	QWidget *pWidget;
	ArpmanetDC *pParent;

	//Parameters
	QString pOtherNick;
    bool userOnline;

	//GUI elements
	QTextBrowser *chatBrowser;
	QLineEdit *chatLineEdit;
};

#endif

