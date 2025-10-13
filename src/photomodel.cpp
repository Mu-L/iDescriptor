#include "photomodel.h"
#include "iDescriptor.h"
#include "mediastreamermanager.h"
#include "servicemanager.h"
#include <QDebug>
#include <QEventLoop>
#include <QIcon>
#include <QImage>
#include <QMediaPlayer>
#include <QPixmap>
#include <QRegularExpression>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoSink>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>

PhotoModel::PhotoModel(iDescriptorDevice *device, QObject *parent)
    : QAbstractListModel(parent), m_device(device), m_thumbnailSize(256, 256),
      m_sortOrder(NewestFirst), m_filterType(All)
{
    // Set up cache directory for persistent storage
    m_cacheDir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
        "/photo_thumbs";
    QDir().mkpath(m_cacheDir);

    // Configure memory cache (50MB limit - much more reasonable)
    m_thumbnailCache.setMaxCost(50 * 1024 * 1024);

    connect(this, &PhotoModel::thumbnailNeedsToBeLoaded, this,
            &PhotoModel::requestThumbnail, Qt::QueuedConnection);

    // Don't populate paths in constructor - wait for setAlbumPath
}

PhotoModel::~PhotoModel()
{
    // Clean up any active watchers
    for (auto *watcher : m_activeLoaders.values()) {
        if (watcher) {
            watcher->cancel();
            watcher->waitForFinished();
            watcher->deleteLater();
        }
    }
    m_activeLoaders.clear();
}

QPixmap PhotoModel::generateVideoThumbnail(iDescriptorDevice *device,
                                           const QString &filePath,
                                           const QSize &requestedSize)
{
    QPixmap thumbnail;
    QEventLoop loop;

    // Use a timer to handle potential timeouts
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);

    auto player = std::make_unique<QMediaPlayer>();
    auto sink = std::make_unique<QVideoSink>();
    player->setVideoSink(sink.get());

    // This lambda will be called when a frame is ready
    QObject::connect(sink.get(), &QVideoSink::videoFrameChanged,
                     [&](const QVideoFrame &frame) {
                         if (frame.isValid()) {
                             QImage img = frame.toImage();
                             if (!img.isNull()) {
                                 thumbnail = QPixmap::fromImage(img.scaled(
                                     requestedSize, Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation));
                             }
                         }
                         // We got our frame, so we can stop the loop
                         if (loop.isRunning()) {
                             loop.quit();
                         }
                     });

    // Get the streaming URL and start playback
    QUrl streamUrl = MediaStreamerManager::sharedInstance()->getStreamUrl(
        device, device->afcClient, filePath);
    if (streamUrl.isEmpty()) {
        qWarning() << "Could not get stream URL for video thumbnail:"
                   << filePath;
        return {};
    }

    player->setSource(streamUrl);
    player->setPosition(1000); // Seek 1 second in to get a good frame
    player->play();            // Start playback to trigger frame capture

    // Wait for the videoFrameChanged signal or timeout
    loop.exec();

    // Cleanup
    player->stop();
    MediaStreamerManager::sharedInstance()->releaseStreamer(filePath);

    return thumbnail;
}

int PhotoModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_photos.size();
}

QVariant PhotoModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_photos.size())
        return QVariant();

    const PhotoInfo &info = m_photos.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        return info.fileName;

    case Qt::UserRole:
        return info.filePath;

    case Qt::DecorationRole: {
        qDebug() << "DecorationRole requested for index:" << index.row();

        QString cacheKey = getThumbnailCacheKey(info.filePath);

        // Check memory cache first (works for both images AND videos)
        if (QPixmap *cached = m_thumbnailCache.object(cacheKey)) {
            qDebug() << "Cache HIT for:" << info.fileName;
            return QIcon(*cached);
        }

        // Prevent duplicate requests - this is CRITICAL for both images and
        // videos
        if (m_activeLoaders.contains(cacheKey) ||
            m_loadingPaths.contains(info.filePath)) {
            qDebug() << "Already loading:" << info.fileName;
            // Return appropriate placeholder based on file type
            if (info.fileName.endsWith(".MOV", Qt::CaseInsensitive) ||
                info.fileName.endsWith(".MP4", Qt::CaseInsensitive) ||
                info.fileName.endsWith(".M4V", Qt::CaseInsensitive)) {
                // return QIcon::fromTheme("video-x-generic");
                return QIcon(":/resources/icons/video-x-generic.png");
            } else {
                return QIcon::fromTheme("image-x-generic");
            }
        }

        // Start async loading for both images and videos
        if (!m_loadingPaths.contains(info.filePath)) {
            qDebug() << "Starting load for:" << info.fileName;
            emit const_cast<PhotoModel *>(this)->thumbnailNeedsToBeLoaded(
                index.row());
        }

        // Return placeholder while loading
        if (info.fileName.endsWith(".MOV", Qt::CaseInsensitive) ||
            info.fileName.endsWith(".MP4", Qt::CaseInsensitive) ||
            info.fileName.endsWith(".M4V", Qt::CaseInsensitive)) {
            // return QIcon::fromTheme("video-x-generic");
            return QIcon(":/resources/icons/video-x-generic.png");
        } else {
            return QIcon::fromTheme("image-x-generic");
        }
    }

    case Qt::ToolTipRole:
        return QString("Photo: %1").arg(info.fileName);

    default:
        return QVariant();
    }
}

