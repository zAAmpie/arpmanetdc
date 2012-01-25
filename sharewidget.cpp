#include "sharewidget.h"
#include "customtableitems.h"

ShareWidget::ShareWidget(ShareSearch *share, ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;
	pShare = share;

    connect(this, SIGNAL(updateShares(QList<QDir> *)), pShare, SLOT(updateShares(QList<QDir> *)), Qt::QueuedConnection);
	connect(this, SIGNAL(updateShares()), pShare, SLOT(updateShares()), Qt::QueuedConnection);
    connect(this, SIGNAL(requestTTHFromPath(QString)), pShare, SLOT(requestTTHFromPath(QString)), Qt::QueuedConnection);
    connect(pShare, SIGNAL(returnTTHFromPath(QString, QByteArray, quint64)), this, SLOT(returnTTHFromPath(QString, QByteArray, quint64)), Qt::QueuedConnection);

	createWidgets();
	placeWidgets();
	connectWidgets();

	//Populate share list
	QList<QDir> *shares = pShare->getShares();
	while (!shares->isEmpty())
	{
		QDir currentPath = shares->takeFirst();
		pSharesList.append(currentPath.absolutePath());
        changeRoot(currentPath.absolutePath());
		while (currentPath.cdUp())
			changeRoot(currentPath.absolutePath());

		//QModelIndex index = fileModel->index(currentPath.absolutePath(), 0);
		//bool res = checkProxyModel->setSourceIndexCheckedState(index, true);
	}
    finishedLoading = true;
}

ShareWidget::~ShareWidget()
{
	//Destructor
	fileTree->deleteLater();
	fileModel->deleteLater();
	checkProxyModel->deleteLater();
}

void ShareWidget::createWidgets()
{
	saveButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/CheckIcon.png"), tr("Save shares"));
	refreshButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/RefreshIcon.png"), tr("Refresh shares"));
    containerButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/LowPriorityIcon.png"), tr("Show Containers"));
    
    addContainerButton = new QPushButton(tr("Add"));
    addContainerButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    removeContainerButton = new QPushButton(tr("Delete"));
    removeContainerButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    
    containerCombo = new QComboBox();
    containerCombo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    containerCombo->addItem(QIcon(":/ArpmanetDC/Resources/CheckIcon.png"), tr("Test Container"));

    containerListWidget = new QListWidget();
    containerListWidget->addItem(tr("Not yet implemented. Stay tuned"));
        
	fileModel = new QFileSystemModel();
	//fileModel->setFilter(QDir::Dirs | QDir::Drives | QDir::NoDotAndDotDot);
	fileModel->setRootPath("c:/");
	fileModel->setResolveSymlinks(false);

	checkProxyModel = new CheckableProxyModel();
	checkProxyModel->setSourceModel(fileModel);
	checkProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
		
	fileTree = new QTreeView();
	fileTree->setModel(checkProxyModel);
	//fileTree->sortByColumn(0, Qt::AscendingOrder);
	fileTree->setColumnWidth(0, 500);
	fileTree->setUniformRowHeights(true);
	fileTree->setSortingEnabled(false);
    fileTree->header()->setHighlightSections(false);
    fileTree->setContextMenuPolicy(Qt::CustomContextMenu);

	checkProxyModel->setDefaultCheckState(Qt::Unchecked);	
	//checkProxyModel->sort(0, Qt::AscendingOrder);

    busyLabel = new QLabel(tr("<font color=\"red\">Busy loading directory structure. Please wait...</font>"));

    //Context menu
    contextMenu = new QMenu((QWidget *)pParent);
    calculateMagnetAction = new QAction(QIcon(":/ArpmanetDC/Resources/MagnetIcon.png"), tr("Copy magnet link"), (QWidget *)pParent);
    contextMenu->addAction(calculateMagnetAction);
}

