#include "MultiDesktopManager.h"
#include "DesktopBackground.h"
#include "../TopPanel.h"
#include "../BackgroundSettingsDialog.h"
#include <QDebug>
#include <QGuiApplication>
#include <QTimer>
#include <QMetaObject>
#include <QFileInfo>
#include <QUrl>

MultiDesktopManager::MultiDesktopManager(QObject* parent)
    : QObject(parent)
    , m_primaryTopPanel(nullptr)
    , m_settingsDialog(nullptr)
    , m_enabled(true)
    , m_useHardwareAcceleration(true)
    , m_videoDecodingThreads(DesktopBackgroundSettings::VIDEO_DECODING_THREADS)
    , m_limitVideoFps(true)
    , m_targetFps(DesktopBackgroundSettings::LOW_POWER_TARGET_FPS)
    , m_useVideoCache(false)
    , m_videoCacheSize(10)
{
    qDebug() << "MultiDesktopManager created";
}

MultiDesktopManager::~MultiDesktopManager()
{
    qDebug() << "MultiDesktopManager destroyed";

    if (m_settingsDialog) {
        m_settingsDialog->deleteLater();
    }

    saveSettings();
    clearAllDesktopBackgrounds();
}

MultiDesktopManager& MultiDesktopManager::instance()
{
    static MultiDesktopManager instance;
    return instance;
}

void MultiDesktopManager::initialize()
{
    loadSettings();
    optimizeVideoSettings();

    QSettings settings("MyCompany", "DockApp");
    m_enabled = settings.value("General/UseDockDesktop", true).toBool();

    qDebug() << "MultiDesktopManager initializing with enabled =" << m_enabled;

    if (m_enabled) {
        // Подключаем обработчики добавления/удаления экранов
        connect(qApp, &QGuiApplication::screenAdded,
                this, &MultiDesktopManager::handleScreenAdded);
        connect(qApp, &QGuiApplication::screenRemoved,
                this, &MultiDesktopManager::handleScreenRemoved);

        // Создаем фон для каждого экрана
        for (QScreen* screen : QGuiApplication::screens()) {
            createDesktopForScreen(screen);
        }

        // Быстрая инициализация видео
        QTimer::singleShot(DesktopBackgroundSettings::POST_INITIALIZATION_DELAY_MS, this, [this]() {
            qDebug() << "Performing final video initialization...";
            quickStartAllVideos();
        });

        qDebug() << "MultiDesktopManager initialized for" << m_desktopBackgrounds.size() << "screens";
    }
}

void MultiDesktopManager::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }

    qDebug() << "MultiDesktopManager: Setting enabled =" << enabled;
    m_enabled = enabled;

    if (enabled) {
        // Отключаем предыдущие соединения
        disconnect(qApp, &QGuiApplication::screenAdded, this, &MultiDesktopManager::handleScreenAdded);
        disconnect(qApp, &QGuiApplication::screenRemoved, this, &MultiDesktopManager::handleScreenRemoved);

        // Включаем функционал
        connect(qApp, &QGuiApplication::screenAdded, this, &MultiDesktopManager::handleScreenAdded);
        connect(qApp, &QGuiApplication::screenRemoved, this, &MultiDesktopManager::handleScreenRemoved);

        // Создаем фоны для всех экранов
        for (QScreen* screen : QGuiApplication::screens()) {
            createDesktopForScreen(screen);
        }

        // Быстрая инициализация видео
        QTimer::singleShot(DesktopBackgroundSettings::POST_INITIALIZATION_DELAY_MS,
                          this, &MultiDesktopManager::quickStartAllVideos);

        qDebug() << "MultiDesktopManager enabled for" << m_desktopBackgrounds.size() << "screens";
    } else {
        // Выключаем функционал
        clearAllDesktopBackgrounds();
        disconnect(qApp, &QGuiApplication::screenAdded, this, &MultiDesktopManager::handleScreenAdded);
        disconnect(qApp, &QGuiApplication::screenRemoved, this, &MultiDesktopManager::handleScreenRemoved);
        qDebug() << "MultiDesktopManager disabled";
    }

    // Сохраняем настройки
    QSettings settings("MyCompany", "DockApp");
    settings.setValue("General/UseDockDesktop", m_enabled);
    settings.sync();
}