void PhotoModel::setThumbnailSize(const QSize &size)
{
    if (m_thumbnailSize != size) {
        m_thumbnailSize = size;
        // Clear cache when size changes
        clearCache();
    }
}

void PhotoModel::clearCache()
{
    m_thumbnailCache.clear();

    // Reset all requested flags
    for (PhotoInfo &info : m_photos) {
        info.thumbnailRequested = false;
    }

    // Notify view to refresh
    if (!m_photos.isEmpty()) {
        emit dataChanged(createIndex(0, 0), createIndex(m_photos.size() - 1, 0),
                         {Qt::DecorationRole});
    }
}

QString PhotoModel::getThumbnailCacheKey(const QString &filePath) const
{
    // Create unique key based on file path and thumbnail size
    QString key = QString("%1_%2x%3")
                      .arg(filePath)
                      .arg(m_thumbnailSize.width())
                      .arg(m_thumbnailSize.height());
    return QString(
        QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5)
            .toHex());
}

QString PhotoModel::getThumbnailCachePath(const QString &filePath) const
{
    return m_cacheDir + "/" + getThumbnailCacheKey(filePath) + ".jpg";
}

void PhotoModel::requestThumbnail(int index)
{
    if (index < 0 || index >= m_photos.size())
        return;

    PhotoInfo &info = m_photos[index];
    info.thumbnailRequested = true;

    QString cacheKey = getThumbnailCacheKey(info.filePath);

    if (m_activeLoaders.contains(cacheKey) ||
        m_loadingPaths.contains(info.filePath))
        return;

    m_loadingPaths.insert(info.filePath);

    auto *watcher = new QFutureWatcher<QPixmap>();
    m_activeLoaders[cacheKey] = watcher;

    // Connect the finished signal to handle both images and videos
    connect(watcher, &QFutureWatcher<QPixmap>::finished, this,
            [this, watcher, cacheKey, filePath = info.filePath]() {
                QPixmap thumbnail = watcher->result();

                // Remove from loading sets
                m_loadingPaths.remove(filePath);
                m_activeLoaders.remove(cacheKey);

                if (!thumbnail.isNull()) {
                    // Cache the thumbnail (both memory and disk)
                    int cost = thumbnail.width() * thumbnail.height() * 4;
                    m_thumbnailCache.insert(cacheKey, new QPixmap(thumbnail),
                                            cost);

                    // Find the model index and emit dataChanged
                    for (int i = 0; i < m_photos.size(); ++i) {
                        if (m_photos[i].filePath == filePath) {
                            QModelIndex idx = createIndex(i, 0);
                            emit dataChanged(idx, idx, {Qt::DecorationRole});
                            break;
                        }
                    }
                } else {
                    qDebug() << "Failed to load thumbnail for:"
                             << QFileInfo(filePath).fileName();
                }

                // Clean up the watcher
                watcher->deleteLater();
            });

    // Determine if this is a video or image and load accordingly
    bool isVideo = info.fileName.endsWith(".MOV", Qt::CaseInsensitive) ||
                   info.fileName.endsWith(".MP4", Qt::CaseInsensitive) ||
                   info.fileName.endsWith(".M4V", Qt::CaseInsensitive);

    QString cachePath = getThumbnailCachePath(info.filePath);

    QFuture<QPixmap> future;
    if (isVideo) {
        // Load video thumbnail asynchronously
        // todo: implement
        future = QtConcurrent::run([this]() {
            // Check disk cache first
            // if (QFile::exists(cachePath)) {
            //     QPixmap cached(cachePath);
            //     if (!cached.isNull() && cached.size() == m_thumbnailSize) {
            //         qDebug() << "Video disk cache HIT for:"
            //                  << QFileInfo(info.filePath).fileName();
            //         return cached;
            //     }
            // }

            // // Generate video thumbnail
            // QPixmap thumbnail = generateVideoThumbnail(m_device,
            // info.filePath,
            //                                            m_thumbnailSize);

            // // Save to disk cache if successful
            // if (!thumbnail.isNull()) {
            //     QDir().mkpath(QFileInfo(cachePath).absolutePath());
            //     if (thumbnail.save(cachePath, "JPG", 85)) {
            //         qDebug() << "Saved video thumbnail to disk cache:"
            //                  << QFileInfo(info.filePath).fileName();
            //     }
            // }
            return QPixmap(); // Placeholder until implemented
            // return thumbnail;
        });
    } else {
        // Load image thumbnail asynchronously (existing logic)
        future = QtConcurrent::run([info, cachePath, this]() {
            return loadThumbnailFromDevice(m_device, info.filePath,
                                           m_thumbnailSize, cachePath);
        });
    }

    watcher->setFuture(future);
}

