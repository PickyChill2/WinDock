#include "DesktopBackground.h"

#include "MediaRenderer.h"

#include "ImageRenderer.h"

#include "VideoRenderer.h"

#include "MultiDesktopManager.h"

#include <QDebug>

#include <QFileInfo>

#include <QPainter>

#include <QLinearGradient>

#include <QMenu>

#include <QAction>

#include <QMessageBox>

#include <QProcess>

#include <QApplication>

#include <QGuiApplication>

#include <QUrl>

#include <QTime>



DesktopBackground::DesktopBackground(QScreen* targetScreen, const ScreenSettings& settings, QWidget* parent)

    : QWidget(parent)

    , m_targetScreen(targetScreen)

    , m_settings(settings)

    , m_renderer(nullptr)

    , m_quickStartTimer(nullptr)

    , m_statusCheckTimer(nullptr)

    , m_wakeupTimer(nullptr)

    , m_contextMenu(nullptr)

    , m_settingsAction(nullptr)

    , m_amdSettingsAction(nullptr)

{

    // Оптимизированные флаги окна для быстрого запуска

    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnBottomHint);



    // Критически важные атрибуты

    setAttribute(Qt::WA_ShowWithoutActivating, true);

    setAttribute(Qt::WA_TranslucentBackground, false);

    setAttribute(Qt::WA_NoSystemBackground, false);

    setAttribute(Qt::WA_OpaquePaintEvent, true);

    setAttribute(Qt::WA_PaintOnScreen, false);

    setProperty("DesktopBackground", true);



    // Отключаем фокус

    setFocusPolicy(Qt::NoFocus);

    setAttribute(Qt::WA_InputMethodEnabled, false);



    // Включаем обновления

    setUpdatesEnabled(true);

    setAttribute(Qt::WA_UpdatesDisabled, false);



    // Устанавливаем геометрию экрана

    if (m_targetScreen) {

        setGeometry(m_targetScreen->geometry());

    }



    // Устанавливаем флаг isVideo правильно

    if (!m_settings.backgroundImage.isEmpty()) {

        m_settings.isVideo = isVideoFile(m_settings.backgroundImage);

        qDebug() << "DesktopBackground constructor: screen =" << (m_targetScreen ? m_targetScreen->name() : "unknown")

                 << "isVideo =" << m_settings.isVideo

                 << "path =" << m_settings.backgroundImage;

    }



    // Инициализируем контекстное меню

    createContextMenu();



    // Настраиваем таймеры

    setupTimers(); // Вызов метода настройки таймеров



    // Создаем рендерер

    createRenderer();



    // Подключаем обработчик изменения состояния приложения

    connect(qApp, &QApplication::applicationStateChanged,

            this, &DesktopBackground::handleApplicationStateChange);



    // Показываем окно

    show();

    lower();

}



// НОВЫЙ МЕТОД: Настройка таймеров

void DesktopBackground::setupTimers()

{

    // Таймер для быстрого запуска видео

    if (m_settings.isVideo) {

        m_quickStartTimer = new QTimer(this);

        m_quickStartTimer->setSingleShot(true);

        connect(m_quickStartTimer, &QTimer::timeout,

                this, &DesktopBackground::startVideoImmediately);

        m_quickStartTimer->start(DesktopBackgroundSettings::INITIAL_VIDEO_DELAY_MS);

    }



    // Таймер для проверки пробуждения системы

    m_wakeupTimer = new QTimer(this);

    m_wakeupTimer->setInterval(DesktopBackgroundSettings::WAKEUP_CHECK_INTERVAL_MS);

    connect(m_wakeupTimer, &QTimer::timeout,

            this, &DesktopBackground::handleSystemWake);

    m_wakeupTimer->start();



    // Таймер для проверки статуса видео

    m_statusCheckTimer = new QTimer(this);

    m_statusCheckTimer->setInterval(DesktopBackgroundSettings::VIDEO_STATUS_CHECK_INTERVAL_MS);

    connect(m_statusCheckTimer, &QTimer::timeout,

            this, &DesktopBackground::checkVideoStatus);



    if (m_settings.isVideo) {

        m_statusCheckTimer->start();

    }

}



DesktopBackground::~DesktopBackground()

{

    cleanupRenderer();



    // Останавливаем и удаляем таймеры

    if (m_quickStartTimer) {

        m_quickStartTimer->stop();

        delete m_quickStartTimer;

    }

    if (m_statusCheckTimer) {

        m_statusCheckTimer->stop();

        delete m_statusCheckTimer;

    }

    if (m_wakeupTimer) {

        m_wakeupTimer->stop();

        delete m_wakeupTimer;

    }



    // Удаляем контекстное меню

    if (m_contextMenu) {

        delete m_contextMenu;

    }

}