void MultiDesktopManager::updateDesktopState()
{
    if (m_enabled) {
        for (QScreen* screen : QGuiApplication::screens()) {
            createDesktopForScreen(screen);
        }
    } else {
        clearAllDesktopBackgrounds();
    }
}

void MultiDesktopManager::createDesktopForScreen(QScreen* screen)
{
    if (!m_enabled) {
        return;
    }

    if (!screen) {
        qDebug() << "Cannot create desktop - screen is null";
        return;
    }

    // Проверяем, не существует ли уже DesktopBackground для этого экрана
    for (DesktopBackground* background : m_desktopBackgrounds) {
        if (background->getTargetScreen() == screen) {
            qDebug() << "Desktop background already exists for screen:" << screen->name();
            return;
        }
    }

    // Получаем настройки для экрана
    ScreenSettings settings = m_screenSettings.value(screen->name());

    // Устанавливаем флаг isVideo правильно
    if (!settings.backgroundImage.isEmpty()) {
        QFileInfo fileInfo(settings.backgroundImage);
        if (fileInfo.exists()) {
            settings.isVideo = DesktopBackground::isVideoFile(settings.backgroundImage);
            qDebug() << "Creating desktop for screen:" << screen->name()
                     << "isVideo:" << settings.isVideo
                     << "path:" << settings.backgroundImage
                     << "exists:" << true;
        } else {
            qDebug() << "Background file not found for screen:" << screen->name()
                     << "path:" << settings.backgroundImage;
            settings.isVideo = DesktopBackground::isVideoFile(settings.backgroundImage);
        }
    } else {
        qDebug() << "No background set for screen:" << screen->name();
    }

    // Создаем новый фон для экрана
    DesktopBackground* background = new DesktopBackground(screen, settings);
    m_desktopBackgrounds.append(background);

    qDebug() << "Created desktop background for screen:" << screen->name()
             << "with video:" << settings.isVideo;
}

void MultiDesktopManager::removeDesktopForScreen(QScreen* screen)
{
    if (!screen) return;

    for (int i = 0; i < m_desktopBackgrounds.size(); ++i) {
        DesktopBackground* background = m_desktopBackgrounds.at(i);
        if (background->getTargetScreen() == screen) {
            m_desktopBackgrounds.removeAt(i);
            background->deleteLater();
            break;
        }
    }
}

void MultiDesktopManager::clearAllDesktopBackgrounds()
{
    qDebug() << "Clearing all desktop backgrounds";

    for (DesktopBackground* background : m_desktopBackgrounds) {
        if (background) {
            background->disconnect();
            background->deleteLater();
        }
    }
    m_desktopBackgrounds.clear();
}

void MultiDesktopManager::handleScreenAdded(QScreen* screen)
{
    if (!m_enabled) {
        return;
    }

    qDebug() << "Screen added:" << screen->name();
    createDesktopForScreen(screen);
}

void MultiDesktopManager::handleScreenRemoved(QScreen* screen)
{
    if (!m_enabled) {
        return;
    }

    qDebug() << "Screen removed:" << screen->name();
    removeDesktopForScreen(screen);
}