// Static function that runs in worker thread
QPixmap PhotoModel::loadThumbnailFromDevice(iDescriptorDevice *device,
                                            const QString &filePath,
                                            const QSize &size,
                                            const QString &cachePath)
{
    // Check disk cache first
    if (QFile::exists(cachePath)) {
        QPixmap cached(cachePath);
        if (!cached.isNull() && cached.size() == size) {
            qDebug() << "Disk cache HIT for:" << QFileInfo(filePath).fileName();
            return cached;
        }
    }

    // Load from device using ServiceManager
    QByteArray imageData = ServiceManager::safeReadAfcFileToByteArray(
        device, filePath.toUtf8().constData());

    if (imageData.isEmpty()) {
        qDebug() << "Could not read from device:" << filePath;
        return QPixmap(); // Return empty pixmap on error
    }

    if (filePath.endsWith(".HEIC")) {
        qDebug() << "Loading HEIC image from data for:" << filePath;
        QPixmap img = load_heic(imageData);
        return img.isNull() ? QPixmap() : img;
    }

    // Load pixmap from data
    QPixmap original;
    if (!original.loadFromData(imageData)) {
        qDebug() << "Could not decode image data for:" << filePath;
        return QPixmap();
    }

    // Scale to thumbnail size
    QPixmap thumbnail =
        original.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // Save to disk cache
    QDir().mkpath(QFileInfo(cachePath).absolutePath());
    if (thumbnail.save(cachePath, "JPG", 85)) {
        qDebug() << "Saved to disk cache:" << QFileInfo(filePath).fileName();
    }

    return thumbnail;
}

QPixmap PhotoModel::loadImage(iDescriptorDevice *device,
                              const QString &filePath, const QString &cachePath)
{
    // Check disk cache first
    if (QFile::exists(cachePath)) {
        QPixmap cached(cachePath);
        if (!cached.isNull()) {
            qDebug() << "Disk cache HIT for:" << QFileInfo(filePath).fileName();
            return cached;
        }
    }

    // Load from device using ServiceManager
    QByteArray imageData = ServiceManager::safeReadAfcFileToByteArray(
        device, filePath.toUtf8().constData());

    if (imageData.isEmpty()) {
        qDebug() << "Could not read from device:" << filePath;
        return QPixmap(); // Return empty pixmap on error
    }

    if (filePath.endsWith(".HEIC")) {
        qDebug() << "Loading HEIC image from data for:" << filePath;
        QPixmap img = load_heic(imageData);
        return img.isNull() ? QPixmap() : img;
    }

    // TODO
    // Load pixmap from data
    QPixmap original;
    if (!original.loadFromData(imageData)) {
        qDebug() << "Could not decode image data for:" << filePath;
        return QPixmap();
    }

    // Save to disk cache
    QDir().mkpath(QFileInfo(cachePath).absolutePath());
    if (original.save(cachePath, "JPG", 85)) {
        qDebug() << "Saved to disk cache:" << QFileInfo(filePath).fileName();
    }

    return original;
}