void DesktopBackground::createContextMenu()

{

    m_contextMenu = new QMenu(this);



    m_settingsAction = m_contextMenu->addAction("Настройки фона");

    connect(m_settingsAction, &QAction::triggered,

            this, &DesktopBackground::openBackgroundSettings);



    m_amdSettingsAction = m_contextMenu->addAction("Открыть настройки видеокарты");

    connect(m_amdSettingsAction, &QAction::triggered,

            this, &DesktopBackground::openAmdSettings);



    // Стилизация меню

    m_contextMenu->setStyleSheet(

        "QMenu {"

        "    background-color: #2D2D2D;"

        "    border: 1px solid #404040;"

        "    border-radius: 8px;"

        "    padding: 4px;"

        "}"

        "QMenu::item {"

        "    background-color: transparent;"

        "    color: white;"

        "    padding: 6px 12px;"

        "    border-radius: 4px;"

        "    margin: 2px;"

        "}"

        "QMenu::item:selected {"

        "    background-color: #404040;"

        "}"

    );

}



void DesktopBackground::createRenderer()

{

    cleanupRenderer();



    if (m_settings.isVideo) {

        m_renderer = new VideoRenderer(m_targetScreen, this);

    } else {

        m_renderer = new ImageRenderer(this);

    }



    if (m_renderer) {

        applyRendererSettings();

        m_renderer->initialize();

    }

}



void DesktopBackground::cleanupRenderer()

{

    if (m_renderer) {

        delete m_renderer;

        m_renderer = nullptr;

    }

}



void DesktopBackground::applyRendererSettings()

{

    if (!m_renderer) return;



    m_renderer->setSettings(m_settings);



    if (m_settings.isVideo) {

        VideoRenderer* videoRenderer = qobject_cast<VideoRenderer*>(m_renderer);

        if (videoRenderer) {

            videoRenderer->setLoop(m_settings.loopVideo);

            videoRenderer->setSound(m_settings.enableSound);

        }

    }

}



bool DesktopBackground::isVideoFile(const QString& filePath)

