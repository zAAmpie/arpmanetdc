#include "resourceextractor.h"

//Constructor
ResourceExtractor::ResourceExtractor(QObject *parent) : QObject(parent)
{
    pIconSize = 16;
    pPath = ":/ArpmanetDC/Resources/TypeIcons.png";

    initIconList(pPath, pIconSize);

    //Icons: file | audio | archive | doc | application | image | video | folder | unknown | pdf | excel
    QList<QStringList> typeList;
    QStringList list;
    list << "txt" << "cfg"; //Text Files
    typeList.append(list);
    list.clear();
    list << "wav" << "mp3" << "aac" << "flac" << "mid" << "ogg"; //Audio
    typeList.append(list);
    list.clear();
    list << "rar" << "zip" << "cab" << "bz2" << "tar" << "gzip" << "ace" << "z"; //Archives
    typeList.append(list);
    list.clear();
    list << "doc" << "docx"; //Documents
    typeList.append(list);
    list.clear();
    list << "exe" << "msi"; //Applications
    typeList.append(list);
    list.clear();
    list << "jpg" << "bmp" << "jpeg" << "png" << "ico" << "tif" << "gif"; //Images
    typeList.append(list);
    list.clear();
    list << "mp4" << "avi" << "ts" << "wmv" << "mkv" << "flv" << "mov" << "mpg" << "mpeg"; //Videos
    typeList.append(list);
    list.clear();
    list << "folder"; //Folders
    typeList.append(list);
    list.clear();
    list << "unknown"; //Unknown
    typeList.append(list);
    list.clear();
    list << "pdf"; //PDF
    typeList.append(list);
    list.clear();
    list << "xls" << "xlsm" << "xlsx" << "csv"; //Excel
    typeList.append(list);
    list.clear();
    list << "iso" << "bin" << "mdf" << "nrg" << "img"; //Images
    typeList.append(list);
    list.clear();
    list << "adcc"; //ArpmanetDC containers
    typeList.append(list);

    mapToIconList(typeList);
}

ResourceExtractor::ResourceExtractor(const QString &path, quint16 iconSize, QObject *parent) : QObject(parent)
{
    pIconSize = iconSize;
    pPath = path;

    initIconList(pPath, pIconSize);
}

//Convenience constructor
ResourceExtractor::ResourceExtractor(const QString &path, QStringList &list, quint16 iconSize, QObject *parent) : QObject(parent)
{
    pIconSize = iconSize;
    pPath = path;

    initIconList(pPath, pIconSize);

    mapToIconList(list);
}

//Convenience constructor
ResourceExtractor::ResourceExtractor(const QString &path, QList<QStringList> &list, quint16 iconSize, QObject *parent) : QObject(parent)
{
    pIconSize = iconSize;
    pPath = path;

    initIconList(pPath, pIconSize);

    mapToIconList(list);
}

//Destructor
ResourceExtractor::~ResourceExtractor()
{

}

//Initializes a list of icons contained in path
bool ResourceExtractor::initIconList(const QString &path, quint16 iconSize)
{
    if (path.isEmpty())
        return false;

    pIconList.clear();
    pMappedIconList.clear();

    //Load image from path
    QImage image(path);
    if (image.isNull())
        return false;

    //Determine number of icons
    int width = image.width();
    int numIcons = width / iconSize;

    if (numIcons == 0)
        return false;

    //Iterate and load all icons
    for (int i = 0; i < numIcons; i++)
    {
        pIconList.append(new QIcon(QPixmap::fromImage(image.copy(i*iconSize, 0, iconSize, iconSize))));
    }

    return true;
}

//Maps a list of names to a list of icons 1 to 1
bool ResourceExtractor::mapToIconList(const QStringList &list)
{
    if (list.isEmpty())
        return false;

    for (int i = 0; i < (list.size() > pIconList.size() ? pIconList.size() : list.size()); i++)
    {
        if (list.at(i).isEmpty())
            continue;

        if (!pMappedIconList.contains(list.at(i)))
            pMappedIconList.insert(list.at(i), pIconList.at(i));
    }

    return true;
}

//Maps a list of names to a list of icons many to 1
bool ResourceExtractor::mapToIconList(const QList<QStringList> &list)
{
    if (list.isEmpty())
        return false;

    //Iterate through list
    for (int i = 0; i < (list.size() > pIconList.size() ? pIconList.size() : list.size()); i++)
    {
        //Iterate through all strings
        for (int k = 0; k < list.at(i).size(); k++)
        {
            if (list.at(i).at(k).isEmpty())
                continue;

            //Add a number of maps to each icon
            if (!pMappedIconList.contains(list.at(i).at(k).toLower()))
                pMappedIconList.insert(list.at(i).at(k).toLower(), pIconList.at(i));
        }
    }        

    return true;
}

//Get an icon from the initialized list
QIcon ResourceExtractor::getIconFromIndex(int index)
{
    if (index >= 0 && index < pIconList.size())
        return *pIconList.at(index);
    return QIcon();
}

//Get an icon from a mapped list
QIcon ResourceExtractor::getIconFromName(QString &name)
{
    if (pMappedIconList.contains(name.toLower()))
        return *pMappedIconList.value(name.toLower());

    if (pMappedIconList.contains("unknown"))
        return *pMappedIconList.value("unknown");
    return QIcon();
}

int ResourceExtractor::numIcons()
{
    return pIconList.size();
}