void PhotoModel::populatePhotoPaths()
{
    // TODO:beginResetModel called on PhotoModel(0x600002d12a40) without calling
    // endResetModel first
    if (m_albumPath.isEmpty()) {
        qDebug() << "No album path set, skipping population";
        return;
    }

    beginResetModel();
    m_allPhotos.clear();
    m_photos.clear();

    // // Your existing logic to populate photo paths
    // char **files = nullptr;
    // qDebug() << "Populating photos from album path:" << m_albumPath;

    // First verify the album path exists
    QByteArray albumPathBytes = m_albumPath.toUtf8();
    const char *albumPathCStr = albumPathBytes.constData();

    char **albumInfo = nullptr;
    afc_error_t infoResult =
        afc_get_file_info(m_device->afcClient, albumPathCStr, &albumInfo);
    if (infoResult != AFC_E_SUCCESS) {
        qDebug() << "Album path does not exist or cannot be accessed:"
                 << m_albumPath << "Error:" << infoResult;
        endResetModel();
        return;
    }
    if (albumInfo) {
        afc_dictionary_free(albumInfo);
    }

    // Fix: Store the QByteArray to keep the C string valid
    QByteArray photoDirBytes = m_albumPath.toUtf8();
    const char *photoDir = photoDirBytes.constData();
    qDebug() << "Photo directory:" << m_albumPath;
    qDebug() << "Photo directory C string:" << photoDir;

    // Use ServiceManager for thread-safe AFC operations
    char **files = nullptr;
    afc_error_t readResult =
        ServiceManager::safeAfcReadDirectory(m_device, photoDir, &files);
    if (readResult != AFC_E_SUCCESS) {
        qDebug() << "Failed to read photo directory:" << photoDir
                 << "Error:" << readResult;
        endResetModel();
        return;
    }

    if (files) {
        for (int i = 0; files[i]; i++) {
            QString fileName = QString::fromUtf8(files[i]);
            if (fileName.endsWith(".JPG", Qt::CaseInsensitive) ||
                fileName.endsWith(".PNG", Qt::CaseInsensitive) ||
                fileName.endsWith(".HEIC", Qt::CaseInsensitive) ||
                fileName.endsWith(".MOV", Qt::CaseInsensitive) ||
                fileName.endsWith(".MP4", Qt::CaseInsensitive) ||
                fileName.endsWith(".M4V", Qt::CaseInsensitive)) {

                PhotoInfo info;
                info.filePath = m_albumPath + "/" + fileName;
                info.fileName = fileName;
                info.thumbnailRequested = false;
                info.fileType = determineFileType(fileName);
                info.dateTime = extractDateTimeFromFile(info.filePath);

                m_allPhotos.append(info);
            }
        }
        afc_dictionary_free(files);
    }

    // Apply initial filtering and sorting
    applyFilterAndSort();

    endResetModel();

    qDebug() << "Loaded" << m_allPhotos.size() << "media files from device";
    qDebug() << "After filtering:" << m_photos.size() << "items shown";
}

// Sorting and filtering methods
void PhotoModel::setSortOrder(SortOrder order)
{
    if (m_sortOrder != order) {
        m_sortOrder = order;
        applyFilterAndSort();
    }
}

void PhotoModel::setFilterType(FilterType filter)
{
    if (m_filterType != filter) {
        m_filterType = filter;
        applyFilterAndSort();
    }
}

void PhotoModel::applyFilterAndSort()
{
    beginResetModel();

    // Filter photos
    m_photos.clear();
    for (const PhotoInfo &info : m_allPhotos) {
        if (matchesFilter(info)) {
            m_photos.append(info);
        }
    }

    // Sort photos
    sortPhotos(m_photos);

    endResetModel();

    qDebug() << "Applied filter and sort - showing" << m_photos.size() << "of"
             << m_allPhotos.size() << "items";
}

void PhotoModel::sortPhotos(QList<PhotoInfo> &photos) const
{
    std::sort(photos.begin(), photos.end(),
              [this](const PhotoInfo &a, const PhotoInfo &b) {
                  if (m_sortOrder == NewestFirst) {
                      return a.dateTime > b.dateTime;
                  } else {
                      return a.dateTime < b.dateTime;
                  }
              });
}

bool PhotoModel::matchesFilter(const PhotoInfo &info) const
{
    switch (m_filterType) {
    case All:
        return true;
    case ImagesOnly:
        return info.fileType == PhotoInfo::Image;
    case VideosOnly:
        return info.fileType == PhotoInfo::Video;
    default:
        return true;
    }
}

// Export functionality
QStringList
PhotoModel::getSelectedFilePaths(const QModelIndexList &indexes) const
{
    QStringList paths;
    for (const QModelIndex &index : indexes) {
        if (index.isValid() && index.row() < m_photos.size()) {
            paths.append(m_photos.at(index.row()).filePath);
        }
    }
    return paths;
}

