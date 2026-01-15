#include "WinTabOverlay.h"
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

// WindowThumbnail implementation
WindowThumbnail::WindowThumbnail(HWND hwnd, QWidget* parent)
    : QWidget(parent), m_hwnd(hwnd), m_selected(false)
{
    setFixedSize(280, 200);
    setStyleSheet("background-color: rgba(40, 40, 40, 220); border-radius: 8px; border: 2px solid transparent;");

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

WindowThumbnail::~WindowThumbnail()
{
#ifdef Q_OS_WIN
    if (m_thumbHandle) {
        DwmUnregisterThumbnail(m_thumbHandle);
    }
#endif
}

void WindowThumbnail::updateThumbnail()
{
#ifdef Q_OS_WIN
    if (!m_thumbHandle) return;

    RECT sourceRect;
    GetClientRect(m_hwnd, &sourceRect);

    DWM_THUMBNAIL_PROPERTIES dskThumbProps;
    dskThumbProps.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
    dskThumbProps.fVisible = TRUE;
    dskThumbProps.opacity = 255;
    dskThumbProps.rcDestination.left = 10;
    dskThumbProps.rcDestination.top = 10;
    dskThumbProps.rcDestination.right = width() - 10;
    dskThumbProps.rcDestination.bottom = height() - 40;

    DwmUpdateThumbnailProperties(m_thumbHandle, &dskThumbProps);
#endif
}

void WindowThumbnail::setSelected(bool selected)
{
    m_selected = selected;
    if (m_selected) {
        setStyleSheet("background-color: rgba(60, 80, 200, 240); border-radius: 8px; border: 3px solid #4CAF50;");
    } else {
        setStyleSheet("background-color: rgba(40, 40, 40, 220); border-radius: 8px; border: 2px solid transparent;");
    }
    update();
}

void WindowThumbnail::paintEvent(QPaintEvent* event)
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
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);

    // Draw icon if available
    if (!m_icon.isNull()) {
        QPixmap scaledIcon = m_icon.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawPixmap(width()/2 - scaledIcon.width()/2, 20, scaledIcon);
    }

    // Draw window title
    painter.setPen(Qt::white);
    QFont titleFont("Arial", 10, QFont::Bold);
    painter.setFont(titleFont);

    QRect textRect(10, height() - 40, width() - 20, 35);
    QString elidedText = painter.fontMetrics().elidedText(m_title, Qt::ElideRight, textRect.width());
    painter.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, elidedText);

    QWidget::paintEvent(event);
}

void WindowThumbnail::mousePressEvent(QMouseEvent* event)
{
    // Find parent WinTabOverlay and notify about click
    QWidget* parent = parentWidget();
    while (parent && !dynamic_cast<WinTabOverlay*>(parent)) {
        parent = parent->parentWidget();
    }

    if (WinTabOverlay* overlay = dynamic_cast<WinTabOverlay*>(parent)) {
        overlay->onThumbnailClicked(m_hwnd);
    }

    QWidget::mousePressEvent(event);
}

// WinTabOverlay implementation
WinTabOverlay::WinTabOverlay(QWidget* parent)
    : QWidget(parent), m_visible(false), m_selectedIndex(-1)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);

    QScreen *primaryScreen = QGuiApplication::primaryScreen();
    QRect screenGeometry = primaryScreen->geometry();
    setGeometry(screenGeometry);

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Create header
    QWidget* headerWidget = new QWidget(this);
    headerWidget->setFixedHeight(80);
    headerWidget->setStyleSheet("background-color: rgba(30, 30, 30, 230); border-bottom: 1px solid #444;");

    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(30, 10, 30, 10);

    QLabel* titleLabel = new QLabel("Task View");
    titleLabel->setStyleSheet("color: white; font-size: 28px; font-weight: bold;");
    headerLayout->addWidget(titleLabel);

    headerLayout->addStretch();

    QLabel* hintLabel = new QLabel("Use arrow keys to navigate • Enter to select • ESC to close");
    hintLabel->setStyleSheet("color: #AAAAAA; font-size: 12px;");
    headerLayout->addWidget(hintLabel);

    headerLayout->addStretch();

    QPushButton* closeButton = new QPushButton("✕");
    closeButton->setFixedSize(40, 40);
    closeButton->setStyleSheet(
        "QPushButton { background-color: transparent; color: white; font-size: 20px; border: 2px solid #666; border-radius: 20px; }"
        "QPushButton:hover { background-color: rgba(255,255,255,30); border-color: #888; }"
        "QPushButton:pressed { background-color: rgba(255,255,255,50); }"
    );
    connect(closeButton, &QPushButton::clicked, this, &WinTabOverlay::toggleVisibility);
    headerLayout->addWidget(closeButton);

    m_mainLayout->addWidget(headerWidget);

    // Create scroll area for thumbnails
    m_scrollArea = new QScrollArea();
    m_scrollArea->setStyleSheet("background: transparent; border: none;");
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_scrollWidget = new QWidget();
    m_gridLayout = new QGridLayout(m_scrollWidget);
    m_gridLayout->setSpacing(25);
    m_gridLayout->setContentsMargins(50, 30, 50, 30);
    m_gridLayout->setAlignment(Qt::AlignCenter);

    m_scrollArea->setWidget(m_scrollWidget);
    m_scrollArea->setWidgetResizable(true);

    m_mainLayout->addWidget(m_scrollArea);

    // Animation setup
    m_opacityEffect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(m_opacityEffect);

    m_fadeAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_fadeAnimation->setDuration(300);
    m_fadeAnimation->setStartValue(0.0);
    m_fadeAnimation->setEndValue(1.0);

    // Connect animation finished using lambda
    QObject::connect(m_fadeAnimation, &QPropertyAnimation::finished, [this]() {
        this->onAnimationFinished();
    });

    hide();
}

