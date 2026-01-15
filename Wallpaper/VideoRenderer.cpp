#include "VideoRenderer.h"
#include <QDebug>
#include <QFileInfo>
#include <QUrl>
#include <QAudioOutput>
#include <QMediaMetaData>
#include <QApplication>
#include <QGuiApplication>
#include <QPainter>
#include <QTime>
#include <QMetaObject>
#include <QProcess>
#include <QDir>
#include <QProgressBar>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QThread>
#include <QMessageBox>
#include <QScreen>

VideoRenderer::VideoRenderer(QScreen* targetScreen, QWidget* parent)
    : MediaRenderer(parent)
    , m_targetScreen(targetScreen)
    , m_parentWidget(parent)
    , m_mediaPlayer(nullptr)
    , m_videoWidget(nullptr)
    , m_wakeupTimer(nullptr)
    , m_statusCheckTimer(nullptr)
    , m_optimizationTimer(nullptr)
    , m_geometryUpdateTimer(nullptr)
    , m_delayedInitTimer(nullptr)
    , m_ffmpegProcess(nullptr)
    , m_progressBar(nullptr)
    , m_videoDuration(0)
    , m_wasSleeping(false)
    , m_videoOptimized(false)
    , m_videoInitialized(false)
    , m_videoErrorCount(0)
{
    // Устанавливаем начальный размер на основе экрана
    if (m_targetScreen) {
        m_currentSize = m_targetScreen->size();
        qDebug() << "Initial video size from screen:" << m_currentSize;
    }

    // Конфигурируем родительский виджет если он существует
    if (m_parentWidget) {
        m_parentWidget->setAttribute(Qt::WA_OpaquePaintEvent, true);
        m_parentWidget->setAttribute(Qt::WA_NoSystemBackground, true);
        m_parentWidget->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        m_parentWidget->setStyleSheet("background: transparent;");

        // Создаем прогресс бар
        m_progressBar = new QProgressBar(m_parentWidget);
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(0);
        m_progressBar->setTextVisible(true);
        m_progressBar->setFormat("Converting video: %p%");
        m_progressBar->setStyleSheet("QProgressBar { background-color: #333; color: white; border: 1px solid grey; }");
        m_progressBar->setFixedSize(300, 30);
        m_progressBar->hide();
    }
}

VideoRenderer::~VideoRenderer()
{
    // 1. Остановка таймеров
    if (m_statusCheckTimer) {
        m_statusCheckTimer->stop();
        delete m_statusCheckTimer;
        m_statusCheckTimer = nullptr;
    }
    if (m_wakeupTimer) {
        m_wakeupTimer->stop();
        delete m_wakeupTimer;
        m_wakeupTimer = nullptr;
    }
    if (m_optimizationTimer) {
        m_optimizationTimer->stop();
        delete m_optimizationTimer;
        m_optimizationTimer = nullptr;
    }
    if (m_geometryUpdateTimer) {
        m_geometryUpdateTimer->stop();
        delete m_geometryUpdateTimer;
        m_geometryUpdateTimer = nullptr;
    }
    if (m_delayedInitTimer) {
        m_delayedInitTimer->stop();
        delete m_delayedInitTimer;
        m_delayedInitTimer = nullptr;
    }

    // 2. Принудительная остановка FFmpeg
    if (m_ffmpegProcess) {
        if (m_ffmpegProcess->state() != QProcess::NotRunning) {
            qDebug() << "Killing FFmpeg process before destruction...";
            m_ffmpegProcess->kill();
            m_ffmpegProcess->waitForFinished(1000);
        }
        delete m_ffmpegProcess;
        m_ffmpegProcess = nullptr;
    }

    // 3. Удаление UI
    if (m_progressBar) {
        m_progressBar->hide();
        delete m_progressBar;
        m_progressBar = nullptr;
    }

    // 4. Очистка медиа ресурсов
    if (m_mediaPlayer) {
        m_mediaPlayer->stop();
        delete m_mediaPlayer;
        m_mediaPlayer = nullptr;
    }
    if (m_videoWidget) {
        m_videoWidget->hide();
        delete m_videoWidget;
        m_videoWidget = nullptr;
    }
}

