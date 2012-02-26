#include "displaycontainerwidget.h"
#include "arpmanetdc.h"

//Constructor
DisplayContainerWidget::DisplayContainerWidget(QHostAddress host, ContainerContentsType index, QList<ContainerLookupReturnStruct> contents, QString name, ArpmanetDC *parent)
{
    //Set objects
    pParent = parent;
    pIndex = index;
    pHost = host;
    pContents = contents;
    pName = name;

    //Generate widget
    createWidgets();
    placeWidgets();
    connectWidgets();

    //Populate the tree with container data
    populateTree();
}

//Constructor
DisplayContainerWidget::DisplayContainerWidget(ArpmanetDC *parent)
{
    //Set objects
    pParent = parent;
    
    //Generate widget
    createWidgets();
    placeWidgets();
    connectWidgets();
}

//Destructor
DisplayContainerWidget::~DisplayContainerWidget()
{

}

//Functions to generate GUI
void DisplayContainerWidget::createWidgets()
{
    pWidget = new QWidget((QWidget *)pParent);
    
    //Model
    containerModel = new QStandardItemModel(0, 5, this);
    containerModel->setHeaderData(0, Qt::Horizontal, tr("Name"));
    containerModel->setHeaderData(1, Qt::Horizontal, tr("Path"));
    containerModel->setHeaderData(2, Qt::Horizontal, tr("Size"));
    containerModel->setHeaderData(3, Qt::Horizontal, tr("Bytes"));
    containerModel->setHeaderData(4, Qt::Horizontal, tr("TTH"));

    pParentItem = containerModel->invisibleRootItem();

    //Check model
    checkProxyModel = new CheckableProxyModel(this);
    checkProxyModel->setSourceModel(containerModel);
    checkProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    
    //Tree view
    containerTreeView = new QTreeView(pWidget);
    containerTreeView->setModel(checkProxyModel);
    containerTreeView->setUniformRowHeights(true);
    containerTreeView->header()->setHighlightSections(false);
    containerTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    containerTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    containerTreeView->setColumnWidth(0, 300);
    containerTreeView->setColumnWidth(1, 300);
    //containerTreeView->hideColumn(1);

    checkProxyModel->setDefaultCheckState(Qt::Unchecked);

    //Labels
    containerNameLabel = new QLabel(tr("Name: %1").arg(pName), pWidget);
    containerSizeLabel = new QLabel(tr("Size: %1").arg(bytesToSize(pIndex.first)), pWidget);
    selectedFileSizeLabel = new QLabel(tr("Selected file size: 0.00 bytes"), pWidget);
    selectedFileCountLabel = new QLabel(tr("Selected files: 0"), pWidget);

    //Buttons
    openContainerButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/FolderIcon.png"), tr("Open container"), pWidget);
    openContainerButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    downloadSelectedFilesButton = new QPushButton(QIcon(":/ArpmanetDC/Resources/DownloadIcon.png"), tr("Download selected files"), pWidget);
    downloadSelectedFilesButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void DisplayContainerWidget::placeWidgets()
{
    QHBoxLayout *topLayout = new QHBoxLayout;
    topLayout->addSpacing(5);
    topLayout->addWidget(containerNameLabel, 1);
    topLayout->addWidget(containerSizeLabel, 0);
    topLayout->addWidget(openContainerButton, 0);
    topLayout->addSpacing(5);

    QHBoxLayout *bottomLayout = new QHBoxLayout;
    bottomLayout->addStretch(1);
    bottomLayout->addWidget(selectedFileSizeLabel, 0);
    bottomLayout->addWidget(selectedFileCountLabel, 0);
    bottomLayout->addWidget(downloadSelectedFilesButton, 0);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addLayout(topLayout, 0);
    layout->addWidget(containerTreeView, 1);
    layout->addLayout(bottomLayout, 0);
    layout->setContentsMargins(0,5,0,0);

    pWidget->setLayout(layout);
}

void DisplayContainerWidget::connectWidgets()
{
    //Connect widgets...
}

//Populate tree with contents
void DisplayContainerWidget::populateTree()
{
    //Associate the root folder with the invisible parent item
    pPathItemHash.insert("/", pParentItem);

    QListIterator<ContainerLookupReturnStruct> i(pContents);
    while (i.hasNext())
    {
        ContainerLookupReturnStruct c = i.next();
        
        int pos = c.filePath.indexOf("/");
        bool isDirectory = (pos != -1);

        QString parentPath = "/";
        QString dirPath;
        QString dirName;
        QString totalPath;

        //If not a directory
        if (pos == -1)
        {
            dirPath = "/";
            totalPath = "/";
            dirName = "/";
        }
        //If a directory
        else
        {
            dirPath = c.filePath.left(pos); //Get directory path
        
            dirName = dirPath.mid(dirPath.lastIndexOf("/")+1);
            dirPath += "/"; //Add the / to the end of the path

            totalPath = c.filePath.left(c.filePath.lastIndexOf("/")+1);
        }        

        while (isDirectory)
        {
            if (!pPathItemHash.contains(dirPath))
            {
                //Insert the directory path into the hash along with its respective item
                QList<QStandardItem *> row;
                QString iconName = tr("folder");
                row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, dirName, pParent->resourceExtractorObject()->getIconFromName(iconName)));
                row.append(new CStandardItem(CStandardItem::PathType, dirPath));
                row.append(new CStandardItem(CStandardItem::SizeType, ""));
                row.append(new CStandardItem(CStandardItem::IntegerType, ""));
                row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, ""));
            
                pPathItemHash.insert(dirPath, row.first());
            
                //Add the row to parent item
                pPathItemHash.value(parentPath)->appendRow(row);
            }

            //Go down one directory
            pos = c.filePath.indexOf("/", pos+1);
            if (pos == -1)
            {
                isDirectory = false;
                break;
            }
            
            parentPath = dirPath;
            dirPath = c.filePath.left(pos); //Get directory path
            dirName = dirPath.mid(dirPath.lastIndexOf("/")+1);
            dirPath += "/"; //Add the / to the end of the path
        }
        
        //Insert item as child of correct item
        QFileInfo fi(c.filePath);
        QString suffix = fi.suffix();
        QList<QStandardItem *> row;

        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, fi.fileName(), pParent->resourceExtractorObject()->getIconFromName(suffix)));
        row.append(new CStandardItem(CStandardItem::PathType, c.filePath));
        row.append(new CStandardItem(CStandardItem::SizeType, bytesToSize(c.fileSize)));
        row.append(new CStandardItem(CStandardItem::IntegerType, tr("%1").arg(c.fileSize)));
        
        QByteArray base32TTH(c.rootTTH);
        base32Encode(base32TTH);
        
        row.append(new CStandardItem(CStandardItem::CaseInsensitiveTextType, base32TTH.data()));

        pPathItemHash.insert(c.filePath, row.first());
        pPathItemHash.value(totalPath)->appendRow(row);
    }

    containerModel->sort(1);
}

//Get the encapsulating widget
QWidget *DisplayContainerWidget::widget()
{
    //Return widget containing all search widgets
    return pWidget;
}
