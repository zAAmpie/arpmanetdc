#ifndef RESOURCEEXTRACTOR_H
#define RESOURCEEXTRACTOR_H

#include <QObject>
#include <QIcon>
#include <QHash>
#include <QList>
#include <QString>

class ResourceExtractor : public QObject
{
    Q_OBJECT
public:
    ResourceExtractor(QObject *parent = 0);
    ResourceExtractor(const QString &path, quint16 iconSize = 16, QObject *parent = 0);
    ResourceExtractor(const QString &path, QStringList &list, quint16 iconSize = 16, QObject *parent = 0);
    ResourceExtractor(const QString &path, QList<QStringList> &list, quint16 iconSize = 16, QObject *parent = 0);
    ~ResourceExtractor();

    //Initializes a list of icons contained in path
    bool initIconList(const QString &path, quint16 iconSize);
    //Maps a list of names to a list of icons
    bool mapToIconList(const QStringList &list);
    //Maps a series of strings to a list of icons
    bool mapToIconList(const QList<QStringList> &list);

    //Get an icon from the initialized list
    QIcon getIconFromIndex(int index);
    //Get an icon from a mapped list
    QIcon getIconFromName(QString &name);

    int numIcons();
 
private:
    QList<QIcon *> pIconList;
    QHash<QString, QIcon *> pMappedIconList;

    QString pPath;
    quint16 pIconSize;

    bool pMapped;
};

#endif