#include "settingsmanager.h"
#include <QSettings>

#define DEFAULT_DEVDISKIMGPATH "./devdiskimages"

SettingsManager *SettingsManager::sharedInstance()
{
    static SettingsManager instance;
    return &instance;
}

SettingsManager::SettingsManager(QObject *parent) : QObject{parent}
{

    m_settings = new QSettings(this);
}

QString SettingsManager::devdiskimgpath() const
{
    return m_settings->value("devdiskimgpath", DEFAULT_DEVDISKIMGPATH)
        .toString();
}