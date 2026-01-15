#include "WinDockBar.h"
#include <QApplication>
#include <QScreen>
#include <QDebug>
#include <QGuiApplication>
#include <QPainter>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbt.h>

// Инициализация статических членов
HHOOK WinDockBarWidget::m_keyboardHook = nullptr;
WinDockBarWidget* WinDockBarWidget::m_instance = nullptr;

// Статическая функция обработки хука клавиатуры
LRESULT CALLBACK WinDockBarWidget::KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* kbdStruct = (KBDLLHOOKSTRUCT*)lParam;

        // Проверяем нажатие Win+G
        if (wParam == WM_KEYDOWN && kbdStruct->vkCode == 'G') {
            // Проверяем, зажата ли клавиша Win
            bool winPressed = (GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000);

            if (winPressed) {
                if (WinDockBarWidget::m_instance && WinDockBarWidget::m_instance->m_overlay) {
                    WinDockBarWidget::m_instance->m_overlay->toggleVisibility();
                    return 1; // Блокируем клавишу
                }
            }
        }
    }

    return CallNextHookEx(WinDockBarWidget::m_keyboardHook, nCode, wParam, lParam);
}
#endif

// ClockWidget implementation
ClockWidget::ClockWidget(QWidget* parent)
    : QWidget(parent)
{
    QString clockStyle = QString(
        "background-color: rgba(%1, %2, %3, %4);"
        "border: 1px solid rgba(%5, %6, %7, %8);"
        "border-radius: 10px;"
        "padding: %9px;"
    ).arg(WinDockBarConstants::SECONDARY_BACKGROUND_COLOR().red())
     .arg(WinDockBarConstants::SECONDARY_BACKGROUND_COLOR().green())
     .arg(WinDockBarConstants::SECONDARY_BACKGROUND_COLOR().blue())
     .arg(WinDockBarConstants::WIDGET_BACKGROUND_ALPHA)
     .arg(WinDockBarConstants::BORDER_COLOR().red())
     .arg(WinDockBarConstants::BORDER_COLOR().green())
     .arg(WinDockBarConstants::BORDER_COLOR().blue())
     .arg(WinDockBarConstants::BORDER_ALPHA)
     .arg(WinDockBarConstants::WIDGET_CONTENTS_MARGINS);

    setStyleSheet(clockStyle);
    setMinimumWidth(WinDockBarConstants::CLOCK_MIN_WIDTH);
    setMaximumWidth(WinDockBarConstants::CLOCK_MAX_WIDTH);
    setMinimumHeight(WinDockBarConstants::CLOCK_MIN_HEIGHT);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(10, 10, 10, 10);
    m_layout->setSpacing(5);

    m_timeLabel = new QLabel();
    m_timeLabel->setStyleSheet(QString(
        "color: %1; font-size: %2px; font-weight: bold; font-family: %3; "
        "background: transparent; padding: 2px;"
    ).arg(WinDockBarConstants::TEXT_COLOR().name())
     .arg(WinDockBarConstants::FONT_SIZE_LARGE)
     .arg(WinDockBarConstants::FONT_FAMILY()));
    m_timeLabel->setAlignment(Qt::AlignCenter);
    m_timeLabel->setWordWrap(false);

    m_dateLabel = new QLabel();
    m_dateLabel->setStyleSheet(QString(
        "color: %1; font-size: %2px; font-family: %3; "
        "background: transparent; padding: 2px;"
    ).arg(WinDockBarConstants::TEXT_COLOR().name())
     .arg(WinDockBarConstants::FONT_SIZE_NORMAL)
     .arg(WinDockBarConstants::FONT_FAMILY()));
    m_dateLabel->setAlignment(Qt::AlignCenter);
    m_dateLabel->setWordWrap(false);

    m_layout->addWidget(m_timeLabel);
    m_layout->addWidget(m_dateLabel);

    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ClockWidget::updateTime);
    m_timer->start(1000);

    updateTime();
}

void ClockWidget::updateTime()
{
    QDateTime current = QDateTime::currentDateTime();

    // Format: 21:15 - 13.10.25
    QString timeText = current.toString("HH:mm - dd.MM.yy");

    // Get day of week in Russian
    QString dayOfWeek;
    switch (current.date().dayOfWeek()) {
        case 1: dayOfWeek = "Понедельник"; break;
        case 2: dayOfWeek = "Вторник"; break;
        case 3: dayOfWeek = "Среда"; break;
        case 4: dayOfWeek = "Четверг"; break;
        case 5: dayOfWeek = "Пятница"; break;
        case 6: dayOfWeek = "Суббота"; break;
        case 7: dayOfWeek = "Воскресенье"; break;
    }

    m_timeLabel->setText(timeText);
    m_dateLabel->setText(dayOfWeek);
}

// WinDockBarOverlay implementation
WinDockBarOverlay::WinDockBarOverlay(QWidget* parent)
    : QWidget(parent), m_visible(false)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);

    QScreen *primaryScreen = QGuiApplication::primaryScreen();
    QRect screenGeometry = primaryScreen->geometry();
    setGeometry(screenGeometry);

#ifdef Q_OS_WIN
    HWND hwnd = (HWND)winId();
    ITaskbarList *taskbar = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskbarList, reinterpret_cast<void**>(&taskbar)))) {
        taskbar->HrInit();
        taskbar->DeleteTab(hwnd);
        taskbar->Release();
    }
