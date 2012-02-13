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

#ifndef PARSEDIRECTORYTHREAD_H
#define PARSEDIRECTORYTHREAD_H

#include <QObject>
#include <QDir>
#include <QList>
#include "sharesearch.h"

#define RECURSION_LIMIT 100 //Ensure the parsing only reaches 100 levels deep

class ParseDirectoryThread : public QObject
{
    Q_OBJECT
public:
    //Constructor
    ParseDirectoryThread(QObject *parent = 0);

public slots:
    //Called by another thread to parse a directory
    void parseDirectory(QString directoryPath);

    //Stop parsing
    void stopParsing();

signals:
    //Done parsing directory
    void done(QString rootDir, QList<FileListStruct> *fileList, ParseDirectoryThread *parseObj);
    //Failed parsing directory
    void failed(QString rootDir, ParseDirectoryThread *parseObj);

private:
    //Recursive private function
    void parse(QDir directory);

    QList<FileListStruct> *pFileList;
    QDir pDir;

    bool pStopParsing;

    QString pRootDir;

    quint8 recursionLimit;
};

#endif