{

    if (filePath.isEmpty()) return false;



    QString extension = QFileInfo(filePath).suffix().toLower();

    QStringList videoExtensions = {"mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "m4v"};



    return videoExtensions.contains(extension);

}



void DesktopBackground::startVideoImmediately()

{

    if (m_settings.isVideo && m_renderer) {

        VideoRenderer* videoRenderer = qobject_cast<VideoRenderer*>(m_renderer);

        if (videoRenderer) {

            videoRenderer->startImmediately();

        }

    }

}



void DesktopBackground::showEvent(QShowEvent* event)

{

    QWidget::showEvent(event);



    qDebug() << "DesktopBackground showEvent for screen:"

             << (m_targetScreen ? m_targetScreen->name() : "unknown");



    if (m_renderer) {

        m_renderer->show();

    }

}



void DesktopBackground::hideEvent(QHideEvent* event)

{

    QWidget::hideEvent(event);



    if (m_renderer) {

        m_renderer->hide();

    }

}



void DesktopBackground::resizeEvent(QResizeEvent* event)

{

    QWidget::resizeEvent(event);



    if (m_renderer) {

        m_renderer->resize(event->size());

    }

}



void DesktopBackground::paintEvent(QPaintEvent* event)

{

    Q_UNUSED(event);



    if (m_renderer) {

        QPainter painter(this);

        m_renderer->paint(&painter, rect());

    } else {

        // Градиентный фон по умолчанию

        QPainter painter(this);

        QLinearGradient gradient(0, 0, 0, height());

        gradient.setColorAt(0, QColor(30, 30, 40));

        gradient.setColorAt(1, QColor(10, 10, 20));

        painter.fillRect(rect(), gradient);

    }

}



void DesktopBackground::contextMenuEvent(QContextMenuEvent* event)

{

    if (m_contextMenu) {

        m_contextMenu->exec(event->globalPos());

    }

}



void DesktopBackground::mousePressEvent(QMouseEvent* event)

{

    Q_UNUSED(event);

    transferFocusToTopPanel();

}



void DesktopBackground::setBackgroundImage(const QString& imagePath)

{

    qDebug() << "Setting background image for screen:"

             << (m_targetScreen ? m_targetScreen->name() : "unknown")

             << "path:" << imagePath;



    // Обновляем настройки

    m_settings.backgroundImage = imagePath;

    m_settings.isVideo = isVideoFile(imagePath);



    // Останавливаем таймеры

    if (m_quickStartTimer) {

        m_quickStartTimer->stop();

    }

    if (m_statusCheckTimer) {

        m_statusCheckTimer->stop();

    }



    // Пересоздаем рендерер

    createRenderer();



    // Настраиваем таймеры в зависимости от типа контента

    if (m_settings.isVideo) {

        if (!m_quickStartTimer) {

            m_quickStartTimer = new QTimer(this);

            m_quickStartTimer->setSingleShot(true);

            connect(m_quickStartTimer, &QTimer::timeout,

                    this, &DesktopBackground::startVideoImmediately);

        }

        m_quickStartTimer->start(DesktopBackgroundSettings::QUICK_START_DELAY_MS);



        if (m_statusCheckTimer) {

            m_statusCheckTimer->start();

        }

    }



    update();

}



void DesktopBackground::setBackgroundAlignment(const QString& alignment)

{

    if (m_settings.alignment != alignment) {

        m_settings.alignment = alignment;

        if (m_renderer) {

            m_renderer->setAlignment(alignment);

            update();

        }

    }

}



void DesktopBackground::setBackgroundScaling(const QString& scaling)

{

    if (m_settings.scaling != scaling) {

        m_settings.scaling = scaling;

        if (m_renderer) {

            m_renderer->setScaling(scaling);

            update();

        }

    }

}



void DesktopBackground::setBackgroundScaleFactor(qreal scaleFactor)

{

    scaleFactor = qBound(0.1, scaleFactor, 3.0);



    if (!qFuzzyCompare(m_settings.scaleFactor, scaleFactor)) {

        m_settings.scaleFactor = scaleFactor;

        if (m_renderer) {

            m_renderer->setScaleFactor(scaleFactor);

            update();

        }

    }

}



void DesktopBackground::setVideoLoop(bool loop)

{

    m_settings.loopVideo = loop;

    if (m_renderer && m_settings.isVideo) {

        VideoRenderer* videoRenderer = qobject_cast<VideoRenderer*>(m_renderer);

        if (videoRenderer) {

            videoRenderer->setLoop(loop);

        }

    }

}



void DesktopBackground::setVideoSound(bool enable)

{

    m_settings.enableSound = enable;

    if (m_renderer && m_settings.isVideo) {

        VideoRenderer* videoRenderer = qobject_cast<VideoRenderer*>(m_renderer);

        if (videoRenderer) {

            videoRenderer->setSound(enable);

        }

    }

}



void DesktopBackground::handleSystemWake()

{

    // Проверяем, не спит ли система

    static QTime lastUpdate = QTime::currentTime();

    QTime currentTime = QTime::currentTime();



    if (lastUpdate.msecsTo(currentTime) > 3000 && m_settings.isVideo) {

        qDebug() << "Detected system wake from sleep, resetting video playback for screen:"

                 << (m_targetScreen ? m_targetScreen->name() : "unknown");

        resetVideoPlayback();

    }



    lastUpdate = currentTime;

}



void DesktopBackground::checkVideoStatus()

{

    if (m_renderer && m_settings.isVideo) {

        VideoRenderer* videoRenderer = qobject_cast<VideoRenderer*>(m_renderer);

        if (videoRenderer) {

            videoRenderer->checkStatus();

        }

    }

}



void DesktopBackground::handleApplicationStateChange(Qt::ApplicationState state)

{

    if (m_renderer && m_settings.isVideo) {

        VideoRenderer* videoRenderer = qobject_cast<VideoRenderer*>(m_renderer);

        if (videoRenderer) {

            videoRenderer->handleApplicationStateChange(state);

        }

    }

}



void DesktopBackground::resetVideoPlayback()

{

    if (m_renderer && m_settings.isVideo) {

        VideoRenderer* videoRenderer = qobject_cast<VideoRenderer*>(m_renderer);

        if (videoRenderer) {

            videoRenderer->reset();

        }

    }

}



void DesktopBackground::openBackgroundSettings()

{

    MultiDesktopManager::instance().showBackgroundSettingsDialog(m_targetScreen);

}



void DesktopBackground::transferFocusToTopPanel()

{

    MultiDesktopManager::instance().transferFocusToTopPanel();

}



void DesktopBackground::openAmdSettings()

{

    QString amdPath = "C:\\Program Files\\AMD\\CNext\\CNext\\RadeonSoftware.exe";



    if (QFile::exists(amdPath)) {

        QProcess::startDetached(amdPath);

    } else {

        QMessageBox::warning(this, "Ошибка",

            "AMD Software: Adrenalin Edition не найден.\nПуть: " + amdPath);

    }

}