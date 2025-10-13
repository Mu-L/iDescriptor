#include "deviceimagewidget.h"
#include <QDateTime>
#include <QDebug>
#include <QMap>
#include <QPainter>
#include <QVBoxLayout>
#include <libimobiledevice/libimobiledevice.h>

DeviceImageWidget::DeviceImageWidget(iDescriptorDevice *device, QWidget *parent)
    : QWidget(parent), m_device(device)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_imageLabel = new ResponsiveQLabel(this);
    m_imageLabel->setMinimumWidth(200);
    m_imageLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_imageLabel->setStyleSheet("background: transparent; border: none;");

    layout->addWidget(m_imageLabel);

    setupDeviceImage();
    m_timeUpdateTimer = new QTimer(this);
    connect(m_timeUpdateTimer, &QTimer::timeout, this,
            &DeviceImageWidget::updateTime);
    m_timeUpdateTimer->start(60000); // Update every minute

    updateTime();
}

DeviceImageWidget::~DeviceImageWidget()
{
    if (m_timeUpdateTimer) {
        m_timeUpdateTimer->stop();
    }
}

void DeviceImageWidget::setupDeviceImage()
{
    m_mockupPath = getDeviceMockupPath();
    m_wallpaperPath = getWallpaperPath();

    qDebug() << "Using mockup:" << m_mockupPath;
    qDebug() << "Using wallpaper:" << m_wallpaperPath;
}

QString DeviceImageWidget::getDeviceMockupPath() const
{
    QString displayName =
        QString::fromStdString(m_device->deviceInfo.productType);
    QString mockupName = getMockupNameFromDisplayName(displayName);

    return QString(":/resources/iphone-mockups/iphone-%1.png").arg(mockupName);
}

QString DeviceImageWidget::getWallpaperPath() const
{
    int iosVersion = getIosVersionFromDevice();

    // Map iOS version to available wallpapers
    QString wallpaperVersion;
    if (iosVersion >= 18) {
        wallpaperVersion = "ios18";
    } else if (iosVersion >= 17) {
        wallpaperVersion = "ios17";
    } else if (iosVersion >= 16) {
        wallpaperVersion = "ios16";
    } else if (iosVersion >= 15) {
        wallpaperVersion = "ios15";
    } else if (iosVersion >= 14) {
        wallpaperVersion = "ios14";
    } else if (iosVersion >= 13) {
        wallpaperVersion = "ios13";
    } else if (iosVersion >= 12) {
        wallpaperVersion = "ios12";
    } else if (iosVersion >= 11) {
        wallpaperVersion = "ios11";
    } else if (iosVersion >= 10) {
        wallpaperVersion = "ios10";
    } else if (iosVersion >= 9) {
        wallpaperVersion = "ios9";
    } else if (iosVersion >= 8) {
        wallpaperVersion = "ios8";
    } else if (iosVersion >= 7) {
        wallpaperVersion = "ios7";
    } else if (iosVersion >= 6) {
        wallpaperVersion = "ios6";
    } else if (iosVersion >= 5) {
        wallpaperVersion = "ios5";
    } else if (iosVersion >= 4) {
        wallpaperVersion = "ios4";
    } else {
        // Unknown version, use ios26 as fallback
        wallpaperVersion = "ios26";
    }

    return QString(":/resources/ios-wallpapers/iphone-%1.png")
        .arg(wallpaperVersion);
}

QString DeviceImageWidget::getMockupNameFromDisplayName(
    const QString &displayName) const
{
    // Map device names to mockup files
    if (displayName.contains("iPhone 16", Qt::CaseInsensitive)) {
        return "16";
    } else if (displayName.contains("iPhone 15", Qt::CaseInsensitive)) {
        return "15";
    } else if (displayName.contains("iPhone X", Qt::CaseInsensitive) ||
               displayName.contains("iPhone 11", Qt::CaseInsensitive) ||
               displayName.contains("iPhone 12", Qt::CaseInsensitive) ||
               displayName.contains("iPhone 13", Qt::CaseInsensitive) ||
               displayName.contains("iPhone 14", Qt::CaseInsensitive)) {
        return "x";
    } else if (displayName.contains("iPhone 6", Qt::CaseInsensitive) ||
               displayName.contains("iPhone 7", Qt::CaseInsensitive) ||
               displayName.contains("iPhone 8", Qt::CaseInsensitive)) {
        return "6";
    } else if (displayName.contains("iPhone 5", Qt::CaseInsensitive) ||
               displayName.contains("iPhone SE", Qt::CaseInsensitive)) {
        return "5";
    } else if (displayName.contains("iPhone 4", Qt::CaseInsensitive)) {
        return "4";
    } else if (displayName.contains("iPhone 3", Qt::CaseInsensitive)) {
        return "3";
    } else {
        // Unknown device, use iPhone X as default
        return "x";
    }
}