void MultiDesktopManager::setBackgroundForScreen(QScreen* screen, const ScreenSettings& settings)
{
    if (!screen) return;

    // Обновляем настройки
    m_screenSettings[screen->name()] = settings;

    // Обновляем существующий фон или создаем новый
    bool found = false;
    for (DesktopBackground* background : m_desktopBackgrounds) {
        if (background->getTargetScreen() == screen) {
            background->setBackgroundImage(settings.backgroundImage);
            background->setBackgroundAlignment(settings.alignment);
            background->setBackgroundScaling(settings.scaling);
            background->setBackgroundScaleFactor(settings.scaleFactor);
            background->setVideoLoop(settings.loopVideo);
            background->setVideoSound(settings.enableSound);
            found = true;
            break;
        }
    }

    // Если фон не найден, создаем его
    if (!found && m_enabled) {
        createDesktopForScreen(screen);
    }

    saveSettings();
}

void MultiDesktopManager::setBackgroundForAllScreens(const ScreenSettings& settings)
{
    for (QScreen* screen : QGuiApplication::screens()) {
        setBackgroundForScreen(screen, settings);
    }
}

ScreenSettings MultiDesktopManager::getBackgroundForScreen(QScreen* screen) const
{
    if (!screen) return ScreenSettings();
    return m_screenSettings.value(screen->name());
}

void MultiDesktopManager::setBackgroundAlignment(QScreen* screen, const QString& alignment)
{
    if (!screen) return;

    m_screenSettings[screen->name()].alignment = alignment;
    updateBackgroundForScreen(screen);
    saveSettings();
}

void MultiDesktopManager::setBackgroundScaling(QScreen* screen, const QString& scaling)
{
    if (!screen) return;

    m_screenSettings[screen->name()].scaling = scaling;
    updateBackgroundForScreen(screen);
    saveSettings();
}

void MultiDesktopManager::setBackgroundScaleFactor(QScreen* screen, qreal scaleFactor)
{
    if (!screen) return;

    scaleFactor = qBound(0.1, scaleFactor, 3.0);
    m_screenSettings[screen->name()].scaleFactor = scaleFactor;
    updateBackgroundForScreen(screen);
    saveSettings();
}

void MultiDesktopManager::setVideoLoop(QScreen* screen, bool loop)
{
    if (!screen) return;

    m_screenSettings[screen->name()].loopVideo = loop;
    updateBackgroundForScreen(screen);
    saveSettings();
}

void MultiDesktopManager::setVideoSound(QScreen* screen, bool enable)
{
    if (!screen) return;

    m_screenSettings[screen->name()].enableSound = enable;
    updateBackgroundForScreen(screen);
    saveSettings();
}

void MultiDesktopManager::updateBackgroundForScreen(QScreen* screen)
{
    if (!screen) return;

    for (DesktopBackground* background : m_desktopBackgrounds) {
        if (background->getTargetScreen() == screen) {
            ScreenSettings settings = m_screenSettings.value(screen->name());
            background->setBackgroundAlignment(settings.alignment);
            background->setBackgroundScaling(settings.scaling);
            background->setBackgroundScaleFactor(settings.scaleFactor);
            background->setVideoLoop(settings.loopVideo);
            background->setVideoSound(settings.enableSound);
            break;
        }
    }
}

