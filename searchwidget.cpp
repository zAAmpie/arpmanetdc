#include "searchwidget.h"
#include "QCryptographicHash"
#include "arpmanetdc.h"

quint64 SearchWidget::staticID = 0;

SearchWidget::SearchWidget(ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;

	createWidgets();
	placeWidgets();
	connectWidgets();

	pID = staticID++;
}

SearchWidget::~SearchWidget()
{
	//Destructor
}

void SearchWidget::createWidgets()
{
	searchLineEdit = new QLineEdit();
	searchButton = new QPushButton(QIcon(tr(":/ArpmanetDC/Resources/SearchIcon.png")),tr("Search"));
	searchButton->setIconSize(QSize(16,16));

	searchProgress = new QProgressBar();
	searchProgress->setStyle(new QPlastiqueStyle());
	searchProgress->setValue(0);
	searchProgress->setRange(0,0);
	searchProgress->setVisible(false);

	resultsModel = new QStandardItemModel(0, 7);
	//Display
	resultsModel->setHeaderData(0, Qt::Horizontal, tr("Filename"));
	resultsModel->setHeaderData(1, Qt::Horizontal, tr("Hits"));
	resultsModel->setHeaderData(2, Qt::Horizontal, tr("Size"));
	resultsModel->setHeaderData(3, Qt::Horizontal, tr("User"));
	resultsModel->setHeaderData(4, Qt::Horizontal, tr("Type"));
	//Used for connection
	resultsModel->setHeaderData(5, Qt::Horizontal, tr("Exact size"));
	resultsModel->setHeaderData(6, Qt::Horizontal, tr("IP"));
	resultsModel->setHeaderData(7, Qt::Horizontal, tr("TTH root"));

	resultsTable = new QTableView();
	resultsTable->setModel(resultsModel);

	resultsTable->setShowGrid(true);
	resultsTable->setGridStyle(Qt::DotLine);
	resultsTable->verticalHeader()->hide();
	resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	resultsTable->setItemDelegate(new HTMLDelegate(resultsTable));

	//resultsTable->hideColumn(5);
	//resultsTable->hideColumn(6);
	//resultsTable->hideColumn(7);

	pWidget = new QWidget();
}

void SearchWidget::placeWidgets()
{
	//TODO: Place all widgets
	QHBoxLayout *hlayout = new QHBoxLayout;
	hlayout->addSpacing(5);
	hlayout->addWidget(new QLabel(tr("Search for")));
	hlayout->addWidget(searchLineEdit);
	hlayout->addWidget(searchButton);
	hlayout->addStretch(1);
	hlayout->addWidget(searchProgress);

	QVBoxLayout *vlayout = new QVBoxLayout;
	vlayout->addLayout(hlayout);
	vlayout->addWidget(resultsTable);
	vlayout->setContentsMargins(0,5,0,0);

	pWidget->setLayout(vlayout);
}

void SearchWidget::connectWidgets()
{
	//TODO: Connect all widgets
	connect(searchButton, SIGNAL(clicked()), this, SLOT(searchPressed()));
	connect(searchLineEdit, SIGNAL(returnPressed()), this, SLOT(searchPressed()));
}

QByteArray SearchWidget::idGenerator()
{
	//Generate hash from nick/password/time
	QByteArray hash = QByteArray().append(pParent->nick() + pParent->password() + QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss:zzz"));

	QCryptographicHash *cryptHash = new QCryptographicHash(QCryptographicHash::Sha1);
	cryptHash->addData(hash);

	return cryptHash->result();
}

void SearchWidget::addSearchResult(QString res)
{
	//TODO: Parse result and put it into the table
}

void SearchWidget::searchPressed()
{
	//TODO: When search button is pressed
	if (!searchLineEdit->text().isEmpty())
	{
		emit search(pID, searchLineEdit->text(), this);
		searchProgress->setVisible(true);
	}
}

//===== GET FUNCTIONS =====

quint64 SearchWidget::id()
{
	return pID;
}

QWidget *SearchWidget::widget()
{
	return pWidget;
}