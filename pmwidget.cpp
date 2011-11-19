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
}

PMWidget::~PMWidget()
{
	//Destructor
}

void PMWidget::createWidgets()
{
	//Chat line
	chatLineEdit = new QLineEdit();

	//Chat browser
	chatBrowser = new QTextBrowser();
	chatBrowser->setOpenExternalLinks(true);
}

void PMWidget::placeWidgets()
{
	//Layouts
	QVBoxLayout *vlayout = new QVBoxLayout;
	vlayout->addWidget(chatBrowser);
	vlayout->addWidget(chatLineEdit);
	vlayout->setContentsMargins(0,0,0,0);

	pWidget = new QWidget();
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
		if (msg.left(1).compare("<") == 0)
		{
			QString nick = msg.mid(1,msg.indexOf(">")-1);
			msg.remove(0,msg.indexOf(">")+1);
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

void PMWidget::sendMessage()
{
	QString msg = chatLineEdit->text();
	chatLineEdit->setText("");

	receivePrivateMessage(tr("<%1> %2").arg(pParent->nick()).arg(msg));

	emit sendPrivateMessage(pOtherNick, msg, this);
}

QWidget *PMWidget::widget()
{
	return pWidget;
}

QString PMWidget::otherNick()
{
	return pOtherNick;
}