#endif

    // Основной layout с тремя колонками: левая, центральная, правая
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(
        WinDockBarConstants::OVERLAY_MARGINS,
        WinDockBarConstants::OVERLAY_MARGINS,
        WinDockBarConstants::OVERLAY_MARGINS,
        WinDockBarConstants::OVERLAY_MARGINS
    );
    m_mainLayout->setSpacing(0);

    // Левая часть - микшер (посередине слева)
    QVBoxLayout* leftLayout = new QVBoxLayout();
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addStretch(); // Добавляем растяжение сверху

    m_volumeMixer = new VolumeMixerWidget(this);
    leftLayout->addWidget(m_volumeMixer, 0, Qt::AlignLeft); // Выравнивание по левому краю

    leftLayout->addStretch(); // Добавляем растяжение снизу

    // Центральная часть - пустая (можно добавить другие виджеты)
    QVBoxLayout* centerLayout = new QVBoxLayout();
    centerLayout->addStretch();

    // Правая часть - часы
    QVBoxLayout* rightLayout = new QVBoxLayout();
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addStretch();

    m_clockWidget = new ClockWidget(this);
    rightLayout->addWidget(m_clockWidget, 0, Qt::AlignRight | Qt::AlignBottom);

    // Добавляем все layout'ы в основной
    m_mainLayout->addLayout(leftLayout);
    m_mainLayout->addLayout(centerLayout, 1); // Центральная часть растягивается
    m_mainLayout->addLayout(rightLayout);

    // Animation setup
    m_opacityEffect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(m_opacityEffect);

    m_fadeAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_fadeAnimation->setDuration(300);
    m_fadeAnimation->setStartValue(0.0);
    m_fadeAnimation->setEndValue(1.0);

    hide();
}

void WinDockBarOverlay::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Рисуем полупрозрачный фон
    QColor backgroundColor = WinDockBarConstants::PRIMARY_BACKGROUND_COLOR();
    backgroundColor.setAlpha(WinDockBarConstants::OVERLAY_ALPHA);
    painter.fillRect(rect(), backgroundColor);
}

WinDockBarOverlay::~WinDockBarOverlay()
{
}

void WinDockBarOverlay::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        toggleVisibility();
    }
    QWidget::keyPressEvent(event);
}

void WinDockBarOverlay::mousePressEvent(QMouseEvent* event)
{
    // Закрываем при клике в любом месте фона
    toggleVisibility();
    QWidget::mousePressEvent(event);
}

void WinDockBarOverlay::toggleVisibility()
{
    if (m_visible) {
        m_fadeAnimation->setDirection(QPropertyAnimation::Backward);
        m_fadeAnimation->start();
        QTimer::singleShot(300, this, &QWidget::hide);
    } else {
        show();
        raise();
        activateWindow();
        m_fadeAnimation->setDirection(QPropertyAnimation::Forward);
        m_fadeAnimation->start();

#ifdef Q_OS_WIN
        // Принудительно поднимаем поверх всего
        HWND hwnd = (HWND)winId();
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
#endif
    }
    m_visible = !m_visible;
}

// WinDockBarWidget implementation
WinDockBarWidget::WinDockBarWidget(QWidget* parent)
    : QWidget(parent)
{
    qDebug() << "WinDockBarWidget constructor called";

    // This widget is invisible and just handles the hotkey
    setFixedSize(1, 1);
    hide();

    m_overlay = new WinDockBarOverlay();
    qDebug() << "WinDockBarOverlay created";

#ifdef Q_OS_WIN
    m_instance = this;
    registerHotkey();
#endif
}

WinDockBarWidget::~WinDockBarWidget()
{
#ifdef Q_OS_WIN
    unregisterHotkey();
    m_instance = nullptr;
#endif

    if (m_overlay) {
        delete m_overlay;
    }
}

#ifdef Q_OS_WIN
void WinDockBarWidget::registerHotkey()
{
    // Устанавливаем низкоуровневый хук клавиатуры
    m_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, GetModuleHandle(NULL), 0);
    if (!m_keyboardHook) {
        qDebug() << "Failed to set keyboard hook. Error:" << GetLastError();

        // Альтернативный метод с RegisterHotKey
        bool registered = RegisterHotKey((HWND)winId(), 1, MOD_WIN, 'G');
        if (!registered) {
            qDebug() << "Failed to register Win+G hotkey. Error:" << GetLastError();
            // Пробуем альтернативную комбинацию
            registered = RegisterHotKey((HWND)winId(), 2, MOD_WIN | MOD_SHIFT, 'G');
            if (registered) {
                qDebug() << "Registered Win+Shift+G as alternative";
            } else {
                qDebug() << "Failed to register any hotkey";
            }
        } else {
            qDebug() << "Successfully registered Win+G hotkey using RegisterHotKey";
        }
    } else {
        qDebug() << "Keyboard hook set successfully for Win+G";
    }
}

void WinDockBarWidget::unregisterHotkey()
{
    if (m_keyboardHook) {
        UnhookWindowsHookEx(m_keyboardHook);
        m_keyboardHook = nullptr;
        qDebug() << "Keyboard hook unset";
    }

    // Также снимаем зарегистрированные горячие клавиши
    UnregisterHotKey((HWND)winId(), 1);
    UnregisterHotKey((HWND)winId(), 2);
}

bool WinDockBarWidget::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        if (msg->wParam == 1 || msg->wParam == 2) { // Наша горячая клавиша
            if (m_overlay) {
                m_overlay->toggleVisibility();
                return true;
            }
        }
    }
#endif
    return QWidget::nativeEvent(eventType, message, result);
}
#endif

// Extension implementation
QWidget* WinDockBarExtension::createWidget(QWidget* parent)
{
    return new WinDockBarWidget(parent);
}