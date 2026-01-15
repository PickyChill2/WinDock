// AltTabOverlay.h
#ifndef ALTTABOVERLAY_H
#define ALTTABOVERLAY_H

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
#include <QApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#include <psapi.h>
#endif

class AltTabThumbnail : public QWidget
{
public:
    explicit AltTabThumbnail(HWND hwnd, QWidget* parent = nullptr);
    ~AltTabThumbnail();

    HWND windowHandle() const { return m_hwnd; }
    void setSelected(bool selected);
    QString windowTitle() const { return m_title; }
    void updateThumbnail();

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

class AltTabOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit AltTabOverlay(QWidget* parent = nullptr);
    ~AltTabOverlay();

    void showOverlay();
    void hideOverlay();
    void refreshWindows();
    void onThumbnailClicked(HWND hwnd);
    void navigateSelection(int direction);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private slots:
    void onAnimationFinished();
    void checkAltState();

private:
    static BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam);
    void addWindow(HWND hwnd);
    void populateWindows();
    void clearWindows();
    void activateWindowHandle(HWND hwnd);
    bool isValidWindow(HWND hwnd);
    void updateLayout();
    void activateSelectedWindow();
    bool isAltPressed();
    HWND getTopWindow();
    void handleKeyEvent(QKeyEvent* event);

    QVBoxLayout* m_mainLayout;
    QScrollArea* m_scrollArea;
    QWidget* m_scrollWidget;
    QGridLayout* m_gridLayout;

    QList<AltTabThumbnail*> m_thumbnails;
    int m_selectedIndex;

    QPropertyAnimation* m_fadeAnimation;
    QGraphicsOpacityEffect* m_opacityEffect;
    bool m_visible;
    bool m_altPressed;
    QTimer* m_altCheckTimer;
};

class AltTabWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AltTabWidget(QWidget* parent = nullptr);
    ~AltTabWidget();

    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
#ifdef Q_OS_WIN
    void registerHotkey();
    void unregisterHotkey();
#endif

    AltTabOverlay* m_overlay;
};

class AltTabExtension : public ExtensionInterface
{
public:
    AltTabExtension() = default;
    ~AltTabExtension() = default;

    QString name() const override { return "AltTab"; }
    QString version() const override { return "1.0"; }
    QWidget* createWidget(QWidget* parent = nullptr) override;
    ExtensionDisplayLocation displayLocation() const override { return ExtensionDisplayLocation::None; }
};

REGISTER_EXTENSION(AltTabExtension)

#endif // ALTTABOVERLAY_H