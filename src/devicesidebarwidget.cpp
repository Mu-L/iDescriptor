#include "devicesidebarwidget.h"
#include "clickablewidget.h"
#include <QDebug>
#include <QEasingCurve>

// DeviceSidebarItem Implementation
DeviceSidebarItem::DeviceSidebarItem(const QString &deviceName,
                                     const std::string &uuid, QWidget *parent)
    : QFrame(parent), m_deviceName(deviceName), m_uuid(uuid), m_selected(false),
      m_collapsed(true)
{
    setupUI();
    setFrameStyle(QFrame::StyledPanel);
    setLineWidth(1);
    updateToggleButton();

    // Initialize animation
    m_collapseAnimation =
        new QPropertyAnimation(m_optionsWidget, "maximumHeight", this);
    m_collapseAnimation->setDuration(200);
    m_collapseAnimation->setEasingCurve(QEasingCurve::InOutQuad);
}

void DeviceSidebarItem::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->setSpacing(5);

    // Header section (always visible)
    m_headerWidget = new ClickableWidget();
    QVBoxLayout *headerLayout = new QVBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(2);
    m_headerWidget->setStyleSheet("ClickableWidget {  }");
    connect(m_headerWidget, &ClickableWidget::clicked, this,
            [this]() { emit deviceSelected(m_uuid); });

    // Device name label
    m_deviceLabel = new QLabel(m_deviceName);
    m_deviceLabel->setStyleSheet("QLabel { font-weight: bold; color: #333; }");
    m_deviceLabel->setWordWrap(true);
    headerLayout->addWidget(m_deviceLabel);

    // Toggle button
    m_toggleButton = new QPushButton();
    m_toggleButton->setFlat(true);
    m_toggleButton->setMaximumHeight(20);
    m_toggleButton->setStyleSheet("QPushButton { "
                                  "  text-align: left; "
                                  "  padding: 2px 5px; "
                                  "  border: none; "
                                  "  color: #666; "
                                  "  font-size: 11px; "
                                  "} "
                                  "QPushButton:hover { "
                                  "  background-color: #f0f0f0; "
                                  "  border-radius: 3px; "
                                  "}");
    connect(m_toggleButton, &QPushButton::clicked, this,
            &DeviceSidebarItem::onToggleCollapse);
    headerLayout->addWidget(m_toggleButton);

    m_mainLayout->addWidget(m_headerWidget);

    // Options section (collapsible)
    m_optionsWidget = new QWidget();
    QVBoxLayout *optionsLayout = new QVBoxLayout(m_optionsWidget);
    optionsLayout->setContentsMargins(5, 5, 5, 5);
    optionsLayout->setSpacing(3);

    // Create navigation buttons
    m_infoButton = new QPushButton("Info");
    m_appsButton = new QPushButton("Apps");
    m_galleryButton = new QPushButton("Gallery");
    m_filesButton = new QPushButton("Files");

    // Create button group for exclusive selection
    m_navigationGroup = new QButtonGroup(this);
    m_navigationGroup->addButton(m_infoButton, 0);
    m_navigationGroup->addButton(m_appsButton, 1);
    m_navigationGroup->addButton(m_galleryButton, 2);
    m_navigationGroup->addButton(m_filesButton, 3);

    // Style the navigation buttons
    QList<QPushButton *> navButtons = {m_infoButton, m_appsButton,
                                       m_galleryButton, m_filesButton};
    for (QPushButton *btn : navButtons) {
        btn->setCheckable(true);
        btn->setMaximumHeight(25);
        btn->setStyleSheet("QPushButton { "
                           "  background-color: #f8f9fa; "
                           "  border: 1px solid #dee2e6; "
                           "  padding: 4px 8px; "
                           "  text-align: center; "
                           "  border-radius: 3px; "
                           "  font-size: 11px; "
                           "} "
                           "QPushButton:checked { "
                           "  background-color: #0d6efd; "
                           "  color: white; "
                           "  border: 1px solid #0a58ca; "
                           "} "
                           "QPushButton:hover:!checked { "
                           "  background-color: #e9ecef; "
                           "  border-color: #adb5bd; "
                           "} "
                           "QPushButton:checked:hover { "
                           "  background-color: #0b5ed7; "
                           "}");

        connect(btn, &QPushButton::clicked, this,
                &DeviceSidebarItem::onNavigationButtonClicked);
        optionsLayout->addWidget(btn);
    }

    // Set default selection
    m_infoButton->setChecked(true);

    m_mainLayout->addWidget(m_optionsWidget);

    // Initially hide options
    m_optionsWidget->setMaximumHeight(0);
    m_optionsWidget->hide();

    setStyleSheet("DeviceSidebarItem { border: "
                  "1px solid #e0e0e0; border-radius: 5px; }");
}

void DeviceSidebarItem::setSelected(bool selected)
{
    if (m_selected == selected)
        return;

    m_selected = selected;

    if (selected) {
        setStyleSheet("DeviceSidebarItem { border: "
                      "2px solid #2196f3; border-radius: 5px; }");
    } else {
        setStyleSheet("DeviceSidebarItem { border: "
                      "1px solid #e0e0e0; border-radius: 5px; }");
    }
}

void DeviceSidebarItem::setCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed)
        return;

    m_collapsed = collapsed;
    updateToggleButton();
    animateCollapse();
}