void VideoRenderer::initialize()
{
    qDebug() << "VideoRenderer::initialize for screen:"
             << (m_targetScreen ? m_targetScreen->name() : "unknown");

    // Таймер для отложенной инициализации
    m_delayedInitTimer = new QTimer(this);
    m_delayedInitTimer->setSingleShot(true);
    m_delayedInitTimer->setInterval(50);
    connect(m_delayedInitTimer, &QTimer::timeout, this, [this]() {
        if (!m_videoInitialized) {
            setupVideoPlayer();
        }
    });
    m_delayedInitTimer->start();

    // Таймер пробуждения системы
    m_wakeupTimer = new QTimer(this);
    m_wakeupTimer->setInterval(DesktopBackgroundSettings::WAKEUP_CHECK_INTERVAL_MS);
    connect(m_wakeupTimer, &QTimer::timeout, this, &VideoRenderer::handleSystemWake);
    m_wakeupTimer->start();

    // Таймер проверки статуса
    m_statusCheckTimer = new QTimer(this);
    m_statusCheckTimer->setInterval(5000);
    connect(m_statusCheckTimer, &QTimer::timeout, this, &VideoRenderer::checkStatus);

    // Таймер оптимизации
    m_optimizationTimer = new QTimer(this);
    m_optimizationTimer->setSingleShot(true);
    m_optimizationTimer->setInterval(DesktopBackgroundSettings::POST_INITIALIZATION_DELAY_MS);
    connect(m_optimizationTimer, &QTimer::timeout, this, &VideoRenderer::optimizeVideoPlayback);

    // Таймер обновления геометрии
    m_geometryUpdateTimer = new QTimer(this);
    m_geometryUpdateTimer->setSingleShot(true);
    m_geometryUpdateTimer->setInterval(16);
    connect(m_geometryUpdateTimer, &QTimer::timeout, this, &VideoRenderer::updateGeometry);
}

void VideoRenderer::show()
{
    qDebug() << "VideoRenderer::show() called";

    if (m_videoWidget) {
        m_videoWidget->show();
        m_videoWidget->raise();

        if (m_parentWidget) {
            m_parentWidget->show();
            m_parentWidget->raise();
        }

        ensurePlaying();

        updateGeometry();

        if (m_statusCheckTimer) {
            m_statusCheckTimer->start();
        }
        qDebug() << "Video widget and parent shown";
    } else if (m_delayedInitTimer && !m_delayedInitTimer->isActive()) {
        qDebug() << "No video widget, starting delayed init";
        m_delayedInitTimer->start();
    }

    // Позиционируем прогресс бар в центре, если он виден
    if (m_progressBar && m_progressBar->isVisible()) {
        m_progressBar->move((m_currentSize.width() - m_progressBar->width()) / 2,
                            (m_currentSize.height() - m_progressBar->height()) / 2);
    }
}

void VideoRenderer::hide()
{
    qDebug() << "VideoRenderer::hide() called";

    if (m_statusCheckTimer) {
        m_statusCheckTimer->stop();
    }

    if (m_mediaPlayer) {
        m_mediaPlayer->pause();
    }

    if (m_videoWidget) {
        m_videoWidget->hide();
    }

    if (m_parentWidget && !m_parentWidget->parentWidget()) {
        m_parentWidget->hide();
    }

    if (m_progressBar) {
        m_progressBar->hide();
    }
}

void VideoRenderer::resize(const QSize& size)
{
    m_currentSize = size;
    if (m_geometryUpdateTimer) {
        m_geometryUpdateTimer->start();
    }

    // Обновляем позицию прогресс бара при ресайзе
    if (m_progressBar && m_progressBar->isVisible()) {
        m_progressBar->move((size.width() - m_progressBar->width()) / 2,
                            (size.height() - m_progressBar->height()) / 2);
    }
}

void VideoRenderer::paint(QPainter* painter, const QRect& rect)
{
    painter->fillRect(rect, Qt::black);
}

void VideoRenderer::setSettings(const ScreenSettings& settings)
{
    QString decodedPath = QUrl::fromPercentEncoding(settings.backgroundImage.toUtf8());

    // Проверяем, изменился ли путь к видео
    bool pathChanged = (m_currentVideoPath != decodedPath && m_convertedVideoPath != decodedPath);

    if (pathChanged) {
        qDebug() << "Video path changed from" << m_currentVideoPath << "to" << decodedPath;

        // Если идет конвертация предыдущего файла — убиваем её
        if (m_ffmpegProcess && m_ffmpegProcess->state() != QProcess::NotRunning) {
            qDebug() << "Aborting active conversion for previous video.";
            m_ffmpegProcess->kill();
            m_ffmpegProcess->waitForFinished();
        }

        // Скрываем прогресс бар старого процесса
        if (m_progressBar) {
            m_progressBar->hide();
            m_progressBar->setValue(0);
        }

        // Сброс переменных
        m_currentVideoPath = decodedPath;
        m_convertedVideoPath.clear();
        m_videoInitialized = false;
        m_videoOptimized = false;
        m_videoErrorCount = 0;
        m_videoDuration = 0;

        // Очистка старого плеера
        if (m_mediaPlayer) {
            m_mediaPlayer->stop();
            m_mediaPlayer->setSource(QUrl());
        }

        setupVideoPlayer();
        show();
    }

    // Обновляем остальные настройки
    m_settings = settings;

    // Применяем настройки масштабирования без перезагрузки видео
    if (!pathChanged) {
        updateGeometry();
    }
}

void VideoRenderer::setAlignment(const QString& alignment)
{
    m_settings.alignment = alignment;
    updateGeometry();
}

void VideoRenderer::setScaling(const QString& scaling)
{
    m_settings.scaling = scaling;
    updateGeometry();
}

void VideoRenderer::setScaleFactor(qreal scaleFactor)
{
    scaleFactor = qBound(0.1, scaleFactor, 3.0);
    if (!qFuzzyCompare(m_settings.scaleFactor, scaleFactor)) {
        m_settings.scaleFactor = scaleFactor;
        updateGeometry();
    }
}

