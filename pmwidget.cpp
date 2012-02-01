#include "pmwidget.h"
#include "arpmanetdc.h"

PMWidget::PMWidget(QString otherNick, ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;
	pOtherNick = otherNick;

	createWidgets();
	placeWidgets();
	connectWidgets();

    userOnline = true;
}

PMWidget::~PMWidget()
{
	//Destructor
}

void PMWidget::createWidgets()
{
    pWidget = new QWidget((QWidget *)pParent);

	//Chat line
	chatLineEdit = new QLineEdit(pWidget);

	//Chat browser
	chatBrowser = new QTextBrowser(pWidget);
	chatBrowser->setOpenExternalLinks(true);
}

void PMWidget::placeWidgets()
{
	//Layouts
	QVBoxLayout *vlayout = new QVBoxLayout;
	vlayout->addWidget(chatBrowser);
	vlayout->addWidget(chatLineEdit);
	vlayout->setContentsMargins(0,0,0,0);

	
	pWidget->setLayout(vlayout);
}

void PMWidget::connectWidgets()
{
	//TODO: Connect all widgets
	connect(chatLineEdit, SIGNAL(returnPressed()), this, SLOT(sendMessage()));
}

void PMWidget::receivePrivateMessage(QString msg)
{
	if (!msg.isEmpty())
	{
		//Change chat user nick format
		//Change mainchat user nick format
	    if (msg.left(4).compare("&lt;") == 0)
	    {
		    QString nick = msg.mid(4,msg.indexOf("&gt;")-4);
		    msg.remove(0,msg.indexOf("&gt;")+4);
		    msg.prepend(tr("<b>%1</b>").arg(nick));
	    }
		//Replace new lines with <br/>
		msg.replace("\n"," <br/>");
		msg.replace("\r","");

		//Replace nick with red text
		msg.replace(pParent->nick(), tr("<font color=\"red\">%1</font>").arg(pParent->nick()));

		//Convert plain text links to HTML links
		pParent->convertHTMLLinks(msg);

		//Convert plain text magnet links
		pParent->convertMagnetLinks(msg);

		//Output chat line with current time
		chatBrowser->append(tr("<b>[%1]</b> %2").arg(QTime::currentTime().toString()).arg(msg));
	}
}

//User login changed
void PMWidget::userLoginChanged(bool loggedIn)
{
    //Set parameter
    userOnline = loggedIn;

    //Show user login changed
    QString info;
    if (userOnline)
        info = tr("<font color=\"darkgray\">User now online</font>");
    else
        info = tr("<font color=\"darkgray\">User went offline</font>");
    chatBrowser->append(tr("<b>[%1]</b> %2").arg(QTime::currentTime().toString()).arg(info));
}

void PMWidget::sendMessage()
{
    //Check if user is actually online
    if (!userOnline)
    {
        //Output line with current time
        QString info = tr("<font color=\"darkgray\">User currently offline. Personal message wil not be sent.</font>");
		chatBrowser->append(tr("<b>[%1]</b> %2").arg(QTime::currentTime().toString()).arg(info));

        chatLineEdit->setText("");

        //Don't send PM - useless anyway
        return;
    }

	QString msg = chatLineEdit->text();
	chatLineEdit->setText("");

	emit sendPrivateMessage(pOtherNick, msg, this);

    msg.replace("<", "&lt;");
    msg.replace(">", "&gt;");
    msg.replace('"', "&quot;");
	receivePrivateMessage(tr("&lt;%1&gt; %2").arg(pParent->nick()).arg(msg));
}

QWidget *PMWidget::widget()
{
	return pWidget;
}

QString PMWidget::otherNick()
{
	return pOtherNick;
}