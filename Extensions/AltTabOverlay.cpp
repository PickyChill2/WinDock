// AltTabOverlay.cpp
#include "AltTabOverlay.h"
#include <QApplication>
#include <QScreen>
#include <QDebug>
#include <QPainter>
#include <QScrollBar>
#include <QKeyEvent>
#include <QMouseEvent>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#include <psapi.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "psapi.lib")
#endif

// AltTabThumbnail implementation (без изменений)
AltTabThumbnail::AltTabThumbnail(HWND hwnd, QWidget* parent)
    : QWidget(parent), m_hwnd(hwnd), m_selected(false)
{
    setFixedSize(200, 150);
    setStyleSheet("background-color: rgba(40, 40, 40, 220); border-radius: 6px; border: 2px solid transparent;");

    // Get window title
    wchar_t title[256];
    GetWindowTextW(hwnd, title, 256);
    m_title = QString::fromWCharArray(title);

    if (m_title.isEmpty()) {
        m_title = "Unknown Window";
    }

    // Get window icon
    HICON hIcon = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0);
    if (!hIcon) {
        hIcon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICONSM);
    }
    if (!hIcon) {
        hIcon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICON);
    }

    if (hIcon) {
        m_icon = QPixmap::fromImage(QImage::fromHICON(hIcon));
    }

#ifdef Q_OS_WIN
    m_thumbHandle = 0;

    // Try to register thumbnail
    BOOL compositionEnabled = FALSE;
    if (SUCCEEDED(DwmIsCompositionEnabled(&compositionEnabled)) && compositionEnabled) {
        HRESULT hr = DwmRegisterThumbnail((HWND)winId(), hwnd, &m_thumbHandle);
        if (SUCCEEDED(hr)) {
            updateThumbnail();
        }
    }
#endif
}

AltTabThumbnail::~AltTabThumbnail()
{
#ifdef Q_OS_WIN
    if (m_thumbHandle) {
        DwmUnregisterThumbnail(m_thumbHandle);
    }
#endif
}

void AltTabThumbnail::updateThumbnail()
{
#ifdef Q_OS_WIN
    if (!m_thumbHandle) return;

    RECT sourceRect;
    GetClientRect(m_hwnd, &sourceRect);

    DWM_THUMBNAIL_PROPERTIES dskThumbProps;
    dskThumbProps.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
    dskThumbProps.fVisible = TRUE;
    dskThumbProps.opacity = 255;
    dskThumbProps.rcDestination.left = 5;
    dskThumbProps.rcDestination.top = 5;
    dskThumbProps.rcDestination.right = width() - 5;
    dskThumbProps.rcDestination.bottom = height() - 30;

    DwmUpdateThumbnailProperties(m_thumbHandle, &dskThumbProps);
#endif
}

void AltTabThumbnail::setSelected(bool selected)
{
    m_selected = selected;
    if (m_selected) {
        setStyleSheet("background-color: rgba(60, 80, 200, 240); border-radius: 6px; border: 3px solid #4CAF50;");
    } else {
        setStyleSheet("background-color: rgba(40, 40, 40, 220); border-radius: 6px; border: 2px solid transparent;");
    }
    update();
}

void AltTabThumbnail::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw background
    if (m_selected) {
        painter.fillRect(rect(), QColor(60, 80, 200, 240));
    } else {
        painter.fillRect(rect(), QColor(40, 40, 40, 220));
    }

    // Draw border
    if (m_selected) {
        painter.setPen(QPen(QColor("#4CAF50"), 3));
    } else {
        painter.setPen(QPen(QColor(100, 100, 100, 100), 2));
    }
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 6, 6);

    // Draw icon if available
    if (!m_icon.isNull()) {
        QPixmap scaledIcon = m_icon.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawPixmap(width()/2 - scaledIcon.width()/2, 15, scaledIcon);
    }

    // Draw window title
    painter.setPen(Qt::white);
    QFont titleFont("Arial", 9, QFont::Normal);
    painter.setFont(titleFont);

    QRect textRect(10, height() - 25, width() - 20, 20);
    QString elidedText = painter.fontMetrics().elidedText(m_title, Qt::ElideRight, textRect.width());
    painter.drawText(textRect, Qt::AlignCenter, elidedText);

    QWidget::paintEvent(event);
}