WinTabOverlay::~WinTabOverlay()
{
    clearWindows();
}

void WinTabOverlay::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Semi-transparent dark background with blur effect
    QColor backgroundColor = QColor(10, 10, 10, 230);
    painter.fillRect(rect(), backgroundColor);

    // Draw subtle grid pattern
    painter.setPen(QPen(QColor(30, 30, 30, 50), 1));
    for (int x = 0; x < width(); x += 50) {
        painter.drawLine(x, 0, x, height());
    }
    for (int y = 0; y < height(); y += 50) {
        painter.drawLine(0, y, width(), y);
    }
}

void WinTabOverlay::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        toggleVisibility();
        break;
    case Qt::Key_Left:
        navigateSelection(-1);
        break;
    case Qt::Key_Right:
        navigateSelection(1);
        break;
    case Qt::Key_Up:
        navigateSelection(-3); // Move up one row (assuming 3 columns)
        break;
    case Qt::Key_Down:
        navigateSelection(3); // Move down one row
        break;
    case Qt::Key_Enter:
    case Qt::Key_Return:
        if (m_selectedIndex >= 0 && m_selectedIndex < m_thumbnails.size()) {
            activateWindowHandle(m_thumbnails[m_selectedIndex]->windowHandle());
            toggleVisibility();
        }
        break;
    case Qt::Key_Tab:
        if (event->modifiers() & Qt::ControlModifier) {
            navigateSelection(1);
        }
        break;
    }

    QWidget::keyPressEvent(event);
}

void WinTabOverlay::showEvent(QShowEvent* event)
{
    refreshWindows();
    setFocus();
    QWidget::showEvent(event);
}

void WinTabOverlay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateLayout();
}

void WinTabOverlay::toggleVisibility()
{
    if (m_visible) {
        m_fadeAnimation->setDirection(QPropertyAnimation::Backward);
        m_fadeAnimation->start();
    } else {
        show();
        raise();
        QWidget::activateWindow();
        m_fadeAnimation->setDirection(QPropertyAnimation::Forward);
        m_fadeAnimation->start();

#ifdef Q_OS_WIN
        HWND hwnd = (HWND)winId();
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
#endif
    }
    m_visible = !m_visible;
}

void WinTabOverlay::refreshWindows()
{
    populateWindows();
}

bool WinTabOverlay::isValidWindow(HWND hwnd)
{
    if (hwnd == (HWND)winId()) return false;
    if (!IsWindowVisible(hwnd)) return false;

    // Skip system windows and other special windows
    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);
    QString classNameStr = QString::fromWCharArray(className);

    // Skip some common system windows
    if (classNameStr == "Progman" || classNameStr == "WorkerW" ||
        classNameStr == "Shell_TrayWnd" || classNameStr == "Button" ||
        classNameStr == "Shell_SecondaryTrayWnd") {
        return false;
    }

    // Check if window has a title
    wchar_t title[256];
    GetWindowTextW(hwnd, title, 256);
    if (wcslen(title) == 0) return false;

    // Check if window is cloaked (Windows 8+)
    BOOL isCloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &isCloaked, sizeof(isCloaked))) && isCloaked) {
        return false;
    }

    // Check window style
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    if (!(style & WS_OVERLAPPEDWINDOW)) {
        return false;
    }

    return true;
}

void WinTabOverlay::populateWindows()
{
    clearWindows();
    m_selectedIndex = -1;

#ifdef Q_OS_WIN
    qDebug() << "Populating windows...";

    // Enum windows with improved filtering
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        WinTabOverlay* overlay = reinterpret_cast<WinTabOverlay*>(lParam);

        if (!overlay->isValidWindow(hwnd)) {
            return TRUE;
        }

        // Get window title for debugging
        wchar_t title[256];
        GetWindowTextW(hwnd, title, 256);
        QString titleStr = QString::fromWCharArray(title);
        qDebug() << "Found window:" << titleStr;

        // Create thumbnail widget
        WindowThumbnail* thumbnail = new WindowThumbnail(hwnd, overlay->m_scrollWidget);
        overlay->m_thumbnails.append(thumbnail);

        return TRUE;
    }, (LPARAM)this);
