#ifndef TOOLBOXWIDGET_H
#define TOOLBOXWIDGET_H

#include "devdiskimageswidget.h"
#include "iDescriptor-ui.h"
#include "iDescriptor.h"
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

class ToolboxWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ToolboxWidget(QWidget *parent = nullptr);
    static void restartDevice(iDescriptorDevice *device);
    static void shutdownDevice(iDescriptorDevice *device);
    static void _enterRecoveryMode(iDescriptorDevice *device);
private slots:
    void onDeviceSelectionChanged();
    void onToolboxClicked(iDescriptorTool tool);

private:
    void setupUI();
    void updateDeviceList();
    void updateToolboxStates();
    void updateUI();
    ClickableWidget *createToolbox(iDescriptorTool tool,
                                   const QString &description,
                                   bool requiresDevice);
    QComboBox *m_deviceCombo;
    QLabel *m_deviceLabel;
    QScrollArea *m_scrollArea;
    QWidget *m_contentWidget;
    QGridLayout *m_gridLayout;
    QList<QWidget *> m_toolboxes;
    QList<bool> m_requiresDevice;
    iDescriptorDevice *m_currentDevice;
    std::string m_uuid;
    DevDiskImagesWidget *m_devDiskImagesWidget;

signals:
};

#endif // TOOLBOXWIDGET_H