void AltTabThumbnail::mousePressEvent(QMouseEvent* event)
{
    QWidget* parent = parentWidget();
    while (parent && !dynamic_cast<AltTabOverlay*>(parent)) {
        parent = parent->parentWidget();
    }

    if (AltTabOverlay* overlay = dynamic_cast<AltTabOverlay*>(parent)) {
        overlay->onThumbnailClicked(m_hwnd);
    }

    QWidget::mousePressEvent(event);
}

// AltTabOverlay implementation
AltTabOverlay::AltTabOverlay(QWidget* parent)
    : QWidget(parent), m_visible(false), m_selectedIndex(-1),
      m_altPressed(false)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::StrongFocus);

    // Install event filter to capture all keyboard events
    qApp->installEventFilter(this);

    QScreen *primaryScreen = QGuiApplication::primaryScreen();
    QRect screenGeometry = primaryScreen->geometry();
    setGeometry(screenGeometry);

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Create scroll area for thumbnails
    m_scrollArea = new QScrollArea();
    m_scrollArea->setStyleSheet("background: transparent; border: none;");
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_scrollWidget = new QWidget();
    m_gridLayout = new QGridLayout(m_scrollWidget);
    m_gridLayout->setSpacing(20);
    m_gridLayout->setContentsMargins(40, 40, 40, 40);
    m_gridLayout->setAlignment(Qt::AlignCenter);

    m_scrollArea->setWidget(m_scrollWidget);
    m_scrollArea->setWidgetResizable(true);

    m_mainLayout->addWidget(m_scrollArea);

    // Animation setup
    m_opacityEffect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(m_opacityEffect);

    m_fadeAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_fadeAnimation->setDuration(150);
    m_fadeAnimation->setStartValue(0.0);
    m_fadeAnimation->setEndValue(1.0);

    // Timer to check Alt state
    m_altCheckTimer = new QTimer(this);
    m_altCheckTimer->setInterval(50);
    connect(m_altCheckTimer, &QTimer::timeout, this, &AltTabOverlay::checkAltState);

    connect(m_fadeAnimation, &QPropertyAnimation::finished, this, &AltTabOverlay::onAnimationFinished);

    hide();
}

AltTabOverlay::~AltTabOverlay()
{
    qApp->removeEventFilter(this);
    clearWindows();
}

bool AltTabOverlay::eventFilter(QObject* obj, QEvent* event)
{
    if (m_visible && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

        // Пропускаем Tab, так как он обрабатывается в nativeEvent
        if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
            return true; // Блокируем, но не обрабатываем здесь
        }

        // Обрабатываем остальные клавиши
        handleKeyEvent(keyEvent);
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void AltTabOverlay::handleKeyEvent(QKeyEvent* event)
{
    qDebug() << "Key event:" << event->key() << "modifiers:" << event->modifiers();

    switch (event->key()) {
        case Qt::Key_Escape:
            hideOverlay();
            break;
        case Qt::Key_Left:
            navigateSelection(-1);
            break;
        case Qt::Key_Right:
            navigateSelection(1);
            break;
        case Qt::Key_Up:
            navigateSelection(-4);
            break;
        case Qt::Key_Down:
            navigateSelection(4);
            break;
        case Qt::Key_Enter:
        case Qt::Key_Return:
            if (m_selectedIndex >= 0 && m_selectedIndex < m_thumbnails.size()) {
                activateSelectedWindow();
                hideOverlay();
            }
            break;
        case Qt::Key_Alt:
            // Alt key обрабатывается в nativeEvent
            break;
        default:
            // Игнорируем другие клавиши
            break;
    }
}

// Статическая функция для EnumWindows
BOOL CALLBACK AltTabOverlay::enumWindowsProc(HWND hwnd, LPARAM lParam)
{
    AltTabOverlay* overlay = reinterpret_cast<AltTabOverlay*>(lParam);
    if (overlay && overlay->isValidWindow(hwnd)) {
        overlay->addWindow(hwnd);
    }
    return TRUE;
}

void AltTabOverlay::addWindow(HWND hwnd)
{
    // Get window title for debugging
    wchar_t title[256];
    GetWindowTextW(hwnd, title, 256);
    QString titleStr = QString::fromWCharArray(title);

    qDebug() << "Adding window to Alt+Tab:" << titleStr << "HWND:" << hwnd;

    // Create thumbnail widget
    AltTabThumbnail* thumbnail = new AltTabThumbnail(hwnd, m_scrollWidget);
    m_thumbnails.append(thumbnail);
}

bool AltTabOverlay::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    MSG* msg = static_cast<MSG*>(message);

    // Обрабатываем только когда оверлей видим
    if (m_visible) {
        if (msg->message == WM_SYSKEYDOWN || msg->message == WM_KEYDOWN) {
            if (msg->wParam == VK_TAB) {
                // Alt+Tab или Shift+Alt+Tab
                bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                if (shiftPressed) {
                    navigateSelection(-1);
                } else {
                    navigateSelection(1);
                }
                return true; // Блокируем событие
            }
        }
    }

    // Обработка Alt для отслеживания состояния
    if (msg->message == WM_SYSKEYDOWN || msg->message == WM_KEYDOWN) {
        if (msg->wParam == VK_MENU) {
            m_altPressed = true;
            return true;
        }
    }
    else if (msg->message == WM_SYSKEYUP || msg->message == WM_KEYUP) {
        if (msg->wParam == VK_MENU) {
            m_altPressed = false;

            if (m_visible) {
                activateSelectedWindow();
                hideOverlay();
            }
            return true;
        }
    }
#endif
    return QWidget::nativeEvent(eventType, message, result);
}

