#include "helpwidget.h"
#include "arpmanetdc.h"

HelpWidget::HelpWidget(ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;

    connect(this, SIGNAL(ftpCheckForUpdate()), pParent, SIGNAL(ftpCheckForUpdate()));

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

    progress = new QProgressBar();
    progress->setVisible(false);
    progress->setStyle(new QPlastiqueStyle());
    
    updateLabel = new QLabel(tr("Downloading..."));
    updateLabel->setVisible(false);

    updateButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/RefreshIcon.png"), tr("Check for updates"));

    browser = new QTextBrowser();
    browser->setOpenExternalLinks(true);

    //Build FAQ
    browser->append("<font size=5><b>ArpmanetDC FAQ</b></font><br/>");

    quint32 state = pParent->getBoostrapStatus();
    quint32 nodes = pParent->getBootstrapNodeNumber();
    
    if (state <= 1 && nodes == 0)
        addQA("Why can't I search?", "You're not connected to any clients (the numbers at the bottom right of your screen is 1 | 0).<br/>This happens when there are no clients close to you. Please be patient and wait while the client searches for other users. This should only happen the first time you open the client.");
    else if ((state == 2 || state == 3) && nodes < 20)
        addQA("Why can't I search?", tr("You're currently connected to %2 clients (the numbers at the bottom right of your screen is %1 | %2).<br/>Try using a more general search like "".avi"" to test the search function. However, you're not connected to a lot of clients. Please be patient and wait while the client searches for additional users.").arg(state).arg(nodes));
    else
        addQA("Why can't I search?", tr("You're currently connected to %2 clients (the numbers at the bottom right of your screen is %1 | %2).<br/>You're connected to enough clients to utilise the search function fully. <br/>Some things to try:<br/>1) Try a more general search<br/>2) Ensure your firewall is turned off or an exception exists for the client<br/>3) Ensure that no other running applications is currently using port 4012 UDP<br/>4) Ensure the External IP in Advanced Settings is your LAN IP.").arg(state).arg(nodes));
    
    addQA("How do I share files with other users?","Click on the Share button and select the files you want to share. Afterwards, click on Save Shares to commit te changes.");
    
    addQA("Why doesn't <i>&lt;insert program feature here&gt;</i> work as expected?","Please understand that this program is still under active development and there are still bugs left. Please report the error and what you were doing directly prior to it being thrown.");
    
    addQA("Is <i>&lt;insert file name here&gt;</i> out yet?","<a href=\"http://arpmanet.ath.cx/phpBB2/index.php\">Check the BBS</a> or press the massive Search button at the top left and check");
    
    addQA("Does anyone have <i>&lt;insert file name here&gt;</i>?","<a href=\"http://arpmanet.ath.cx/phpBB2/index.php\">Check the BBS</a> or press the massive Search button at the top left and check");
    
    addQA("I don't know how the program works?","Please check out the how-to <a href=\"http://fskbhe2.puk.ac.za/pub/Windows/Network/DC/ArpmanetDC/howto/\">here</a>.");
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

    QVBoxLayout *progressLayout = new QVBoxLayout;
    progressLayout->setAlignment(Qt::AlignCenter);
    progressLayout->addStretch(1);
    progressLayout->addWidget(updateLabel);
    progressLayout->addWidget(progress);
    progressLayout->addWidget(updateButton);
    //progressLayout->addStretch(1);

    aboutLayout->addLayout(aboutVLayout);
    aboutLayout->addStretch(1);
    aboutLayout->addLayout(progressLayout);

    QVBoxLayout *vlayout = new QVBoxLayout();
    vlayout->addLayout(aboutLayout);

    browser->scrollToAnchor("ArpmanetDC");
    vlayout->addWidget(browser);

	pWidget->setLayout(vlayout);
}

void HelpWidget::connectWidgets()
{
    connect(updateButton, SIGNAL(clicked()), this, SLOT(checkForUpdate()));
}

void HelpWidget::updateProgress(qint64 done, qint64 total)
{
    updateLabel->setText(tr("Downloading..."));
    if (done < 0)
    {
        updateLabel->setVisible(true);
        updateLabel->setText(tr("Client is up to date"));
        progress->setVisible(false);
    }
    else if (done == total)
    {
        progress->setVisible(false);
        updateLabel->setVisible(false);
    }
    else
    {
        progress->setVisible(true);
        updateLabel->setVisible(true);
    }

    int val = (done * 100) / total;    
    progress->setValue(val);
}

void HelpWidget::checkForUpdate()
{
    progress->setVisible(true);
    updateLabel->setVisible(true);

    updateLabel->setText(tr("Checking for updates..."));
    progress->setRange(0,0);
    
    emit ftpCheckForUpdate();
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