void VideoRenderer::setLoop(bool loop)
{
    m_settings.loopVideo = loop;
    if (m_mediaPlayer) {
        m_mediaPlayer->setLoops(loop ? QMediaPlayer::Infinite : 1);
    }
}

void VideoRenderer::setSound(bool enable)
{
    m_settings.enableSound = enable;
    if (m_mediaPlayer) {
        if (enable) {
            QAudioOutput* audioOutput = new QAudioOutput(this);
            audioOutput->setVolume(0.3);
            m_mediaPlayer->setAudioOutput(audioOutput);
        } else {
            m_mediaPlayer->setAudioOutput(nullptr);
        }
    }
}

void VideoRenderer::reset()
{
    if (m_mediaPlayer && m_videoWidget) {
        bool wasPlaying = isPlaying();
        qint64 currentPosition = m_mediaPlayer->position();

        m_mediaPlayer->pause();
        QTimer::singleShot(DesktopBackgroundSettings::VIDEO_RESTART_DELAY_MS, this, [this, wasPlaying, currentPosition]() {
            if (m_mediaPlayer && !m_currentVideoPath.isEmpty()) {
                QUrl videoUrl = QUrl::fromLocalFile(m_currentVideoPath);
                m_mediaPlayer->setSource(videoUrl);
                m_mediaPlayer->setPosition(currentPosition);
                if (wasPlaying) {
                    m_mediaPlayer->play();
                }
                updateGeometry();
                qDebug() << "Video playback reset successfully";
            }
        });
    }
}

bool VideoRenderer::isPlaying() const
{
    return m_mediaPlayer && m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState;
}

void VideoRenderer::startImmediately()
{
    if (!m_videoInitialized) {
        setupVideoPlayer();
    }
}

void VideoRenderer::ensurePlaying()
{
    if (m_mediaPlayer && m_videoWidget && m_videoWidget->isVisible() && !isPlaying()) {
        if (m_mediaPlayer->mediaStatus() == QMediaPlayer::NoMedia && !m_currentVideoPath.isEmpty()) {
            QUrl videoUrl = QUrl::fromLocalFile(m_currentVideoPath);
            m_mediaPlayer->setSource(videoUrl);
        }
        m_mediaPlayer->play();
    }
}

void VideoRenderer::updateGeometry()
{
    if (!m_videoWidget || !m_geometryUpdateTimer) {
        return;
    }

    // Троттлинг обновлений геометрии
    if (m_geometryUpdateTimer->isActive()) {
        return;
    }

    m_geometryUpdateTimer->start();

    QRect videoRect = QRect(QPoint(0, 0), m_currentSize);

    // Применяем настройки масштабирования
    if (m_settings.scaling == "fill") {
        m_videoWidget->setAspectRatioMode(Qt::KeepAspectRatioByExpanding);
        videoRect = QRect(QPoint(0, 0), m_currentSize);

    } else if (m_settings.scaling == "fit") {
        m_videoWidget->setAspectRatioMode(Qt::KeepAspectRatio);

        // Получаем размер видео
        QSize videoSize = m_videoWidget->sizeHint();
        if (videoSize.isEmpty()) {
            videoSize = QSize(1920, 1080);
        }

        // Масштабируем для вписывания
        videoSize.scale(m_currentSize, Qt::KeepAspectRatio);

        // Применяем выравнивание
        videoRect = applyAlignmentToRect(QRect(QPoint(0, 0), videoSize));

    } else if (m_settings.scaling == "stretch") {
        m_videoWidget->setAspectRatioMode(Qt::IgnoreAspectRatio);
        videoRect = QRect(QPoint(0, 0), m_currentSize);

    } else if (m_settings.scaling == "center") {
        m_videoWidget->setAspectRatioMode(Qt::KeepAspectRatio);
        videoRect = applyAlignmentToRect(m_videoWidget->rect());

    } else if (m_settings.scaling == "zoom") {
        m_videoWidget->setAspectRatioMode(Qt::KeepAspectRatio);

        // Получаем размер видео
        QSize videoSize = m_videoWidget->sizeHint();
        if (videoSize.isEmpty()) {
            videoSize = QSize(1920, 1080);
        }

        // Применяем коэффициент масштабирования
        QSize scaledSize = videoSize * m_settings.scaleFactor;
        scaledSize = scaledSize.boundedTo(m_currentSize * 2);

        // Применяем выравнивание
        videoRect = applyAlignmentToRect(QRect(QPoint(0, 0), scaledSize));
    }

    // Устанавливаем геометрию только если она изменилась
    if (m_videoWidget->geometry() != videoRect) {
        m_videoWidget->setGeometry(videoRect);
        m_videoWidget->update();
    }
}

