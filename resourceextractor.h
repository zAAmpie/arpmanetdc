/* This file is part of ArpmanetDC. Copyright (C) 2012
 * Source code can be found at http://code.google.com/p/arpmanetdc/
 * 
 * ArpmanetDC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ArpmanetDC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with ArpmanetDC.  If not, see <http://www.gnu.org/licenses/>.
 */

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
    //Get pixmap from a mapped list
    QPixmap getPixmapFromName(QString &name);

    int numIcons();
 
private:
    QList<QPixmap> pIconList;
    QHash<QString, QPixmap> pMappedIconList;

    QString pPath;
    quint16 pIconSize;

    bool pMapped;
};

#endif