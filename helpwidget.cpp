#include "helpwidget.h"
#include "arpmanetdc.h"

HelpWidget::HelpWidget(SettingsStruct *settings, ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;
	pSettings = settings;

	createWidgets();
	placeWidgets();
	connectWidgets();
}

HelpWidget::~HelpWidget()
{
	//Destructor
}

void HelpWidget::createWidgets()
{

}

void HelpWidget::placeWidgets()
{
    QLabel *iconLabel = new QLabel();
    iconLabel->setPixmap(QPixmap(":/ArpmanetDC/Resources/ServerIcon.png"));
   
    QHBoxLayout *aboutLayout = new QHBoxLayout();
    aboutLayout->addWidget(iconLabel);
    
    QLabel *aboutLabel = new QLabel("<b><a href=\"http://code.google.com/p/arpmanetdc/\">ArpmanetDC</a></b><br/>Version 0.1 Alpha<br/>Copyright (C) 2012, Arpmanet Community<br/>");
    aboutLabel->setOpenExternalLinks(true);

    aboutLayout->addWidget(aboutLabel);
    aboutLayout->addStretch(1);

    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addLayout(aboutLayout);

    QLabel *linkLabel = new QLabel("<p><font size=5><b>Q: \"Is x out yet?\"</b><br/>A1: <a href=\"http://arpmanet.ath.cx/phpBB2/index.php\">Check the BBS</a><br/>A2: Press the massive Search button at the top left and check</font></p>");
    linkLabel->setOpenExternalLinks(true);
    
    vlayout->addWidget(linkLabel);
    vlayout->addWidget(new QLabel("<p><font size=5><b>Q: \"Does anyone have x?\"</b><br/>A: See above</font></p>"));    
    vlayout->addStretch(1);

	pWidget = new QWidget();
	pWidget->setLayout(vlayout);
}

void HelpWidget::connectWidgets()
{

}

QWidget *HelpWidget::widget()
{
	return pWidget;
}