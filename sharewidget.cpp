#include "sharewidget.h"
#include "customtableitems.h"
#include "arpmanetdc.h"

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

    QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel;
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    QAbstractItemModel *m = containerCombo->model();
    QAbstractItemView *v = containerCombo->view();
    proxyModel->setSourceModel(new QStandardItemModel());
    containerCombo->setModel(proxyModel);
    //containerCombo->view()->setModel(proxyModel);

    ((QStandardItemModel *)((QSortFilterProxyModel *)containerCombo->model())->sourceModel())->appendRow(new QStandardItem(QIcon(":/ArpmanetDC/Resources/CheckIcon.png"), "Default"));
        
    ContainerContentsType contents;
    contents.first = 0;
    pContainerHash.insert(tr("Default"), contents);

    containerModel = new QStandardItemModel(0, 3);
    containerModel->setHeaderData(0, Qt::Horizontal, tr("Filename"));
    containerModel->setHeaderData(1, Qt::Horizontal, tr("Filepath"));
    containerModel->setHeaderData(2, Qt::Horizontal, tr("Filesize"));

    pParentItem = containerModel->invisibleRootItem();

    containerTreeView = new CDropTreeView();
    containerTreeView->setModel(containerModel);
    //containerTreeView->setDragDropMode(QAbstractItemView::DropOnly);
    containerTreeView->setAcceptDrops(true);
    //containerTreeView->setDropIndicatorShown(true);
            
	fileModel = new QFileSystemModel();
	//fileModel->setFilter(QDir::Dirs | QDir::Drives | QDir::NoDotAndDotDot);
	fileModel->setRootPath("c:/");
	fileModel->setResolveSymlinks(false);

	checkProxyModel = new CheckableProxyModel();
	checkProxyModel->setSourceModel(fileModel);
	checkProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
		
	fileTree = new CDragTreeView();
	fileTree->setModel(checkProxyModel);
	//fileTree->sortByColumn(0, Qt::AscendingOrder);
	fileTree->setColumnWidth(0, 500);
	fileTree->setUniformRowHeights(true);
	fileTree->setSortingEnabled(false);
    fileTree->header()->setHighlightSections(false);
    fileTree->setContextMenuPolicy(Qt::CustomContextMenu);
    //fileTree->setDragDropMode(QAbstractItemView::DragOnly);
    //fileTree->setDragEnabled(true);

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
    containerLayout->addWidget(containerTreeView);
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

    //View
    connect(containerTreeView, SIGNAL(droppedURLList(QList<QUrl>)), this, SLOT(droppedURLList(QList<QUrl>)));

    //Combo box
    connect(containerCombo, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(switchedContainer(const QString &)));

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

	//Ensure files in containers are all shared
    foreach (ContainerContentsType c, pContainerHash)
    {
        QHashIterator<QString, quint64> i(c.second);
        while (i.hasNext())
        {
            dirList->append(QDir(i.next().key()));
        }
    }
    	
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
    QString name = QInputDialog::getText((QWidget *)pParent, tr("ArpmanetDC"), tr("Please enter a name for the container"));
    if (!name.isEmpty() && !pContainerHash.contains(name))
    {
        //Add container to hash list
        ContainerContentsType c;
        pContainerHash.insert(name, c);

        //Add container name to combo box
        containerCombo->addItem(QIcon(":/ArpmanetDC/Resources/CheckIcon.png"), name);
        containerCombo->setCurrentIndex(containerCombo->count()-1);

        //Sort list of containers
        containerCombo->view()->model()->sort(0);
        
        //Clear model
        containerModel->removeRows(0, containerModel->rowCount());
    }
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

void ShareWidget::switchedContainer(const QString &name)
{
    //Remove model contents
    containerModel->removeRows(0, containerModel->rowCount());

    //Get previous data from hash
    QHashIterator<QString, quint64> i(pContainerHash.value(name).second);
    while (i.hasNext())
    {
        QFileInfo fi(i.peekNext().key());
        QString fileName = fi.fileName();
        QString filePath = fi.filePath();
        quint64 fileSize = i.peekNext().value();
        if (fileName.isEmpty())
            fileName = filePath;

        //Add contents to model
        QList<QStandardItem *> row;
        QIcon icon;
        
        if (fi.isFile())
        {
            QString suffix = fi.suffix();
            icon = pParent->resourceExtractorObject()->getIconFromName(suffix);
        }
        else if (fi.isDir())
            icon = pParent->resourceExtractorObject()->getIconFromName(tr("folder"));

        row.append(new QStandardItem(icon, fileName));
        row.append(new QStandardItem(filePath));
        row.append(new QStandardItem(tr("%1").arg(fileSize)));

        pParentItem->appendRow(row);  
        
        i.next();
    }

    containerModel->sort(1);
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

//Item dropped in container
void ShareWidget::droppedURLList(QList<QUrl> list)
{
    foreach(QUrl url, list)
    {
        QString path = url.toString();
        QFileInfo fi(path);

        QString fileName = fi.fileName();
        QString filePath = fi.filePath();
        if (fileName.isEmpty())
            fileName = filePath;
        quint64 fileSize = 0;
        if (fi.isFile())
            fileSize = fi.size();

        //Get current contents of container
        ContainerContentsType contents = pContainerHash.value(containerCombo->currentText());
        //Add fileSize to total fileSize
        contents.first += fileSize;

        QHashIterator<QString, quint64> i(contents.second);
        while (i.hasNext())
        {
            QString p = i.next().key();
            if (p == filePath)
                //Don't add duplicates
                return;

            if (filePath.contains(p))
            {
                //Trying to add a lower level folder
                contents.second.remove(p);
                containerModel->removeRow(containerModel->findItems(p, Qt::MatchExactly, 1).first()->row());
            }

            if (p.contains(filePath))
            {
                //Trying to add a higher level folder
                contents.second.remove(p);
                containerModel->removeRow(containerModel->findItems(p, Qt::MatchExactly, 1).first()->row());
            }
        }
        
        //Add the filePath and fileSize to contents
        contents.second.insert(filePath, fileSize);
                
        //Add contents to list
        pContainerHash[containerCombo->currentText()] = contents;
        
        //Add contents to model
        QList<QStandardItem *> row;
        QIcon icon;
        
        if (fi.isFile())
        {
            QString suffix = fi.suffix();
            icon = pParent->resourceExtractorObject()->getIconFromName(suffix);
        }
        else
            icon = pParent->resourceExtractorObject()->getIconFromName(tr("folder"));
            
        row.append(new QStandardItem(icon, fileName));
        row.append(new QStandardItem(filePath));
        row.append(new QStandardItem(tr("%1").arg(fileSize)));

        pParentItem->appendRow(row);      
    }

    containerModel->sort(1);
}

QWidget *ShareWidget::widget()
{
	//TODO: Return widget containing all search widgets
	return pWidget;
}