void ShareWidget::placeWidgets()
{
	QHBoxLayout *hlayout = new QHBoxLayout;
	hlayout->addWidget(refreshButton);
    hlayout->addWidget(containerButton);
	hlayout->addSpacing(10);
    hlayout->addWidget(busyLabel);
	hlayout->addStretch(1);
	hlayout->addWidget(saveButton);

    QGridLayout *topContainerLayout = new QGridLayout;
    topContainerLayout->addWidget(containerCombo, 0, 0);
    topContainerLayout->addWidget(addContainerButton, 0, 1);
    topContainerLayout->addWidget(removeContainerButton, 0, 2);

    QVBoxLayout *containerLayout = new QVBoxLayout;
    containerLayout->addLayout(topContainerLayout);
    containerLayout->addWidget(containerListWidget);
    containerLayout->setContentsMargins(0,0,0,0);

    QWidget *containerWidget = new QWidget();
    containerWidget->setLayout(containerLayout);
    containerWidget->setContentsMargins(0,0,0,0);

    splitter = new QSplitter(Qt::Horizontal, (QWidget *)pParent);
    splitter->addWidget(fileTree);
    splitter->addWidget(containerWidget);

	QVBoxLayout *vlayout = new QVBoxLayout;
	//vlayout->addLayout(hTreeLayout);
    vlayout->addWidget(splitter);
	vlayout->addLayout(hlayout);
	vlayout->setContentsMargins(0,0,0,0);

    splitter->widget(1)->hide();
    splitter->setCollapsible(0,false);
    splitter->setCollapsible(1, false);
       
	pWidget = new QWidget((QWidget *)pParent);
	pWidget->setLayout(vlayout);

    QList<int> sizeList;
    int halfWidth = ((QWidget *)pParent)->size().width()/2;
    sizeList << halfWidth << halfWidth;
    splitter->setSizes(sizeList);
}

void ShareWidget::connectWidgets()
{
    //Context menu
    connect(fileTree, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(contextMenuRequested(const QPoint &)));

	//Models
	connect(checkProxyModel, SIGNAL(checkedNodesChanged()), this, SLOT(selectedItemsChanged()));
	connect(fileModel, SIGNAL(directoryLoaded(QString)), this, SLOT(pathLoaded(QString)));

    //Buttons
    connect(saveButton, SIGNAL(clicked()), this, SLOT(saveSharePressed()));
	connect(refreshButton, SIGNAL(clicked()), this, SLOT(refreshButtonPressed()));
    connect(containerButton, SIGNAL(clicked()), this, SLOT(containerButtonPressed()));
    connect(addContainerButton, SIGNAL(clicked()), this, SLOT(addContainerButtonPressed()));
    connect(removeContainerButton, SIGNAL(clicked()), this, SLOT(removeContainerButtonPressed()));

    //Actions
    connect(calculateMagnetAction, SIGNAL(triggered()), this, SLOT(calculateMagnetActionPressed()));
}

void ShareWidget::changeRoot(QString path)
{
    QFileInfo fi(path);
    if (fi.isDir())
        pLoadingPaths.append(path);

	fileModel->setRootPath(path);
	//QModelIndex index = checkProxyModel->index(0,0, fileModel->index(path,0));
	//QModelIndex index2 = checkProxyModel->mapFromSource(fileModel->index(path));
	
	if (pSharesList.contains(path))
		checkProxyModel->setSourceIndexCheckedState(fileModel->index(path,0), true);
	//fileTree->setRootIndex(index);
}

void ShareWidget::pathLoaded(QString path)
{
    pLoadingPaths.removeAll(path);
    pLoadingPaths.removeAll(path + "/");

    //Check if loading is complete
    if (pLoadingPaths.isEmpty() && finishedLoading)
    {
        busyLabel->hide();   
    }
}

void ShareWidget::selectedItemsChanged()
{
	//Selection changed in the checked proxy model
}