void DeviceSidebarItem::updateToggleButton()
{
    if (m_collapsed) {
        m_toggleButton->setText("▶ Options");
    } else {
        m_toggleButton->setText("▼ Options");
    }
}

void DeviceSidebarItem::animateCollapse()
{
    m_collapseAnimation->stop();

    if (m_collapsed) {
        // Collapsing
        m_collapseAnimation->setStartValue(m_optionsWidget->height());
        m_collapseAnimation->setEndValue(0);

        connect(m_collapseAnimation, &QPropertyAnimation::finished, this,
                [this]() {
                    m_optionsWidget->hide();
                    disconnect(m_collapseAnimation,
                               &QPropertyAnimation::finished, this, nullptr);
                });
    } else {
        // Expanding
        m_optionsWidget->show();
        m_optionsWidget->setMaximumHeight(QWIDGETSIZE_MAX);
        int targetHeight = m_optionsWidget->sizeHint().height();
        m_optionsWidget->setMaximumHeight(0);

        m_collapseAnimation->setStartValue(0);
        m_collapseAnimation->setEndValue(targetHeight);

        connect(m_collapseAnimation, &QPropertyAnimation::finished, this,
                [this]() {
                    m_optionsWidget->setMaximumHeight(QWIDGETSIZE_MAX);
                    disconnect(m_collapseAnimation,
                               &QPropertyAnimation::finished, this, nullptr);
                });
    }

    m_collapseAnimation->start();
}

void DeviceSidebarItem::onToggleCollapse() { setCollapsed(!m_collapsed); }

void DeviceSidebarItem::onNavigationButtonClicked()
{
    QPushButton *button = qobject_cast<QPushButton *>(sender());
    if (button) {
        emit navigationRequested(m_uuid, button->text());
        emit deviceSelected(m_uuid);
    }
}

const std::string &DeviceSidebarItem::getDeviceUuid() const { return m_uuid; }

// DeviceSidebarWidget Implementation
DeviceSidebarWidget::DeviceSidebarWidget(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Create scroll area
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setFrameStyle(QFrame::NoFrame);

    // Create content widget
    m_contentWidget = new QWidget();
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(5, 5, 5, 5);
    m_contentLayout->setSpacing(10);
    m_contentLayout->addStretch(); // Push items to top

    m_scrollArea->setWidget(m_contentWidget);
    mainLayout->addWidget(m_scrollArea);

    // Set minimum width
    setMinimumWidth(200);
    setMaximumWidth(250);
}

DeviceSidebarItem *DeviceSidebarWidget::addToSidebar(const QString &deviceName,
                                                     const std::string &uuid)
{
    DeviceSidebarItem *item = new DeviceSidebarItem(deviceName, uuid, this);

    connect(item, &DeviceSidebarItem::deviceSelected, this,
            &DeviceSidebarWidget::onDeviceSelected);
    connect(item, &DeviceSidebarItem::navigationRequested, this,
            &DeviceSidebarWidget::onSidebarNavigationChanged);

    // TODO
    m_currentDeviceUuid = uuid;
    // item->setSelected(true);
    m_deviceSidebarItems.append(item);
    updateSelection();
    // m_deviceItems.append(item);
    m_contentLayout->insertWidget(m_contentLayout->count() - 1,
                                  item); // Insert before stretch

    // Auto-select first device
    // if (m_currentDeviceIndex == -1) {
    //     setCurrentDevice(deviceIndex);
    // }

    return item;
}

void DeviceSidebarWidget::setCurrentDevice(std::string uuid)
{
    if (m_currentDeviceUuid == uuid)
        return;

    m_currentDeviceUuid = uuid;
    updateSelection();
    emit sidebarDeviceChanged(uuid);
}

void DeviceSidebarWidget::setDeviceNavigationSection(int deviceIndex,
                                                     const QString &section)
{
    // for (DeviceSidebarItem *item : m_deviceItems) {
    //     if (item->getDeviceIndex() == deviceIndex) {
    //         // Find and check the appropriate button
    //         QPushButton *targetButton = nullptr;
    //         if (section == "Info")
    //             targetButton = item->findChild<QPushButton *>();
    //         else if (section == "Apps")
    //             targetButton = item->findChildren<QPushButton *>().value(1);
    //         else if (section == "Gallery")
    //             targetButton = item->findChildren<QPushButton *>().value(2);
    //         else if (section == "Files")
    //             targetButton = item->findChildren<QPushButton *>().value(3);

    //         if (targetButton) {
    //             targetButton->setChecked(true);
    //         }
    //         break;
    //     }
    // }
}

void DeviceSidebarWidget::onDeviceSelected(std::string uuid)
{
    setCurrentDevice(uuid);
}

void DeviceSidebarWidget::onSidebarNavigationChanged(std::string uuid,
                                                     const QString &section)
{
    if (uuid != m_currentDeviceUuid) {
        setCurrentDevice(uuid);
    }
    emit sidebarNavigationChanged(uuid, section);
}

void DeviceSidebarWidget::updateSidebar(std::string uuid)
{
    // TODO : need a proper check
    if (m_deviceSidebarItems.isEmpty())
        return;
    m_currentDeviceUuid = uuid;
    updateSelection();
}

void DeviceSidebarWidget::updateSelection()
{
    for (DeviceSidebarItem *item : m_deviceSidebarItems) {
        item->setSelected(item->getDeviceUuid() == m_currentDeviceUuid);
    }
}