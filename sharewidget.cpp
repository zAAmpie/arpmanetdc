#include "sharewidget.h"
#include "customtableitems.h"

ShareWidget::ShareWidget(ShareSearch *share, ArpmanetDC *parent)
{
	//Constructor
	pParent = parent;
	pShare = share;

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
	saveButton = new QPushButton(QIcon(""), tr("Save shares"));

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

	checkProxyModel->setDefaultCheckState(Qt::Unchecked);	
	//checkProxyModel->sort(0, Qt::AscendingOrder);
}

void ShareWidget::placeWidgets()
{
	QHBoxLayout *hlayout = new QHBoxLayout;
	hlayout->addStretch(1);
	hlayout->addWidget(saveButton);

	QVBoxLayout *vlayout = new QVBoxLayout;
	vlayout->addWidget(fileTree);
	vlayout->addLayout(hlayout);
	vlayout->setContentsMargins(0,0,0,0);

	pWidget = new QWidget((QWidget *)pParent);
	pWidget->setLayout(vlayout);
}

void ShareWidget::connectWidgets()
{
	//TODO: Connect all widgets
	connect(checkProxyModel, SIGNAL(checkedNodesChanged()), this, SLOT(selectedItemsChanged()));
	connect(saveButton, SIGNAL(clicked()), this, SLOT(saveSharePressed()));
	//connect(fileModel, SIGNAL(directoryLoaded(QString)), this, SLOT(modelDirectoryLoaded(QString)));
}

void ShareWidget::changeRoot(QString path)
{
	fileModel->setRootPath(path);
	//QModelIndex index = checkProxyModel->index(0,0, fileModel->index(path,0));
	//QModelIndex index2 = checkProxyModel->mapFromSource(fileModel->index(path));
	
	if (pSharesList.contains(path))
		checkProxyModel->setSourceIndexCheckedState(fileModel->index(path,0), true);
	//fileTree->setRootIndex(index);
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
	
   	pShare->updateShares(dirList);

	emit saveButtonPressed();
}

QWidget *ShareWidget::widget()
{
	//TODO: Return widget containing all search widgets
	return pWidget;
}