void AltTabOverlay::checkAltState()
{
    if (!isAltPressed() && m_visible) {
        m_altPressed = false;
        activateSelectedWindow();
        hideOverlay();
    }
}

bool AltTabOverlay::isAltPressed()
{
#ifdef Q_OS_WIN
    return (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
#else
    return false;
#endif
}

void AltTabOverlay::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Semi-transparent dark background
    QColor backgroundColor = QColor(10, 10, 10, 180);
    painter.fillRect(rect(), backgroundColor);

    // Draw instruction text
    painter.setPen(Qt::white);
    QFont instructionFont("Arial", 12, QFont::Normal);
    painter.setFont(instructionFont);

    QString instruction = "СЫС - 🤡";

    QRect instructionRect(0, height() - 60, width(), 30);
    painter.drawText(instructionRect, Qt::AlignCenter, instruction);

    QWidget::paintEvent(event);
}

void AltTabOverlay::keyPressEvent(QKeyEvent* event)
{
    // Most key handling is done in eventFilter
    QWidget::keyPressEvent(event);
}

void AltTabOverlay::keyReleaseEvent(QKeyEvent* event)
{
    QWidget::keyReleaseEvent(event);
}

void AltTabOverlay::showEvent(QShowEvent* event)
{
    refreshWindows();
    setFocus();
    grabKeyboard();

    // Принудительно устанавливаем фокус и захватываем ввод
    activateWindow();
    raise();

    // Проверяем состояние Alt при показе
    m_altPressed = isAltPressed();
    m_altCheckTimer->start();

    QWidget::showEvent(event);
}

void AltTabOverlay::hideEvent(QHideEvent* event)
{
    releaseKeyboard();
    m_altCheckTimer->stop();
    m_altPressed = false;
    QWidget::hideEvent(event);
}

void AltTabOverlay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateLayout();
}

void AltTabOverlay::showOverlay()
{
    if (!m_visible) {
        qDebug() << "Showing Alt+Tab overlay";
        show();
        raise();
        activateWindow();
        m_fadeAnimation->setDirection(QPropertyAnimation::Forward);
        m_fadeAnimation->start();

#ifdef Q_OS_WIN
        HWND hwnd = (HWND)winId();
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
#endif
        m_visible = true;
    }
}

void AltTabOverlay::hideOverlay()
{
    if (m_visible) {
        qDebug() << "Hiding Alt+Tab overlay";
        m_fadeAnimation->setDirection(QPropertyAnimation::Backward);
        m_fadeAnimation->start();
        m_visible = false;
    }
}

void AltTabOverlay::activateSelectedWindow()
{
    if (m_selectedIndex >= 0 && m_selectedIndex < m_thumbnails.size()) {
        qDebug() << "Activating window:" << m_thumbnails[m_selectedIndex]->windowTitle();
        activateWindowHandle(m_thumbnails[m_selectedIndex]->windowHandle());
    }
}