void VideoRenderer::setupVideoPlayer()
{
    if (m_mediaPlayer || m_currentVideoPath.isEmpty()) {
        return;
    }

    qDebug() << "Setting up video player for:" << m_currentVideoPath;

    // Сначала очищаем предыдущий плеер, если он есть
    if (m_mediaPlayer) {
        m_mediaPlayer->stop();
        m_mediaPlayer->deleteLater();
        m_mediaPlayer = nullptr;
    }

    if (m_videoWidget) {
        m_videoWidget->deleteLater();
        m_videoWidget = nullptr;
    }

    // Проверяем файл
    QFileInfo fileInfo(m_currentVideoPath);
    if (!fileInfo.exists()) {
        qDebug() << "Video file does not exist:" << m_currentVideoPath;
        m_videoInitialized = false;
        return;
    }

    // Проверяем, является ли файл H.264
    QString extension = fileInfo.suffix().toLower();
    bool isH264 = extension == "mp4" || extension == "mov" || extension == "avi" || extension == "m4v" || extension == "mkv";

    if (isH264) {
        // Для H.264 файлов всегда используем конвертацию
        qDebug() << "H.264 video detected, will convert to optimized format";

        // Создаем временный медиаплеер для получения метаданных
        QMediaPlayer* tempPlayer = new QMediaPlayer(this);

        // Подключаем сигнал для получения метаданных
        connect(tempPlayer, &QMediaPlayer::metaDataChanged, this, [this, tempPlayer]() {
            QSize videoSize = tempPlayer->metaData().value(QMediaMetaData::Resolution).toSize();
            if (videoSize.isValid()) {
                qDebug() << "Got video metadata. Size:" << videoSize << "Starting conversion...";

                // Получаем длительность видео
                m_videoDuration = tempPlayer->duration();
                qDebug() << "Video duration:" << m_videoDuration << "ms";

                // Очищаем временный плеер
                tempPlayer->stop();
                tempPlayer->deleteLater();

                // Запускаем конвертацию
                convertH264ToVP9(m_currentVideoPath, videoSize);
            }
        });

        // Подключаем обработчик ошибок
        connect(tempPlayer, &QMediaPlayer::errorOccurred, this, [this, tempPlayer](QMediaPlayer::Error error, const QString &errorString) {
            qDebug() << "Error getting metadata:" << errorString;
            tempPlayer->deleteLater();

            // Если не удалось получить метаданные, используем стандартный размер
            convertH264ToVP9(m_currentVideoPath, QSize(1920, 1080));
        });

        // Устанавливаем источник для получения метаданных
        QUrl videoUrl = QUrl::fromLocalFile(m_currentVideoPath);
        tempPlayer->setSource(videoUrl);

        // Не создаем видео виджет для H.264 - он будет создан после конвертации
        return;
    }

    // Для не-H.264 видео создаем обычный плеер
    createDirectVideoPlayer();
}

void VideoRenderer::createDirectVideoPlayer()
{
    // Создаем медиаплеер
    m_mediaPlayer = new QMediaPlayer(this);

    // Настраиваем аппаратное ускорение
    setupHardwareAcceleration();

    // Создаем видео виджет с правильным родителем
    if (m_parentWidget) {
        m_videoWidget = new QVideoWidget(m_parentWidget);
        // Конфигурируем родительский виджет
        m_parentWidget->setAttribute(Qt::WA_TranslucentBackground, false);
        m_parentWidget->setStyleSheet("background: black;");
        m_parentWidget->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnBottomHint | Qt::Tool);
    } else {
        qDebug() << "No parent widget for video";
        return;
    }

    // Подключаем видео выход
    m_mediaPlayer->setVideoOutput(m_videoWidget);

    // Конфигурируем видео виджет с оптимизациями для плавности
    configureVideoWidget();

    // Сразу устанавливаем правильный размер
    m_videoWidget->resize(m_currentSize);
    m_videoWidget->show();

    // Подключаем сигналы
    connect(m_mediaPlayer, &QMediaPlayer::errorOccurred,
            this, &VideoRenderer::onErrorOccurred);
    connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged,
            this, &VideoRenderer::onMediaStatusChanged);
    connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged,
            this, &VideoRenderer::onPlaybackStateChanged);
    connect(m_mediaPlayer, &QMediaPlayer::durationChanged,
            this, [this](qint64 duration) {
                qDebug() << "Video duration:" << duration << "ms";
                m_videoDuration = duration;
            });

    // Устанавливаем источник
    QUrl videoUrl = QUrl::fromLocalFile(m_currentVideoPath);
    if (videoUrl.isValid()) {
        m_mediaPlayer->setSource(videoUrl);
        qDebug() << "Video source set:" << videoUrl;
    } else {
        qDebug() << "Invalid video URL:" << m_currentVideoPath;
        return;
    }

    // Настраиваем зацикливание
    m_mediaPlayer->setLoops(m_settings.loopVideo ? QMediaPlayer::Infinite : 1);

    // Отключаем звук по умолчанию
    m_mediaPlayer->setAudioOutput(nullptr);

    m_videoInitialized = true;

    qDebug() << "Video player initialized successfully, widget shown";
}

