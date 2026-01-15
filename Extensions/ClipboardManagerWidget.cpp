#include "ClipboardManagerWidget.h"
#include "ClipboardHistoryOverlay.h"
#include <QDebug>

ClipboardManagerWidget::ClipboardManagerWidget(QWidget* parent)
    : QWidget(parent)
#ifdef Q_OS_WIN
    , m_hotkeyId(1)
#endif
{
    qDebug() << "ClipboardManagerWidget constructor called";

    // This widget is invisible and just handles the hotkey
    setFixedSize(1, 1);
    hide();

    m_overlay = new ClipboardHistoryOverlay();
    qDebug() << "ClipboardHistoryOverlay created";

#ifdef Q_OS_WIN
    registerHotkey();
#endif
}

ClipboardManagerWidget::~ClipboardManagerWidget()
{
#ifdef Q_OS_WIN
    unregisterHotkey();
#endif

    if (m_overlay) {
        delete m_overlay;
    }
}

#ifdef Q_OS_WIN
void ClipboardManagerWidget::registerHotkey()
{
    bool registered = RegisterHotKey((HWND)winId(), m_hotkeyId, MOD_WIN, 0x56); // 0x56 is 'V'
    if (registered) {
        qDebug() << "Successfully registered Win+V hotkey for window:" << (HWND)winId();
    } else {
        DWORD error = GetLastError();
        qDebug() << "Failed to register Win+V hotkey. Error:" << error;
        // Try alternative combination
        registered = RegisterHotKey((HWND)winId(), m_hotkeyId + 1, MOD_WIN | MOD_SHIFT, 0x56);
        if (registered) {
            qDebug() << "Registered Win+Shift+V as alternative";
        } else {
            qDebug() << "Failed to register any clipboard hotkey";
        }
    }
}

void ClipboardManagerWidget::unregisterHotkey()
{
    UnregisterHotKey((HWND)winId(), m_hotkeyId);
    UnregisterHotKey((HWND)winId(), m_hotkeyId + 1);
}

bool ClipboardManagerWidget::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        qDebug() << "WM_HOTKEY received, wParam:" << msg->wParam;
        if (msg->wParam == m_hotkeyId || msg->wParam == m_hotkeyId + 1) {
            if (m_overlay) {
                qDebug() << "Toggling clipboard overlay visibility";
                m_overlay->toggleVisibility();
                *result = 1;
                return true;
            }
        }
    }
#endif
    return QWidget::nativeEvent(eventType, message, result);
}
#endif