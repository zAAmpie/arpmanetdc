#include "helpwidget.h"
#include "arpmanetdc.h"

HelpWidget::HelpWidget(ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;

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
    pWidget = new QWidget();
}

void HelpWidget::placeWidgets()
{
    QLabel *iconLabel = new QLabel(pWidget);
    iconLabel->setPixmap(QPixmap(":/ArpmanetDC/Resources/Logo.png").scaled(100,100, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

    QLabel *gplLabel = new QLabel(pWidget);
    gplLabel->setPixmap(QPixmap(":/ArpmanetDC/Resources/GPLv3 Logo 128px.png").scaled(48,20, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
   
    QHBoxLayout *aboutLayout = new QHBoxLayout();
    aboutLayout->addWidget(iconLabel);
    
    QLabel *aboutLabel = new QLabel(tr("<b><a href=\"http://code.google.com/p/arpmanetdc/\">ArpmanetDC</a></b><br/>Version %1 Alpha<br/>Copyright (C) 2012, Arpmanet Community<p>Free software licenced under <a href=\"http://www.gnu.org/licenses/\">GPLv3</a></p>").arg(VERSION_STRING), pWidget);
    aboutLabel->setOpenExternalLinks(true);

    QVBoxLayout *aboutVLayout = new QVBoxLayout();
    aboutVLayout->addStretch(1);
    aboutVLayout->addWidget(aboutLabel);
    aboutVLayout->addWidget(gplLabel);
    aboutVLayout->addStretch(1);

    aboutLayout->addLayout(aboutVLayout);
    aboutLayout->addStretch(1);

    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addLayout(aboutLayout);

    browser = new QTextBrowser();
    browser->setOpenExternalLinks(true);
    browser->append("<font size=5><b>ArpmanetDC FAQ</b></font><br/>");
    
    //Build FAQ
    quint32 state = pParent->getBoostrapStatus();
    quint32 nodes = pParent->getBootstrapNodeNumber();
    
    if (state <= 1 && nodes == 0)
        addQA("Why can't I search?", "You're not connected to any clients (the numbers at the bottom right of your screen is 1 | 0).<br/>This happens when there are no clients close to you. Please be patient and wait while the client searches for other users. This should only happen the first time you open the client.");
    else if ((state == 2 || state == 3) && nodes < 20)
        addQA("Why can't I search?", tr("You're currently connected to %2 clients (the numbers at the bottom right of your screen is %1 | %2).<br/>Try using a more general search like "".avi"" to test the search function. However, you're not connected to a lot of clients. Please be patient and wait while the client searches for additional users.").arg(state).arg(nodes));
    else
        addQA("Why can't I search?", tr("You're currently connected to %2 clients (the numbers at the bottom right of your screen is %1 | %2).<br/>You're connected to enough clients to utilise the search function fully. Try a more general search.").arg(state).arg(nodes));
    
    addQA("How do I share files with other users?","Click on the Share button and select the files you want to share. Afterwards, click on Save Shares to commit te changes.");
    
    addQA("Why doesn't <i>&lt;insert program feature here&gt;</i> work as expected?","Please understand that this program is still under active development and there are still bugs left. Please report the error and what you were doing directly prior to it being thrown.");
    
    addQA("Is <i>&lt;insert file name here&gt;</i> out yet?","<a href=\"http://arpmanet.ath.cx/phpBB2/index.php\">Check the BBS</a> or press the massive Search button at the top left and check");
    
    addQA("Does anyone have <i>&lt;insert file name here&gt;</i>?","<a href=\"http://arpmanet.ath.cx/phpBB2/index.php\">Check the BBS</a> or press the massive Search button at the top left and check");
    
    addQA("I don't know how the program works?","Please check out the how-to <a href=\"http://fskbhe2.puk.ac.za/pub/Windows/Network/DC/ArpmanetDC/howto/\">here</a>.");
    
    browser->scrollToAnchor("ArpmanetDC");
    vlayout->addWidget(browser);

	pWidget->setLayout(vlayout);
}

void HelpWidget::connectWidgets()
{

}

void HelpWidget::addQA(QString question, QString answer)
{
    browser->append(tr("<font size=3><br/><b>Q: %1</b></font>").arg(question));
    browser->append(tr("<font size=3><b>A:</b> %1</font>").arg(answer));
}

QWidget *HelpWidget::widget()
{
	return pWidget;
}