void VideoRenderer::configureVideoWidget()
{
    if (!m_videoWidget) return;

    m_videoWidget->setStyleSheet("background: black; border: none;");
    m_videoWidget->setAttribute(Qt::WA_OpaquePaintEvent, true);
    m_videoWidget->setAttribute(Qt::WA_NoSystemBackground, true);
    m_videoWidget->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_videoWidget->setFocusPolicy(Qt::NoFocus);
    m_videoWidget->setAttribute(Qt::WA_ShowWithoutActivating, true);
    m_videoWidget->setUpdatesEnabled(true);
    m_videoWidget->setAutoFillBackground(false);

    // Критические настройки для плавности
    m_videoWidget->setAttribute(Qt::WA_PaintOnScreen, true);
    m_videoWidget->setAttribute(Qt::WA_NativeWindow, true);
    m_videoWidget->setAttribute(Qt::WA_DontCreateNativeAncestors, true);

    // Устанавливаем высокий приоритет обновления
    m_videoWidget->setAttribute(Qt::WA_UpdatesDisabled, false);
}

void VideoRenderer::setupHardwareAcceleration()
{
    if (!m_mediaPlayer) {
        return;
    }

    // ОСОБЕННО ВАЖНО для Windows
#ifdef Q_OS_WINDOWS
    // Используем Windows Media Foundation для лучшей производительности
    qputenv("QT_MEDIA_BACKEND", "windowsmediafoundation");

    // Настройки для DirectX
    qputenv("QT_OPENGL", "angle");
    qputenv("QT_ANGLE_PLATFORM", "d3d11");

    // Увеличиваем размер буфера для плавности
    qputenv("QT_MEDIA_BUFFER_SIZE", "52428800");
#endif

    // Критические настройки для снижения нагрузки и увеличения плавности
    QSize screenSize = m_targetScreen ? m_targetScreen->size() : QSize(1920, 1080);

    // Оптимизации для разных разрешений
    if (screenSize.width() <= 1920) {
        // 1080p или меньше
        m_mediaPlayer->setProperty("videoBufferSize", 1024 * 1024 * 40);
        m_mediaPlayer->setProperty("videoFrameRate", 60);
        m_mediaPlayer->setProperty("decodingQuality", "high");
        m_mediaPlayer->setProperty("bufferSize", 52428800);
    } else {
        // 4K или выше
        m_mediaPlayer->setProperty("videoBufferSize", 1024 * 1024 * 80);
        m_mediaPlayer->setProperty("videoFrameRate", 60);
        m_mediaPlayer->setProperty("decodingQuality", "medium");
        m_mediaPlayer->setProperty("bufferSize", 104857600);
    }

    // Дополнительные настройки для плавности
    m_mediaPlayer->setProperty("networkBufferSize", 0);
    m_mediaPlayer->setProperty("playbackRate", 1.0);

    if (m_videoWidget) {
        m_videoWidget->setAttribute(Qt::WA_OpaquePaintEvent, true);
        m_videoWidget->setAttribute(Qt::WA_NoSystemBackground, true);
        m_videoWidget->setAutoFillBackground(false);
        m_videoWidget->setAttribute(Qt::WA_PaintOnScreen, true);
    }
}

void VideoRenderer::optimizeVideoPlayback()
{
    if (!m_mediaPlayer || !m_videoWidget || m_videoOptimized) {
        return;
    }

    // Получаем разрешение видео
    QSize videoSize = m_mediaPlayer->metaData().value(QMediaMetaData::Resolution).toSize();

    // Применяем оптимизации для видео высокого разрешения
    if (isHighResolution(videoSize)) {
        qDebug() << "Applying high-res video optimizations:" << videoSize;
        optimizeFor4KVideo();
        m_videoOptimized = true;
    } else {
        // Базовые оптимизации для всех видео
        m_videoWidget->setAttribute(Qt::WA_OpaquePaintEvent, true);
        m_videoWidget->setAttribute(Qt::WA_NoSystemBackground, true);
        m_videoWidget->setAutoFillBackground(false);
        m_videoWidget->setAttribute(Qt::WA_TranslucentBackground, false);
        m_videoWidget->setAttribute(Qt::WA_PaintOnScreen, true);

        m_videoOptimized = true;
    }
}

bool VideoRenderer::isHighResolution(const QSize& size) const
{
    if (size.isEmpty()) return false;

    // Считаем высоким разрешением 1080p и выше
    return (size.width() >= 1920 && size.height() >= 1080) ||
           (m_targetScreen && m_targetScreen->size().width() >= 3840 && m_targetScreen->size().height() >= 2160);
}

