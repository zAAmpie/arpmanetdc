#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QHash>
#include <QVariant>

#define ENSURE_INITIALIZED() {if (!initialized) return;}

class ArpmanetDC;
struct sqlite3;

class SettingsManager
{
public:
    SettingsManager();
    SettingsManager(sqlite3 *db, ArpmanetDC *parent);
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
        NICKNAME = STRTYPE_FIRST,
        PASSWORD,
        HUB_ADDRESS,
        EXTERNAL_IP,
        DOWNLOAD_PATH,
        LAST_DOWNLOAD_FOLDER,
        PROTOCOL_HINT,
        LAST_SEEN_IP,
        CONTAINER_DIRECTORY,
        FTP_UPDATE_HOST,
        FTP_UPDATE_DIRECTORY,
        SHARED_MEMORY_KEY,
        STRTYPE_LAST
    };

    //All integer type settings
    enum IntTypeSetting
    {
        INTTYPE_FIRST = STRTYPE_LAST + 1,
        HUB_PORT,
        EXTERNAL_PORT,
        MAX_SEARCH_RESULTS,
        MAX_SIMULTANEOUS_DOWNLOADS,
        MAX_SIMULTANEOUS_UPLOADS,
        MAX_LABEL_HISTORY_ENTRIES,
        INTTYPE_LAST
    };

    //All 64-bit integer settings
    enum Int64TypeSetting
    {
        INT64TYPE_FIRST = INTTYPE_LAST + 1,
        AUTO_UPDATE_SHARE_INTERVAL,
        CHECK_FOR_NEW_VERSION_INTERVAL_MS,
        MAX_MAINCHAT_BLOCKS,
        INT64TYPE_LAST
    };

    //All boolean settings
    enum BoolTypeSetting
    {
        BOOLTYPE_FIRST = INT64TYPE_LAST + 1,
        SHOW_ADVANCED_MENU,
        SHOW_EMOTICONS,
        FOCUS_PM_ON_NOTIFY,
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
};

#endif