int DeviceImageWidget::getIosVersionFromDevice() const
{
    unsigned int version = idevice_get_device_version(m_device->device);

    if (version > 0) {
        int majorVersion = (version >> 16) & 0xFF;
        return majorVersion;
    }

    // Fallback: parse from productVersion string
    QString versionString =
        QString::fromStdString(m_device->deviceInfo.productVersion);
    QStringList parts = versionString.split('.');
    if (!parts.isEmpty()) {
        bool ok;
        int majorVersion = parts.first().toInt(&ok);
        if (ok) {
            return majorVersion;
        }
    }

    // If all else fails, return unknown version (will use ios26 wallpaper)
    return 0;
}

/*
    this method is only here to calculate the screen area
    so that wallpaper perfectly fits to the screen size
    it's costy so if you want to add a new mock run
    through this method qDebug the result and add it to createCompositeImage
    example :     screenRect = QRect(152, 79, 195, 296);
*/
QRect DeviceImageWidget::findScreenArea(const QPixmap &mockup) const
{
    QImage image = mockup.toImage().convertToFormat(QImage::Format_ARGB32);
    if (image.isNull()) {
        return QRect();
    }

    int width = image.width();
    int height = image.height();
    int centerX = width / 2;
    int centerY = height / 2;

    if (qAlpha(image.pixel(centerX, centerY)) != 0) {
        qWarning() << "Cannot find screen area: center pixel is not "
                      "transparent. Falling back to default.";
        return QRect(width * 0.1, height * 0.1, width * 0.8, height * 0.8);
    }

    int left = centerX;
    int right = centerX;
    int top = centerY;
    int bottom = centerY;

    // Scan left from center
    while (left > 0 && qAlpha(image.pixel(left, centerY)) == 0) {
        left--;
    }

    // Scan right from center
    while (right < width - 1 && qAlpha(image.pixel(right, centerY)) == 0) {
        right++;
    }

    // Scan up from center
    while (top > 0 && qAlpha(image.pixel(centerX, top)) == 0) {
        top--;
    }

    // Scan down from center
    while (bottom < height - 1 && qAlpha(image.pixel(centerX, bottom)) == 0) {
        bottom++;
    }

    // Add a small margin to avoid drawing over the bezel anti-aliasing
    int margin = 2;

    return QRect(left + 1 + margin, top + 1 + margin,
                 right - left - 2 - (margin * 2),
                 bottom - top - 2 - (margin * 2));
}

QPixmap DeviceImageWidget::createCompositeImage() const
{
    QPixmap mockup(m_mockupPath);
    QPixmap wallpaper(m_wallpaperPath);

    if (mockup.isNull()) {
        qWarning() << "Failed to load mockup:" << m_mockupPath;
        return QPixmap(":/resources/iphone.png"); // Fallback
    }

    if (wallpaper.isNull()) {
        qWarning() << "Failed to load wallpaper:" << m_wallpaperPath;
        return mockup; // Return just the mockup
    }

    // Start with the mockup as the base layer
    QPixmap composite = mockup.copy();
    QPainter painter(&composite);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Use pre-calculated screen areas for optimal performance
    QRect screenRect;
    QString mockupName = getMockupNameFromDisplayName(
        QString::fromStdString(m_device->deviceInfo.productType));

    if (mockupName == "3") {
        screenRect = QRect(152, 79, 195, 296);
    } else if (mockupName == "4") {
        screenRect = QRect(421, 188, 366, 534);
    } else if (mockupName == "5") {
        screenRect = QRect(34, 113, 290, 523);
    } else if (mockupName == "6") {
        screenRect = QRect(75, 355, 1265, 2256);
    } else if (mockupName == "x") {
        screenRect = QRect(252, 436, 2375, 4989);
    } else if (mockupName == "15") {
        screenRect = QRect(22, 56, 323, 674);
    } else if (mockupName == "16") {
        screenRect = QRect(24, 61, 319, 668);
    } else {
        // Fallback for unknown devices
        screenRect = QRect(mockup.width() * 0.12, mockup.height() * 0.08,
                           mockup.width() * 0.76, mockup.height() * 0.84);
    }

    // Draw wallpaper BEHIND the mockup (into the screen area)
    QPixmap scaledWallpaper = wallpaper.scaled(
        screenRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    painter.drawPixmap(screenRect, scaledWallpaper);

    // Draw current time in the center of the screen
    QString currentTime = QDateTime::currentDateTime().toString("hh:mm");

    // Setup text rendering with better font sizing
    QFont timeFont;
    timeFont.setFamily("SF Pro Display, Helvetica, Arial");

    // Scale font size based on screen dimensions
    int fontSize = screenRect.width() / 5;
    timeFont.setPointSize(fontSize);
    timeFont.setWeight(QFont::Light);

    painter.setFont(timeFont);

    // Draw text shadow for better readability
    painter.setPen(QColor(0, 0, 0, 150));
    painter.drawText(screenRect.adjusted(2, 2, 2, 2), Qt::AlignCenter,
                     currentTime);

    // Draw main text - perfectly centered in the screen area
    painter.setPen(QColor(255, 255, 255, 255));
    painter.drawText(screenRect, Qt::AlignCenter, currentTime);

    painter.end();
    return composite;
}

void DeviceImageWidget::updateTime()
{
    QPixmap composite = createCompositeImage();
    m_imageLabel->setPixmap(composite);
}