void VideoRenderer::optimizeFor4KVideo()
{
    if (!m_mediaPlayer || !m_videoWidget) {
        return;
    }

    qDebug() << "Applying optimizations for high-res video";

    // Критические оптимизации для снижения нагрузки на GPU
    m_videoWidget->setAttribute(Qt::WA_OpaquePaintEvent, true);
    m_videoWidget->setAttribute(Qt::WA_NoSystemBackground, true);
    m_videoWidget->setAutoFillBackground(false);
    m_videoWidget->setAttribute(Qt::WA_TranslucentBackground, false);
    m_videoWidget->setAttribute(Qt::WA_PaintOnScreen, true);

    // Получаем частоту обновления экрана
    int refreshRate = m_targetScreen ? m_targetScreen->refreshRate() : 60;
    int targetFps = qMin(refreshRate, 60);

    // Рассчитываем интервал обновления
    int interval = 1000 / targetFps;
    interval = qBound(16, interval, 33);

    QTimer* updateTimer = new QTimer(this);
    updateTimer->setInterval(interval);
    connect(updateTimer, &QTimer::timeout, this, [this]() {
        if (m_videoWidget && m_videoWidget->isVisible()) {
            m_videoWidget->update();
        }
    });
    updateTimer->start();

    // Увеличиваем буфер для 4K видео
    if (m_mediaPlayer) {
        m_mediaPlayer->setProperty("videoBufferSize", 1024 * 1024 * 100);
        m_mediaPlayer->setProperty("videoFrameRate", targetFps);
        m_mediaPlayer->setProperty("decodingQuality", "medium");
        m_mediaPlayer->setProperty("bufferSize", 157286400);
    }

    qDebug() << "High-res video optimizations applied with target FPS:" << targetFps;
}

QRect VideoRenderer::applyAlignmentToRect(const QRect& contentRect)
{
    QRect result = contentRect;

    if (m_currentSize.isEmpty()) {
        return result;
    }

    QRect targetRect = QRect(QPoint(0, 0), m_currentSize);

    // Горизонтальное выравнивание
    if (m_settings.alignment == "left") {
        result.moveLeft(targetRect.left());
    } else if (m_settings.alignment == "right") {
        result.moveRight(targetRect.right());
    } else { // center
        result.moveCenter(QPoint(targetRect.center().x(), result.center().y()));
    }

    // Вертикальное выравнивание - всегда по центру для видео
    result.moveCenter(QPoint(result.center().x(), targetRect.center().y()));

    return result;
}

void VideoRenderer::handleSystemWake()
{
    static QTime lastUpdate = QTime::currentTime();
    QTime currentTime = QTime::currentTime();

    if (lastUpdate.msecsTo(currentTime) > 3000) {
        if (!m_wasSleeping) {
            m_wasSleeping = true;
            qDebug() << "Detected system wake from sleep, resetting video playback";
            reset();
        }
    }

    lastUpdate = currentTime;
    m_wasSleeping = false;
}

void VideoRenderer::checkStatus()
{
    if (!m_mediaPlayer || !m_videoWidget) {
        return;
    }

    // Проверяем состояние воспроизведения
    if (m_mediaPlayer->playbackState() != QMediaPlayer::PlayingState &&
        m_videoWidget->isVisible()) {
        qDebug() << "Video not playing, attempting to restart";

        if (m_mediaPlayer->mediaStatus() == QMediaPlayer::LoadedMedia ||
            m_mediaPlayer->mediaStatus() == QMediaPlayer::BufferedMedia) {
            // Немедленный перезапуск
            m_mediaPlayer->play();
        } else if (m_mediaPlayer->mediaStatus() == QMediaPlayer::NoMedia ||
                   m_mediaPlayer->mediaStatus() == QMediaPlayer::InvalidMedia) {
            // Перезагружаем видео
            QTimer::singleShot(100, this, [this]() {
                if (m_mediaPlayer && !m_currentVideoPath.isEmpty()) {
                    QUrl videoUrl = QUrl::fromLocalFile(m_currentVideoPath);
                    m_mediaPlayer->setSource(videoUrl);
                    qDebug() << "Reloading video";
                }
            });
        }
    }
}

void VideoRenderer::handleApplicationStateChange(Qt::ApplicationState state)
{
    if (state == Qt::ApplicationActive) {
        // Приложение стало активным
        qDebug() << "Application became active, checking video playback";
        if (m_mediaPlayer && m_videoWidget && m_videoWidget->isVisible()) {
            if (m_mediaPlayer->playbackState() != QMediaPlayer::PlayingState) {
                m_mediaPlayer->play();
                updateGeometry();
            }
        }

        // Запускаем таймер проверки статуса
        if (m_statusCheckTimer) {
            m_statusCheckTimer->start();
        }
    } else if (state == Qt::ApplicationInactive) {
        // Приложение стало неактивным
        if (m_statusCheckTimer) {
            m_statusCheckTimer->stop();
        }
    }
}