void MultiDesktopManager::showBackgroundSettingsDialog(QScreen* screen)
{
    if (!m_enabled) {
        QMessageBox::information(nullptr, "Информация",
            "Настройки фона недоступны, так как функция 'Рабочий стол дока' отключена в общих настройках.");
        return;
    }

    if (!m_settingsDialog) {
        QWidget* parent = nullptr;
        for (DesktopBackground* background : m_desktopBackgrounds) {
            if (background && background->isVisible()) {
                parent = background;
                break;
            }
        }

        m_settingsDialog = new BackgroundSettingsDialog(this, parent);
        connect(m_settingsDialog, &QDialog::finished,
                this, &MultiDesktopManager::onSettingsDialogClosed);
    }

    if (screen) {
        m_settingsDialog->setCurrentScreen(screen);
    }

    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

void MultiDesktopManager::onSettingsDialogClosed()
{
    transferFocusToTopPanel();
    m_settingsDialog->deleteLater();
    m_settingsDialog = nullptr;
}

void MultiDesktopManager::transferFocusToTopPanel()
{
    if (!m_enabled) return;

    if (m_primaryTopPanel && m_primaryTopPanel->isVisible()) {
        m_primaryTopPanel->activateWindow();
        m_primaryTopPanel->setFocus();
        m_primaryTopPanel->raise();
        m_primaryTopPanel->update();
    }
}

void MultiDesktopManager::setPrimaryTopPanel(TopPanel* panel)
{
    m_primaryTopPanel = panel;
}

TopPanel* MultiDesktopManager::getPrimaryTopPanel() const
{
    return m_primaryTopPanel;
}

void MultiDesktopManager::optimizeVideoSettings()
{
    QSettings settings("MyCompany", "DockApp");
    m_useHardwareAcceleration = settings.value("Video/UseHardwareAcceleration", true).toBool();
    m_videoDecodingThreads = settings.value("Video/DecodingThreads", DesktopBackgroundSettings::VIDEO_DECODING_THREADS).toInt();
    m_limitVideoFps = settings.value("Video/LimitFps", true).toBool();
    m_targetFps = settings.value("Video/TargetFps", DesktopBackgroundSettings::LOW_POWER_TARGET_FPS).toInt();
    m_useVideoCache = settings.value("Video/UseVideoCache", false).toBool();
    m_videoCacheSize = settings.value("Video/CacheSize", 10).toInt();

    // Ограничиваем FPS в допустимых пределах
    m_targetFps = qBound(DesktopBackgroundSettings::MIN_TARGET_FPS,
                        m_targetFps,
                        DesktopBackgroundSettings::MAX_TARGET_FPS);

    qDebug() << "Video settings optimized:";
    qDebug() << "  Hardware Acceleration =" << m_useHardwareAcceleration;
    qDebug() << "  Decoding Threads =" << m_videoDecodingThreads;
    qDebug() << "  Target FPS =" << m_targetFps;
    qDebug() << "  Use Cache =" << m_useVideoCache;
    qDebug() << "  Cache Size =" << m_videoCacheSize << "MB";
}

void MultiDesktopManager::initializeVideoBackgrounds()
{
    qDebug() << "Initializing video backgrounds after startup...";

    for (DesktopBackground* background : m_desktopBackgrounds) {
        if (background && background->getSettings().isVideo) {
            qDebug() << "Initializing video for screen:"
                     << (background->getTargetScreen() ? background->getTargetScreen()->name() : "unknown");

            // Принудительно показываем окно
            background->show();

            // Используем быстрый запуск видео
            QMetaObject::invokeMethod(background, "startVideoImmediately", Qt::QueuedConnection);
        }
    }
}

void MultiDesktopManager::quickStartAllVideos()
{
    qDebug() << "Quick starting all videos...";

    for (DesktopBackground* background : m_desktopBackgrounds) {
        if (background && background->getSettings().isVideo) {
            qDebug() << "Quick starting video for screen:"
                     << (background->getTargetScreen() ? background->getTargetScreen()->name() : "unknown");

            // Немедленный запуск видео
            QMetaObject::invokeMethod(background, "startVideoImmediately", Qt::QueuedConnection);
        }
    }
}

void MultiDesktopManager::resetAllVideoPlayback()
{
    qDebug() << "Resetting video playback for all backgrounds";

    for (DesktopBackground* background : m_desktopBackgrounds) {
        if (background && background->getSettings().isVideo) {
            QMetaObject::invokeMethod(background, "resetVideoPlayback", Qt::QueuedConnection);
        }
    }
}

void MultiDesktopManager::loadSettings()
{
    QSettings settings("MyCompany", "DockApp");

    // Загружаем настройки для каждого экрана
    settings.beginGroup("DesktopBackgrounds");
    QStringList screenNames = settings.childKeys();
    for (const QString& screenName : screenNames) {
        QString imagePath = settings.value(screenName).toString();
        if (!imagePath.isEmpty()) {
            // Декодируем путь если он закодирован
            QString decodedPath = QUrl::fromPercentEncoding(imagePath.toUtf8());

            // Полная проверка существования файла и его типа
            QFileInfo fileInfo(decodedPath);
            if (fileInfo.exists() && fileInfo.isFile()) {
                m_screenSettings[screenName].backgroundImage = decodedPath;
                m_screenSettings[screenName].isVideo = DesktopBackground::isVideoFile(decodedPath);
                qDebug() << "Loaded background for screen:" << screenName
                         << "path:" << decodedPath
                         << "exists:" << true
                         << "isVideo:" << m_screenSettings[screenName].isVideo;
            } else {
                qDebug() << "Background file not found for screen:" << screenName
                         << "path:" << decodedPath;
                // Очищаем путь если файл не существует
                m_screenSettings[screenName].backgroundImage = "";
                m_screenSettings[screenName].isVideo = false;
            }
        }
    }
    settings.endGroup();

    // Загружаем дополнительные настройки
    settings.beginGroup("DesktopSettings");
    for (const QString& screenName : settings.childGroups()) {
        settings.beginGroup(screenName);
        m_screenSettings[screenName].alignment = settings.value("alignment", "center").toString();
        m_screenSettings[screenName].scaling = settings.value("scaling", "fill").toString();
        m_screenSettings[screenName].scaleFactor = settings.value("scaleFactor", 1.0).toDouble();
        m_screenSettings[screenName].loopVideo = settings.value("loopVideo", true).toBool();
        m_screenSettings[screenName].enableSound = settings.value("enableSound", false).toBool();

        // Убедимся, что isVideo синхронизирован с текущим файлом
        if (!m_screenSettings[screenName].backgroundImage.isEmpty()) {
            m_screenSettings[screenName].isVideo =
                DesktopBackground::isVideoFile(m_screenSettings[screenName].backgroundImage);
        }
        settings.endGroup();
    }
    settings.endGroup();
}

void MultiDesktopManager::saveSettings()
{
    QSettings settings("MyCompany", "DockApp");

    // Сохраняем основные настройки
    settings.setValue("General/UseDockDesktop", m_enabled);

    // Сохраняем настройки видео
    settings.setValue("Video/UseHardwareAcceleration", m_useHardwareAcceleration);
    settings.setValue("Video/DecodingThreads", m_videoDecodingThreads);
    settings.setValue("Video/LimitFps", m_limitVideoFps);
    settings.setValue("Video/TargetFps", m_targetFps);
    settings.setValue("Video/UseVideoCache", m_useVideoCache);
    settings.setValue("Video/CacheSize", m_videoCacheSize);

    // Сохраняем фоновые изображения (кодируем спецсимволы)
    settings.beginGroup("DesktopBackgrounds");
    for (auto it = m_screenSettings.constBegin(); it != m_screenSettings.constEnd(); ++it) {
        if (!it.value().backgroundImage.isEmpty()) {
            // Кодируем спецсимволы для корректного сохранения в QSettings
            QString encodedPath = QUrl::toPercentEncoding(it.value().backgroundImage, "/:\\");
            settings.setValue(it.key(), encodedPath);
        }
    }
    settings.endGroup();

    // Сохраняем дополнительные настройки
    settings.beginGroup("DesktopSettings");
    for (auto it = m_screenSettings.constBegin(); it != m_screenSettings.constEnd(); ++it) {
        settings.beginGroup(it.key());
        settings.setValue("alignment", it.value().alignment);
        settings.setValue("scaling", it.value().scaling);
        settings.setValue("scaleFactor", it.value().scaleFactor);
        settings.setValue("loopVideo", it.value().loopVideo);
        settings.setValue("enableSound", it.value().enableSound);
        settings.endGroup();
    }
    settings.endGroup();

    settings.sync();
}