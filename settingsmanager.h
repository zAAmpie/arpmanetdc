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

#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QHash>
#include <QVariant>
#include <QMutex>

//Macro to check if object was initialized
#define ENSURE_INITIALIZED \
    do { \
        if (!initialized) \
            return; \
    } while (0)

//Macro to check if object was initialized that returns a value specified by a
#define ENSURE_INITIALIZED_RET(a) \
    do { \
        if (!initialized) \
            return a; \
    } while (0)

class ArpmanetDC;
struct sqlite3;

class SettingsManager
{
public:
    //Constructor for uninitialized object
    SettingsManager();
    //Constructor for initialized object
    SettingsManager(sqlite3 *db, ArpmanetDC *parent);
    //Destructor - doh
    ~SettingsManager();

    //All types
    enum SettingType
    {
        SettingStringType,
        SettingIntType,
        SettingInt64Type,
        SettingBoolType,
        SettingUninitialized
    };

    struct SettingStruct
    {
        QVariant value;
        QVariant defaultValue;
        QString tag;
        SettingType type;
    };

    //All string type settings
    enum StringTypeSetting
    {
        STRTYPE_FIRST = 0x00,
        NICKNAME = STRTYPE_FIRST,                       //Nickname
        PASSWORD,                                       //Password
        HUB_ADDRESS,                                    //The hub address to connect to chat
        EXTERNAL_IP,                                    //The external IP used for searches
        DOWNLOAD_PATH,                                  //The default download path
        LAST_DOWNLOAD_FOLDER,                           //The last folder downloaded to
        PROTOCOL_HINT,                                  //A string containing a map of the supported protocols in order
        LAST_SEEN_IP,                                   //The last IP the program used
        CONTAINER_DIRECTORY,                            //The directory used to store containers
        FTP_UPDATE_HOST,                                //The FTP host where program updates are stored
        FTP_UPDATE_DIRECTORY,                           //The directory in the FTP host where the application is located
        SHARED_MEMORY_KEY,                              //The key used to identify the shared memory region
        STRTYPE_LAST
    };

    //All integer type settings
    enum IntTypeSetting
    {
        INTTYPE_FIRST = STRTYPE_LAST + 1,
        HUB_PORT = INTTYPE_FIRST,                       //The port used to connect to the hub for chat
        EXTERNAL_PORT,                                  //The external port used for searches
        MAX_SEARCH_RESULTS,                             //The amount of search results contained within a search reply
        MAX_SIMULTANEOUS_DOWNLOADS,                     //The amount of simultaneous downloads
        MAX_SIMULTANEOUS_UPLOADS,                       //The amount of simultaneous uploads
        MAX_LABEL_HISTORY_ENTRIES,                      //The amount of lines kept in history in the status and additional info labels
        MAX_MAINCHAT_BLOCKS,                            //The amount of blocks contained in the chat before older blocks are deleted
        MAX_HASH_SPEED_MB,                              //The maximum speed in megabytes that files should be hashed at
        INTTYPE_LAST
    };

    //All 64-bit integer settings
    enum Int64TypeSetting
    {
        INT64TYPE_FIRST = INTTYPE_LAST + 1,
        AUTO_UPDATE_SHARE_INTERVAL = INT64TYPE_FIRST,   //The interval for checking if shares have changed
        CHECK_FOR_NEW_VERSION_INTERVAL_MS,              //The interval for checking if a client upgrade is available
        TOTAL_UPLOAD,                                   //The total amount in bytes that the client has uploaded
        TOTAL_DOWNLOAD,                                 //The total amount in bytes that the client has downloaded
        INT64TYPE_LAST
    };

    //All boolean settings
    enum BoolTypeSetting
    {
        BOOLTYPE_FIRST = INT64TYPE_LAST + 1,
        SHOW_ADVANCED_MENU = BOOLTYPE_FIRST,            //Should the advanced menu be shown
        SHOW_EMOTICONS,                                 //Should emoticons be used in main chat
        FOCUS_PM_ON_NOTIFY,                             //Should PM widget be automatically focussed when a new message arrives
        BOOLTYPE_LAST
    };

    void setSetting(StringTypeSetting setting, const QString &value);
    void setSetting(IntTypeSetting setting, int value);
    void setSetting(Int64TypeSetting setting, qint64 value);
    void setSetting(BoolTypeSetting setting, bool value);
    void setSetting(int setting, QVariant value); //Generic

    const QString getSetting(StringTypeSetting setting);
    int getSetting(IntTypeSetting setting);
    qint64 getSetting(Int64TypeSetting setting);
    bool getSetting(BoolTypeSetting setting);
    QVariant getSetting(int setting); //Generic

    const QString getTag(StringTypeSetting setting);
    const QString getTag(IntTypeSetting setting);
    const QString getTag(Int64TypeSetting setting);
    const QString getTag(BoolTypeSetting setting);
    const QString getTag(int setting); //Generic

    SettingType getType(StringTypeSetting setting);
    SettingType getType(IntTypeSetting setting);
    SettingType getType(Int64TypeSetting setting);
    SettingType getType(BoolTypeSetting setting);
    SettingType getType(int setting); //Generic

    bool isDefault(int setting); //Generic

    void setToDefault(StringTypeSetting setting);
    void setToDefault(IntTypeSetting setting);
    void setToDefault(Int64TypeSetting setting);
    void setToDefault(BoolTypeSetting setting);
    void setToDefault(int setting); //Generic

    bool loadSettings();
    bool saveSettings();

private:
    //Default set functions
    void setDefault(StringTypeSetting setting, const QString &value, QString settingTag);
    void setDefault(IntTypeSetting setting, int value, QString settingTag);
    void setDefault(Int64TypeSetting setting, qint64 value, QString settingTag);
    void setDefault(BoolTypeSetting setting, bool value, QString settingTag);

    //Get the setting from a tag - WARNING! Slow (takes O(n) time - amort. 50% of linear)
    int getSettingFromTag(const QString &tag);

    //Setting lists
    QHash<int, SettingStruct> pSettings;

    //Initialization status
    bool initialized;

    //Objects
    sqlite3 *pDB;
    ArpmanetDC *pParent;
    static QMutex mutex;
};

#endif