void VideoRenderer::convertH264ToVP9(const QString& inputPath, const QSize& videoSize)
{
    // Если процесс уже идет - не запускаем новый, если это тот же файл
    if (m_ffmpegProcess && m_ffmpegProcess->state() != QProcess::NotRunning) {
        return;
    }

    // Формируем путь к кэшу
    QString tempDir = QDir::tempPath() + "/VideoWallpaperCache";
    QDir().mkpath(tempDir);

    QString baseName = QFileInfo(inputPath).completeBaseName();
    QString outputName = QString("optimized_%1_%2x%3.webm")
        .arg(baseName)
        .arg(videoSize.width())
        .arg(videoSize.height());
    QString outputPath = tempDir + "/" + outputName;

    // Если файл уже есть в кэше и он валидный (размер > 0), используем его
    QFileInfo outputInfo(outputPath);
    if (outputInfo.exists() && outputInfo.size() > 0) {
        qDebug() << "Found cached optimized video:" << outputPath;
        handleConversionFinished(0, outputPath);
        return;
    }

    qDebug() << "Starting H.264 -> VP9 conversion for:" << inputPath << "Size:" << videoSize;

    // --- ИНИЦИАЛИЗАЦИЯ ПРОЦЕССА ---
    if (!m_ffmpegProcess) {
        m_ffmpegProcess = new QProcess(this);
    }

    // Подключаем сигналы
    disconnect(m_ffmpegProcess, nullptr, this, nullptr);
    connect(m_ffmpegProcess, &QProcess::readyReadStandardError,
            this, &VideoRenderer::updateConversionProgress);
    connect(m_ffmpegProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, outputPath](int exitCode, QProcess::ExitStatus) {
        handleConversionFinished(exitCode, outputPath);
    });

    // --- НАСТРОЙКА UI (ПРОГРЕСС БАР) ---
    if (!m_progressBar) {
        m_progressBar = new QProgressBar(nullptr);
        m_progressBar->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(0);
        m_progressBar->setTextVisible(true);
        m_progressBar->setFormat("Optimizing Wallpaper... %p%");
        m_progressBar->setFixedSize(400, 40);
        m_progressBar->setStyleSheet(
            "QProgressBar { "
            "   background-color: #2b2b2b; "
            "   color: #ffffff; "
            "   border: 1px solid #555555; "
            "   border-radius: 4px; "
            "   text-align: center;"
            "   font-weight: bold;"
            "} "
            "QProgressBar::chunk { "
            "   background-color: #3daee9; "
            "   border-radius: 3px;"
            "}"
        );
    }

    // --- ЦЕНТРИРОВАНИЕ НА ЦЕЛЕВОМ ЭКРАНЕ ---
    if (m_targetScreen) {
        QRect screenGeo = m_targetScreen->geometry();
        int x = screenGeo.x() + (screenGeo.width() - m_progressBar->width()) / 2;
        int y = screenGeo.y() + (screenGeo.height() - m_progressBar->height()) / 2;
        m_progressBar->move(x, y);
    }

    m_progressBar->setValue(0);
    m_progressBar->show();
    m_progressBar->raise();

    // --- ПАРАМЕТРЫ FFMPEG для максимальной плавности ---
    QStringList args;
    args << "-y" << "-i" << inputPath;

    // Адаптивное масштабирование для производительности
    if (videoSize.width() > 1920 || videoSize.height() > 1080) {
        // Для 4K и выше масштабируем до 1440p для баланса качества/производительности
        args << "-vf" << "scale=2560:-2";
        args << "-b:v" << "8M";
    } else if (videoSize.width() > 1280 || videoSize.height() > 720) {
        // Для 1080p оставляем как есть
        args << "-b:v" << "4M";
    } else {
        // Для HD и ниже
        args << "-b:v" << "2M";
    }

    // VP9 кодек с оптимизациями для плавности
    args << "-c:v" << "libvpx-vp9"
         << "-crf" << "28"
         << "-deadline" << "good"
         << "-cpu-used" << "2"
         << "-row-mt" << "1"
         << "-tile-columns" << "2"
         << "-tile-rows" << "2"
         << "-frame-parallel" << "1"
         << "-lag-in-frames" << "16"
         << "-auto-alt-ref" << "1";

    // Настройки аудио
    if (!m_settings.enableSound) {
        args << "-an";
    } else {
        args << "-c:a" << "libopus"
             << "-b:a" << "128k"
             << "-ac" << "2";
    }

    // Дополнительные настройки для плавности
    args << "-vsync" << "cfr"
         << "-r" << "60"
         << "-pix_fmt" << "yuv420p"
         << "-g" << "120"
         << "-keyint_min" << "60";

    args << outputPath;

    qDebug() << "FFmpeg command:" << "ffmpeg" << args;

    m_ffmpegProcess->start("ffmpeg", args);
}

void VideoRenderer::updateConversionProgress()
{
    if (!m_ffmpegProcess) return;

    // FFmpeg пишет прогресс в StandardError
    QByteArray output = m_ffmpegProcess->readAllStandardError();
    QString outputStr = QString::fromUtf8(output);

    // Ищем time=HH:MM:SS.ms
    static QRegularExpression timeRegex("time=(\\d{2}):(\\d{2}):(\\d{2})\\.(\\d{2})");
    QRegularExpressionMatch match = timeRegex.match(outputStr);

    if (match.hasMatch() && m_videoDuration > 0) {
        int hours = match.captured(1).toInt();
        int mins = match.captured(2).toInt();
        int secs = match.captured(3).toInt();
        int ms = match.captured(4).toInt() * 10;

        qint64 currentMs = ((hours * 3600) + (mins * 60) + secs) * 1000 + ms;

        int progress = static_cast<int>((currentMs * 100.0) / m_videoDuration);
        progress = qBound(0, progress, 99);

        if (m_progressBar) {
            m_progressBar->setValue(progress);
        }
    }
}