#endif

    qDebug() << "Total windows found:" << m_thumbnails.size();
    updateLayout();

    // Select first window
    if (!m_thumbnails.isEmpty()) {
        m_selectedIndex = 0;
        m_thumbnails[0]->setSelected(true);
    }
}

void WinTabOverlay::updateLayout()
{
    // Clear existing layout
    QLayoutItem* item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        delete item;
    }

    if (m_thumbnails.isEmpty()) {
        return;
    }

    // Calculate optimal number of columns based on available width
    int thumbnailWidth = 280 + 25; // width + spacing
    int availableWidth = m_scrollArea->width() - 100; // account for margins
    int columns = qMax(1, availableWidth / thumbnailWidth);
    columns = qMin(columns, 4); // Max 4 columns

    qDebug() << "Layout update - Available width:" << availableWidth << "Columns:" << columns;

    // Add thumbnails to grid layout
    for (int i = 0; i < m_thumbnails.size(); ++i) {
        int row = i / columns;
        int col = i % columns;
        m_gridLayout->addWidget(m_thumbnails[i], row, col, Qt::AlignCenter);
    }

    m_scrollWidget->adjustSize();
}

void WinTabOverlay::clearWindows()
{
    for (WindowThumbnail* thumbnail : m_thumbnails) {
        thumbnail->setParent(nullptr);
        delete thumbnail;
    }
    m_thumbnails.clear();
}

void WinTabOverlay::activateWindowHandle(HWND hwnd)
{
#ifdef Q_OS_WIN
    qDebug() << "Activating window:" << hwnd;

    // Restore if minimized
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }

    // Bring to front
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    // Force window to be topmost temporarily, then set back to normal
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
#endif
}

void WinTabOverlay::onThumbnailClicked(HWND hwnd)
{
    activateWindowHandle(hwnd);
    toggleVisibility();
}

void WinTabOverlay::navigateSelection(int direction)
{
    if (m_thumbnails.isEmpty()) return;

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

    // Ensure selected item is visible in scroll area
    QTimer::singleShot(0, [this]() {
        m_scrollArea->ensureWidgetVisible(m_thumbnails[m_selectedIndex]);
    });
}

void WinTabOverlay::onAnimationFinished()
{
    if (!m_visible) {
        hide();
        clearWindows();
    }
}

// WinTabWidget implementation
#ifdef Q_OS_WIN
HHOOK WinTabWidget::m_keyboardHook = nullptr;
WinTabWidget* WinTabWidget::m_instance = nullptr;

LRESULT CALLBACK WinTabWidget::KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* kbdStruct = (KBDLLHOOKSTRUCT*)lParam;

        // Check for Win+Tab
        if (wParam == WM_KEYDOWN && kbdStruct->vkCode == VK_TAB) {
            bool winPressed = (GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000);

            if (winPressed) {
                if (WinTabWidget::m_instance && WinTabWidget::m_instance->m_overlay) {
                    WinTabWidget::m_instance->m_overlay->toggleVisibility();
                    return 1; // Block the key
                }
            }
        }
    }

    return CallNextHookEx(WinTabWidget::m_keyboardHook, nCode, wParam, lParam);
}
#endif

WinTabWidget::WinTabWidget(QWidget* parent)
    : QWidget(parent)
{
    qDebug() << "WinTabWidget constructor called";

    // This widget is invisible and just handles the hotkey
    setFixedSize(1, 1);
    hide();

    m_overlay = new WinTabOverlay();
    qDebug() << "WinTabOverlay created";

#ifdef Q_OS_WIN
    m_instance = this;
    registerHotkey();
#endif
}

WinTabWidget::~WinTabWidget()
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
void WinTabWidget::registerHotkey()
{
    // Set low-level keyboard hook
    m_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, GetModuleHandle(NULL), 0);
    if (!m_keyboardHook) {
        qDebug() << "Failed to set keyboard hook for Win+Tab. Error:" << GetLastError();

        // Alternative method with RegisterHotKey
        bool registered = RegisterHotKey((HWND)winId(), 1, MOD_WIN, VK_TAB);
        if (!registered) {
            qDebug() << "Failed to register Win+Tab hotkey. Error:" << GetLastError();
        } else {
            qDebug() << "Successfully registered Win+Tab hotkey using RegisterHotKey";
        }
    } else {
        qDebug() << "Keyboard hook set successfully for Win+Tab";
    }
}

void WinTabWidget::unregisterHotkey()
{
    if (m_keyboardHook) {
        UnhookWindowsHookEx(m_keyboardHook);
        m_keyboardHook = nullptr;
        qDebug() << "Keyboard hook unset";
    }

    UnregisterHotKey((HWND)winId(), 1);
}

bool WinTabWidget::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        if (msg->wParam == 1) { // Our hotkey
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
QWidget* WinTabExtension::createWidget(QWidget* parent)
{
    return new WinTabWidget(parent);
}