HWND AltTabOverlay::getTopWindow()
{
#ifdef Q_OS_WIN
    return GetForegroundWindow();
#else
    return 0;
#endif
}

void AltTabOverlay::refreshWindows()
{
    populateWindows();
}

bool AltTabOverlay::isValidWindow(HWND hwnd)
{
    if (!hwnd) return false;
    if (hwnd == (HWND)winId()) return false;
    if (!IsWindowVisible(hwnd)) return false;

    // Check if window is valid
    if (!IsWindow(hwnd)) return false;

    // Skip system windows
    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);
    QString classNameStr = QString::fromWCharArray(className);

    if (classNameStr == "Progman" || classNameStr == "WorkerW" ||
        classNameStr == "Shell_TrayWnd" || classNameStr == "Button" ||
        classNameStr == "Shell_SecondaryTrayWnd" || classNameStr == "Windows.UI.Core.CoreWindow" ||
        classNameStr.startsWith("ApplicationManager_") || classNameStr == "MSCTFIME UI" ||
        classNameStr == "ConsoleWindowClass") {
        return false;
    }

    // Check window title
    wchar_t title[256];
    GetWindowTextW(hwnd, title, 256);
    QString titleStr = QString::fromWCharArray(title);

    if (titleStr.isEmpty() || titleStr == "Start" || titleStr == "Program Manager") {
        return false;
    }

    // Check if window is cloaked
    BOOL isCloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &isCloaked, sizeof(isCloaked))) && isCloaked) {
        return false;
    }

    // Check if minimized
    if (IsIconic(hwnd)) {
        return false;
    }

    // Check window style
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    if (!(style & WS_VISIBLE)) return false;

    // Skip child windows
    if ((style & WS_CHILD)) return false;

    // Skip tool windows unless they are app windows
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if ((exStyle & WS_EX_TOOLWINDOW) && !(exStyle & WS_EX_APPWINDOW)) {
        return false;
    }

    return true;
}

void AltTabOverlay::populateWindows()
{
    clearWindows();
    m_selectedIndex = -1;

#ifdef Q_OS_WIN
    qDebug() << "Populating windows for Alt+Tab...";

    // Use static callback function with proper 'this' pointer
    EnumWindows(enumWindowsProc, (LPARAM)this);
#endif

    qDebug() << "Total windows found for Alt+Tab:" << m_thumbnails.size();

    if (!m_thumbnails.isEmpty()) {
        m_selectedIndex = 0;
        m_thumbnails[0]->setSelected(true);
    }

    updateLayout();
}

void AltTabOverlay::updateLayout()
{
    // Clear existing layout
    QLayoutItem* item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        delete item;
    }

    if (m_thumbnails.isEmpty()) {
        return;
    }

    // Calculate optimal number of columns
    int thumbnailWidth = 200 + 20;
    int availableWidth = m_scrollArea->viewport()->width() - 80;
    int columns = qMax(1, availableWidth / thumbnailWidth);
    columns = qMin(columns, 4);

    qDebug() << "All windows layout - Available width:" << availableWidth << "Columns:" << columns;

    // Add thumbnails to grid
    for (int i = 0; i < m_thumbnails.size(); ++i) {
        int row = i / columns;
        int col = i % columns;
        m_gridLayout->addWidget(m_thumbnails[i], row, col, Qt::AlignCenter);
    }

    m_gridLayout->setAlignment(Qt::AlignCenter);

    m_scrollWidget->adjustSize();

    // Center content vertically
    QTimer::singleShot(0, [this]() {
        if (!m_thumbnails.isEmpty()) {
            int rowHeight = 150 + 20;
            int rows = (m_thumbnails.size() + 3) / 4;
            int totalHeight = rows * rowHeight + 80;

            QScrollBar* vScroll = m_scrollArea->verticalScrollBar();
            if (vScroll && totalHeight < m_scrollArea->viewport()->height()) {
                vScroll->setValue((totalHeight - m_scrollArea->viewport()->height()) / 2);
            }

            // Ensure selected item is visible
            if (m_selectedIndex >= 0 && m_selectedIndex < m_thumbnails.size()) {
                m_scrollArea->ensureWidgetVisible(m_thumbnails[m_selectedIndex]);
            }
        }
    });
}

