/*
 * iDescriptor: A free and open-source idevice management tool.
 *
 * Copyright (C) 2025 Uncore <https://github.com/uncor3>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "wirelessgalleryimportwidget.h"
#include "photoimportdialog.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>

WirelessGalleryImportWidget::WirelessGalleryImportWidget(QWidget *parent)
    : QWidget(parent), m_leftPanel(nullptr), m_scrollArea(nullptr),
      m_scrollContent(nullptr), m_fileListLayout(nullptr),
      m_browseButton(nullptr), m_importButton(nullptr), m_statusLabel(nullptr),
      m_rightPanel(nullptr), m_tutorialPlayer(nullptr),
      m_tutorialVideoWidget(nullptr), m_loadingIndicator(nullptr),
      m_loadingLabel(nullptr), m_tutorialLayout(nullptr)
{
    setupUI();
    QTimer::singleShot(100, this,
                       &WirelessGalleryImportWidget::setupTutorialVideo);
}

WirelessGalleryImportWidget::~WirelessGalleryImportWidget()
{
    if (m_tutorialPlayer) {
        m_tutorialPlayer->stop();
    }
}

void WirelessGalleryImportWidget::setupUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Left panel - file selection
    m_leftPanel = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);

    // Browse button
    m_browseButton = new QPushButton("Select Files");
    connect(m_browseButton, &QPushButton::clicked, this,
            &WirelessGalleryImportWidget::onBrowseFiles);
    leftLayout->addWidget(m_browseButton);

    // Status label
    m_statusLabel = new QLabel("No files selected");
    m_statusLabel->setWordWrap(true);
    leftLayout->addWidget(m_statusLabel);

    // Scroll area for file list
    m_scrollArea = new QScrollArea();
    m_scrollArea->setStyleSheet("QScrollArea { border: none; }");
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setMinimumWidth(300);

    m_scrollContent = new QWidget();
    m_fileListLayout = new QVBoxLayout(m_scrollContent);
    m_fileListLayout->setContentsMargins(5, 5, 5, 5);
    m_fileListLayout->setSpacing(5);
    m_fileListLayout->addStretch();

    m_scrollArea->setWidget(m_scrollContent);
    leftLayout->addWidget(m_scrollArea, 1);

    // Import button
    m_importButton = new QPushButton("Import Photos to iOS");
    m_importButton->setEnabled(false);
    connect(m_importButton, &QPushButton::clicked, this,
            &WirelessGalleryImportWidget::onImportPhotos);
    leftLayout->addWidget(m_importButton);

    mainLayout->addWidget(m_leftPanel, 1);

    // Right panel - tutorial video
    m_rightPanel = new QWidget();
    m_tutorialLayout = new QVBoxLayout(m_rightPanel);
    m_tutorialLayout->setContentsMargins(0, 0, 0, 0);
    m_tutorialLayout->setSpacing(10);

    // Loading indicator
    m_loadingIndicator = new QProcessIndicator();
    m_loadingIndicator->setType(QProcessIndicator::line_rotate);
    m_loadingIndicator->setFixedSize(64, 32);
    m_loadingIndicator->start();

    QHBoxLayout *loadingLayout = new QHBoxLayout();
    m_loadingLabel = new QLabel("Loading tutorial...");
    m_loadingLabel->setAlignment(Qt::AlignCenter);

    loadingLayout->addWidget(m_loadingLabel);
    loadingLayout->addWidget(m_loadingIndicator);
    loadingLayout->setAlignment(Qt::AlignCenter);

    m_tutorialLayout->addStretch();
    m_tutorialLayout->addLayout(loadingLayout);
    m_tutorialLayout->addStretch();

    mainLayout->addWidget(m_rightPanel, 1);
}

void WirelessGalleryImportWidget::setupTutorialVideo()
{
    m_tutorialPlayer = new QMediaPlayer(this);
    m_tutorialVideoWidget = new QVideoWidget();
    m_tutorialVideoWidget->setSizePolicy(QSizePolicy::Expanding,
                                         QSizePolicy::Expanding);

    m_tutorialPlayer->setVideoOutput(m_tutorialVideoWidget);
    m_tutorialPlayer->setSource(QUrl("qrc:/resources/airplayer-tutorial.mp4"));
    m_tutorialVideoWidget->setAspectRatioMode(
        Qt::AspectRatioMode::KeepAspectRatioByExpanding);

    // Loop the tutorial video
    connect(m_tutorialPlayer, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::EndOfMedia) {
                    m_tutorialPlayer->setPosition(0);
                    m_tutorialPlayer->play();
                }
            });

    // Auto-play when ready and hide loading indicator
    connect(m_tutorialPlayer, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::LoadedMedia) {
                    m_loadingIndicator->stop();
                    m_loadingIndicator->setVisible(false);
                    m_loadingLabel->setVisible(false);
                    m_tutorialPlayer->play();
                }
            });

    // Clear the loading layout and add video widget
    QLayoutItem *child;
    while ((child = m_tutorialLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->setParent(nullptr);
        }
        delete child;
    }

    m_tutorialLayout->addWidget(m_tutorialVideoWidget);
}

void WirelessGalleryImportWidget::onBrowseFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, "Select Photos/Videos to Import",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        "Media Files (*.jpg *.jpeg *.png *.gif *.bmp *.tiff *.tif *.webp "
        "*.heic *.heif *.mp4 *.mov *.avi *.mkv *.m4v *.3gp *.webm);;All Files "
        "(*)");

    if (files.isEmpty()) {
        return;
    }

    // Filter out non-compatible files
    QStringList compatibleFiles;
    for (const QString &file : files) {
        if (isGalleryCompatible(file)) {
            compatibleFiles.append(file);
        }
    }

    m_selectedFiles = compatibleFiles;
    updateFileList();
    updateStatusLabel();
}

void WirelessGalleryImportWidget::updateFileList()
{
    // Clear existing file list
    QLayoutItem *child;
    while ((child = m_fileListLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    // Add files to the list
    for (int i = 0; i < m_selectedFiles.size(); ++i) {
        QFileInfo fileInfo(m_selectedFiles[i]);

        QWidget *fileItem = new QWidget();
        QHBoxLayout *fileLayout = new QHBoxLayout(fileItem);
        fileLayout->setContentsMargins(5, 5, 5, 5);
        fileLayout->setSpacing(0);

        QLabel *fileLabel = new QLabel(fileInfo.fileName());
        fileLabel->setWordWrap(true);

        QPushButton *removeButton = new QPushButton("Remove");
        removeButton->setMaximumWidth(80);

        int index = i;
        connect(removeButton, &QPushButton::clicked, this,
                [this, index]() { onRemoveFile(index); });

        fileLayout->addWidget(fileLabel, 1);
        fileLayout->addWidget(removeButton);

        m_fileListLayout->insertWidget(m_fileListLayout->count() - 1, fileItem);
    }

    m_importButton->setEnabled(!m_selectedFiles.isEmpty());
}

void WirelessGalleryImportWidget::updateStatusLabel()
{
    if (m_selectedFiles.isEmpty()) {
        m_statusLabel->setText("No files selected");
    } else {
        m_statusLabel->setText(
            QString("Selected %1 file(s)").arg(m_selectedFiles.size()));
    }
}

void WirelessGalleryImportWidget::onRemoveFile(int index)
{
    if (index >= 0 && index < m_selectedFiles.size()) {
        m_selectedFiles.removeAt(index);
        updateFileList();
        updateStatusLabel();
    }
}

QStringList WirelessGalleryImportWidget::getGalleryCompatibleExtensions() const
{
    return {"jpg",  "jpeg", "png", "gif", "bmp", "tiff", "tif", "webp", "heic",
            "heif", "mp4",  "mov", "avi", "mkv", "m4v",  "3gp", "webm"};
}

bool WirelessGalleryImportWidget::isGalleryCompatible(
    const QString &filePath) const
{
    QFileInfo info(filePath);
    QString ext = info.suffix().toLower();
    return getGalleryCompatibleExtensions().contains(ext);
}

void WirelessGalleryImportWidget::onImportPhotos()
{
    if (m_selectedFiles.isEmpty()) {
        QMessageBox::warning(this, "No Files",
                             "No gallery-compatible files selected.");
        return;
    }

    PhotoImportDialog dialog(m_selectedFiles, false, this);
    dialog.exec();
}

QStringList WirelessGalleryImportWidget::getSelectedFiles() const
{
    return m_selectedFiles;
}
