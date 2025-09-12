#ifndef DEVICESIDEBARWIDGET_H
#define DEVICESIDEBARWIDGET_H

#include "clickablewidget.h"
#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

class DeviceSidebarItem : public QFrame
{
    Q_OBJECT

public:
    explicit DeviceSidebarItem(const QString &deviceName,
                               const std::string &uuid,
                               QWidget *parent = nullptr);
    const std::string &getDeviceUuid() const;

    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }
    void setCollapsed(bool collapsed);
    bool isCollapsed() const { return m_collapsed; }

signals:
    void deviceSelected(const std::string &uuid);
    void navigationRequested(const std::string &uuid, const QString &section);

private slots:
    void onToggleCollapse();
    void onNavigationButtonClicked();

private:
    void setupUI();
    void updateToggleButton();
    void animateCollapse();

    std::string m_uuid;
    QString m_deviceName;
    bool m_selected;
    bool m_collapsed;
    QVBoxLayout *m_mainLayout;
    ClickableWidget *m_headerWidget;
    QWidget *m_optionsWidget;
    QPushButton *m_toggleButton;
    QLabel *m_deviceLabel;

    // Navigation buttons
    QPushButton *m_infoButton;
    QPushButton *m_appsButton;
    QPushButton *m_galleryButton;
    QPushButton *m_filesButton;
    QButtonGroup *m_navigationGroup;

    QPropertyAnimation *m_collapseAnimation;
};

class DeviceSidebarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceSidebarWidget(QWidget *parent = nullptr);
    std::string getUuid() const;

    DeviceSidebarItem *addToSidebar(const QString &deviceName,
                                    const std::string &uuid);

    void setDeviceNavigationSection(int deviceIndex, const QString &section);
    void updateSidebar(std::string uuid);

public slots:
    void onSidebarNavigationChanged(std::string uuid, const QString &section);

signals:
    void sidebarNavigationChanged(std::string uuid, const QString &section);
    void deviceNavigationChanged(std::string uuid, const QString &section);
    void sidebarDeviceChanged(std::string uuid);

private:
    void updateSelection();
    void onDeviceSelected(std::string uuid);
    void setCurrentDevice(std::string uuid);
    QScrollArea *m_scrollArea;
    QWidget *m_contentWidget;
    QVBoxLayout *m_contentLayout;

    std::string m_currentDeviceUuid;
    QList<DeviceSidebarItem *> m_deviceSidebarItems;
};

#endif // DEVICESIDEBARWIDGET_H