void AltTabOverlay::clearWindows()
{
    for (AltTabThumbnail* thumbnail : m_thumbnails) {
        thumbnail->setParent(nullptr);
        delete thumbnail;
    }
    m_thumbnails.clear();
}

void AltTabOverlay::activateWindowHandle(HWND hwnd)
{
#ifdef Q_OS_WIN
    qDebug() << "Activating window handle:" << hwnd;

    // Восстанавливаем окно если оно свернуто
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }

    // Активируем окно
    SetForegroundWindow(hwnd);

    // Даем фокус
    SetFocus(hwnd);

    // Поднимаем наверх
    BringWindowToTop(hwnd);

    // Убираем TOPMOST если был установлен
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
#endif
}

void AltTabOverlay::onThumbnailClicked(HWND hwnd)
{
    qDebug() << "Thumbnail clicked for window:" << hwnd;
    activateWindowHandle(hwnd);
    hideOverlay();
}

void AltTabOverlay::navigateSelection(int direction)
{
    if (m_thumbnails.isEmpty()) {
        qDebug() << "No thumbnails available for navigation";
        return;
    }

    qDebug() << "Navigating selection, direction:" << direction << "current index:" << m_selectedIndex;

    // Clear current selection
    if (m_selectedIndex >= 0 && m_selectedIndex < m_thumbnails.size()) {
        m_thumbnails[m_selectedIndex]->setSelected(false);
    }

    // Calculate new selection
    int newIndex = m_selectedIndex + direction;
    if (newIndex < 0) {
        newIndex = m_thumbnails.size() - 1;
    } else if (newIndex >= m_thumbnails.size()) {
        newIndex = 0;
    }

    m_selectedIndex = newIndex;

    // Set new selection
    m_thumbnails[m_selectedIndex]->setSelected(true);

    qDebug() << "New selected index:" << m_selectedIndex << "window:" << m_thumbnails[m_selectedIndex]->windowTitle();

    // Ensure selected item is visible in scroll area
    QTimer::singleShot(0, [this]() {
        m_scrollArea->ensureWidgetVisible(m_thumbnails[m_selectedIndex]);
    });
}

void AltTabOverlay::onAnimationFinished()
{
    if (!m_visible) {
        qDebug() << "Alt+Tab overlay animation finished - hiding";
        hide();
        clearWindows();
    }
}

// AltTabWidget implementation
AltTabWidget::AltTabWidget(QWidget* parent)
    : QWidget(parent)
{
    qDebug() << "AltTabWidget constructor called";

    setFixedSize(1, 1);
    hide();

    m_overlay = new AltTabOverlay();
    qDebug() << "AltTabOverlay created";

#ifdef Q_OS_WIN
    registerHotkey();
#endif
}

AltTabWidget::~AltTabWidget()
{
#ifdef Q_OS_WIN
    unregisterHotkey();
#endif

    if (m_overlay) {
        delete m_overlay;
    }
}

bool AltTabWidget::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        qDebug() << "Hotkey received in AltTabWidget";
        if (msg->wParam == 1001) {
            qDebug() << "Alt+Tab hotkey triggered!";
            if (m_overlay) {
                if (!m_overlay->isVisible()) {
                    m_overlay->showOverlay();
                } else {
                    bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                    int direction = shiftPressed ? -1 : 1;
                    m_overlay->navigateSelection(direction);
                }
            }
            return true;
        }
    }
#endif
    return QWidget::nativeEvent(eventType, message, result);
}

#ifdef Q_OS_WIN
void AltTabWidget::registerHotkey()
{
    qDebug() << "Registering Alt+Tab hotkey...";

    bool registered = RegisterHotKey((HWND)winId(), 1001, MOD_ALT, VK_TAB);
    if (!registered) {
        DWORD error = GetLastError();
        qDebug() << "Failed to register Alt+Tab hotkey. Error:" << error;
    } else {
        qDebug() << "Successfully registered Alt+Tab hotkey using RegisterHotKey";
    }
}

void AltTabWidget::unregisterHotkey()
{
    qDebug() << "Unregistering Alt+Tab hotkey";
    UnregisterHotKey((HWND)winId(), 1001);
}
#endif

// Extension implementation
QWidget* AltTabExtension::createWidget(QWidget* parent)
{
    return new AltTabWidget(parent);
}