#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QSettings>

class SettingsManager : public QObject
{
    Q_OBJECT
public:
    explicit SettingsManager(QObject *parent = nullptr);
    static SettingsManager *sharedInstance();

signals:

public slots:
    QString devdiskimgpath() const;

private:
    QSettings *m_settings;
};

#endif // SETTINGSMANAGER_H