void ShareWidget::saveSharePressed()
{
	QModelIndexList selectedFiles;
    QModelIndexList selectedDirectories;

	//Extract selected files and directories
    CheckableProxyModelState *state = checkProxyModel->checkedState();
	state->checkedLeafSourceModelIndexes(selectedFiles);
	state->checkedBranchSourceModelIndexes(selectedDirectories);
	delete state;

	//Construct list of selected directory paths
	QList<QDir> *dirList = new QList<QDir>();
	foreach (const QModelIndex index, selectedDirectories)
		dirList->append(QDir(index.data(QFileSystemModel::FilePathRole).toString()));
	foreach (const QModelIndex index, selectedFiles)
		dirList->append(QDir(index.data(QFileSystemModel::FilePathRole).toString()));

	//Generate list of directories
	
	/*//Show info
	QString list = QString("<b>Selected files (%1):</b><ul>").arg(selectedFiles.count());

    foreach (const QModelIndex index, selectedFiles) {
        list += "<li>" + index.data(QFileSystemModel::FilePathRole).toString() + "</li>";
    }

    list += QString("</ul><br/><b>Selected directories (%1):</b><ul>").arg(selectedDirectories.count());
    foreach (const QModelIndex index, selectedDirectories) {
        list += "<li>" + index.data(QFileSystemModel::FilePathRole).toString() + "</li>";
    }
    list += "</ul>";

	QMessageBox::information(0, tr("ArpmanetDC"), list);*/
	
   	//pShare->updateShares(dirList);
    pShare->stopParsing();
    pShare->stopHashing();
    emit updateShares(dirList);

	emit saveButtonPressed();
}

void ShareWidget::refreshButtonPressed()
{
	emit updateShares();
	emit saveButtonPressed();
}

void ShareWidget::containerButtonPressed()
{
    if (splitter->widget(1)->isHidden())
    {
        splitter->widget(1)->setVisible(true);
        containerButton->setText(tr("Hide Containers"));
    }
    else
    {
        splitter->widget(1)->hide();
        containerButton->setText(tr("Show Containers"));
    }
}

void ShareWidget::addContainerButtonPressed()
{

}

void ShareWidget::removeContainerButtonPressed()
{

}

void ShareWidget::calculateMagnetActionPressed()
{
    QModelIndex index = fileTree->selectionModel()->selectedRows().first();
    
    QString filePath = fileModel->filePath(checkProxyModel->mapToSource(index));
    
    emit requestTTHFromPath(filePath);

    QWhatsThis::showText(contextMenu->pos(), tr("Calculating hash. Please wait..."));
}

void ShareWidget::returnTTHFromPath(QString filePath, QByteArray tthRoot, quint64 fileSize)
{
    if (!tthRoot.isEmpty())
    {
        //TTH was found in database
        QByteArray base32TTH(tthRoot);
        base32Encode(base32TTH);
        
        QString tthStr(base32TTH);

        QFileInfo fi(filePath);
        
        //Generate magnet link
        QString fileName = fi.fileName();
        QString magnetLink = tr("magnet:?xt=urn:tree:tiger:%1&xl=%2&dn=%3").arg(tthStr).arg(fileSize).arg(fileName.replace(" ", "+"));
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(magnetLink);

        QWhatsThis::showText(contextMenu->pos(), tr("Magnet link copied to clipboard"));
    }
}

//Context menu for magnets
void ShareWidget::contextMenuRequested(const QPoint &pos)
{
    if (fileTree->selectionModel()->selectedRows().size() == 0)
        return;

    QModelIndex index = fileTree->selectionModel()->selectedRows().first();
    QString filePath = fileModel->filePath(checkProxyModel->mapToSource(index));
    QFileInfo fi(filePath);
    if (fi.isFile())
    {
        QPoint globalPos = fileTree->viewport()->mapToGlobal(pos);
        contextMenu->popup(globalPos);
    }
}

QWidget *ShareWidget::widget()
{
	//TODO: Return widget containing all search widgets
	return pWidget;
}