QString PhotoModel::getFilePath(const QModelIndex &index) const
{
    if (index.isValid() && index.row() < m_photos.size()) {
        return m_photos.at(index.row()).filePath;
    }
    return QString();
}

PhotoInfo::FileType PhotoModel::getFileType(const QModelIndex &index) const
{
    if (index.isValid() && index.row() < m_photos.size()) {
        return m_photos.at(index.row()).fileType;
    }
    return PhotoInfo::Image;
}

QStringList PhotoModel::getAllFilePaths() const
{
    QStringList paths;
    for (const PhotoInfo &info : m_allPhotos) {
        paths.append(info.filePath);
    }
    return paths;
}

QStringList PhotoModel::getFilteredFilePaths() const
{
    QStringList paths;
    for (const PhotoInfo &info : m_photos) {
        paths.append(info.filePath);
    }
    return paths;
}

// Helper methods
QDateTime PhotoModel::extractDateTimeFromFile(const QString &filePath) const
{
    // Use AFC to get actual file creation time from device
    plist_t info = nullptr;
    afc_error_t afc_err = afc_get_file_info_plist(
        // TODO:AFC CLIENT IS NOT LONG LIVED
        m_device->afcClient, filePath.toUtf8().constData(), &info);

    if (afc_err == AFC_E_SUCCESS && info) {
        // Try to get st_birthtime (creation time) first
        plist_t birthtime_node = plist_dict_get_item(info, "st_birthtime");
        if (birthtime_node &&
            plist_get_node_type(birthtime_node) == PLIST_UINT) {
            uint64_t birthtime_ns = 0;
            plist_get_uint_val(birthtime_node, &birthtime_ns);

            // Convert nanoseconds since epoch to QDateTime
            // The timestamp appears to be in nanoseconds since Unix epoch
            uint64_t seconds = birthtime_ns / 1000000000ULL;
            QDateTime dateTime =
                QDateTime::fromSecsSinceEpoch(seconds, Qt::UTC);

            plist_free(info);
            if (dateTime.isValid()) {
                return dateTime;
            }
        }

        // Fallback to st_mtime (modification time) if birthtime not available
        plist_t mtime_node = plist_dict_get_item(info, "st_mtime");
        if (mtime_node && plist_get_node_type(mtime_node) == PLIST_UINT) {
            uint64_t mtime_ns = 0;
            plist_get_uint_val(mtime_node, &mtime_ns);

            // Convert nanoseconds since epoch to QDateTime
            uint64_t seconds = mtime_ns / 1000000000ULL;
            QDateTime dateTime =
                QDateTime::fromSecsSinceEpoch(seconds, Qt::UTC);

            plist_free(info);
            if (dateTime.isValid()) {
                return dateTime;
            }
        }

        plist_free(info);
    }

    // Final fallback: try to extract date from filename pattern like
    // IMG_20231025_143052.jpg
    QFileInfo fileInfo(filePath);
    QString baseName = fileInfo.baseName();

    QRegularExpression dateRegex(
        R"((\d{4})(\d{2})(\d{2})_(\d{2})(\d{2})(\d{2}))");
    QRegularExpressionMatch match = dateRegex.match(baseName);

    if (match.hasMatch()) {
        int year = match.captured(1).toInt();
        int month = match.captured(2).toInt();
        int day = match.captured(3).toInt();
        int hour = match.captured(4).toInt();
        int minute = match.captured(5).toInt();
        int second = match.captured(6).toInt();

        QDateTime dateTime(QDate(year, month, day),
                           QTime(hour, minute, second));
        if (dateTime.isValid()) {
            return dateTime;
        }
    }

    // Ultimate fallback: return current time
    return QDateTime::currentDateTime();
}

PhotoInfo::FileType PhotoModel::determineFileType(const QString &fileName) const
{
    if (fileName.endsWith(".MOV", Qt::CaseInsensitive) ||
        fileName.endsWith(".MP4", Qt::CaseInsensitive) ||
        fileName.endsWith(".M4V", Qt::CaseInsensitive)) {
        return PhotoInfo::Video;
    } else {
        return PhotoInfo::Image;
    }
}

void PhotoModel::setAlbumPath(const QString &albumPath)
{
    if (m_albumPath != albumPath) {
        m_albumPath = albumPath;
        populatePhotoPaths();
    }
}

void PhotoModel::refreshPhotos() { populatePhotoPaths(); }