void VideoRenderer::handleConversionFinished(int exitCode, const QString& outputPath)
{
    // Скрываем прогресс бар
    if (m_progressBar) {
        m_progressBar->hide();
    }

    if (exitCode == 0 && QFile::exists(outputPath)) {
        qDebug() << "Conversion finished successfully:" << outputPath;
        m_convertedVideoPath = outputPath;

        // Обновляем текущий путь на сконвертированный файл
        m_currentVideoPath = outputPath;

        // Создаем плеер для сконвертированного файла
        createDirectVideoPlayer();

        // Показываем видео
        show();

        m_videoOptimized = true;
    } else {
        qDebug() << "Conversion failed or aborted (Code" << exitCode << ")";
        // Если конвертация не удалась, показываем ошибку
        if (m_parentWidget) {
            QMessageBox::warning(m_parentWidget, "Conversion Error",
                "Failed to convert video. Please try another video file.");
        }
    }
}

void VideoRenderer::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    qDebug() << "Media status changed to:" << status;

    switch (status) {
    case QMediaPlayer::LoadingMedia: {
        qDebug() << "Media loading started";
        break;
    }

    case QMediaPlayer::LoadedMedia: {
        qDebug() << "Media loaded successfully";

        // Получаем информацию о видео
        QSize videoSize = m_mediaPlayer->metaData().value(QMediaMetaData::Resolution).toSize();
        qint64 duration = m_mediaPlayer->duration();
        qDebug() << "Video resolution:" << videoSize << "Duration:" << duration << "ms";

        // Автоматический старт воспроизведения с задержкой для стабилизации
        QTimer::singleShot(100, this, [this]() {
            if (m_mediaPlayer && m_mediaPlayer->playbackState() != QMediaPlayer::PlayingState) {
                m_mediaPlayer->play();
                qDebug() << "Auto-started video playback";
            }
        });

        // Запускаем оптимизацию с задержкой
        if (m_optimizationTimer) {
            m_optimizationTimer->start();
        }

        // Обновляем геометрию с небольшой задержкой
        QTimer::singleShot(50, this, &VideoRenderer::updateGeometry);
        break;
    }

    case QMediaPlayer::BufferedMedia: {
        qDebug() << "Media buffered";

        // Обеспечиваем продолжение воспроизведения
        if (m_mediaPlayer->playbackState() != QMediaPlayer::PlayingState) {
            m_mediaPlayer->play();
            qDebug() << "Playback started after buffering";
        }
        break;
    }

    case QMediaPlayer::EndOfMedia: {
        qDebug() << "Media playback ended";

        if (m_settings.loopVideo && m_mediaPlayer) {
            m_mediaPlayer->setPosition(0);
            m_mediaPlayer->play();
            qDebug() << "Looping playback";
        }
        break;
    }

    case QMediaPlayer::InvalidMedia: {
        qDebug() << "Invalid media file:" << m_currentVideoPath;
        m_videoErrorCount++;

        // Пробуем перезагрузить с задержкой
        QTimer::singleShot(1000, this, [this]() {
            if (m_mediaPlayer && !m_currentVideoPath.isEmpty()) {
                QUrl videoUrl = QUrl::fromLocalFile(m_currentVideoPath);
                m_mediaPlayer->setSource(videoUrl);
                qDebug() << "Retrying video load after invalid media";
            }
        });
        break;
    }

    default:
        break;
    }
}

void VideoRenderer::onErrorOccurred(QMediaPlayer::Error error, const QString &errorString)
{
    qDebug() << "MediaPlayer error:" << error << errorString;

    m_videoErrorCount++;

    int maxRetries = DesktopBackgroundSettings::MAX_VIDEO_ERROR_COUNT;

    if (m_videoErrorCount > maxRetries) {
        qDebug() << "Too many video errors, disabling video for:" << m_currentVideoPath;
        emit videoError(m_currentVideoPath);
    } else {
        // Увеличиваем задержку при каждой следующей попытке
        int delay = DesktopBackgroundSettings::VIDEO_ERROR_RETRY_DELAY_MS * m_videoErrorCount;

        QTimer::singleShot(delay, this, [this]() {
            if (m_mediaPlayer && !m_currentVideoPath.isEmpty()) {
                qDebug() << "Retrying video load (attempt" << m_videoErrorCount << ")";

                // Очищаем полностью и пересоздаем
                m_mediaPlayer->stop();
                m_mediaPlayer->setSource(QUrl());

                QTimer::singleShot(100, this, [this]() {
                    if (m_mediaPlayer) {
                        QUrl videoUrl = QUrl::fromLocalFile(m_currentVideoPath);
                        m_mediaPlayer->setSource(videoUrl);
                    }
                });
            }
        });
    }
}

void VideoRenderer::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    qDebug() << "Playback state changed to:" << state;
}