#ifndef WINTABOVERLAY_H
#define WINTABOVERLAY_H

#include "..//ExtensionManager.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPushButton>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#include <psapi.h>
#endif

class WindowThumbnail : public QWidget
{
public:
    explicit WindowThumbnail(HWND hwnd, QWidget* parent = nullptr);
    ~WindowThumbnail();

    HWND windowHandle() const { return m_hwnd; }
    void updateThumbnail();
    void setSelected(bool selected);
    QString windowTitle() const { return m_title; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    HWND m_hwnd;
    bool m_selected;
    QString m_title;
    QPixmap m_icon;

#ifdef Q_OS_WIN
    HTHUMBNAIL m_thumbHandle;
#endif
};

class WinTabOverlay : public QWidget
{
public:
    explicit WinTabOverlay(QWidget* parent = nullptr);
    ~WinTabOverlay();

    void toggleVisibility();
    void refreshWindows();
    void onThumbnailClicked(HWND hwnd);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void populateWindows();
    void clearWindows();
    void activateWindowHandle(HWND hwnd);
    void navigateSelection(int direction);
    void onAnimationFinished();
    bool isValidWindow(HWND hwnd);
    void updateLayout();

    QVBoxLayout* m_mainLayout;
    QScrollArea* m_scrollArea;
    QWidget* m_scrollWidget;
    QGridLayout* m_gridLayout;

    QList<WindowThumbnail*> m_thumbnails;
    int m_selectedIndex;

    QPropertyAnimation* m_fadeAnimation;
    QGraphicsOpacityEffect* m_opacityEffect;
    bool m_visible;
};

class WinTabWidget : public QWidget
{
public:
    explicit WinTabWidget(QWidget* parent = nullptr);
    ~WinTabWidget();

    WinTabOverlay* m_overlay;

#ifdef Q_OS_WIN
private:
    void registerHotkey();
    void unregisterHotkey();
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

    static LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static HHOOK m_keyboardHook;
    static WinTabWidget* m_instance;
#endif
};

class WinTabExtension : public ExtensionInterface
{
public:
    WinTabExtension() = default;
    ~WinTabExtension() = default;

    QString name() const override { return "WinTab"; }
    QString version() const override { return "1.0"; }
    QWidget* createWidget(QWidget* parent = nullptr) override;
    ExtensionDisplayLocation displayLocation() const override { return ExtensionDisplayLocation::None; }
};

REGISTER_EXTENSION(WinTabExtension)

#endif // WINTABOVERLAY_H