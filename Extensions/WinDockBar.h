#ifndef WINDOCKBAR_H
#define WINDOCKBAR_H

#include "../ExtensionManager.h"
#include "WinDockBarConstants.h"
#include "VolumeMixerWidget.h"
#include "AppVolumeWidget.h"
#include "../AudioDeviceManager.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QTimer>
#include <QDateTime>
#include <QKeyEvent>
#include <QApplication>
#include <QScreen>
#include <QPushButton>
#include <QMenu>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QProcess>

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <propsys.h>
#include <shobjidl.h>  // Для ITaskbarList
#endif

class ClockWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ClockWidget(QWidget* parent = nullptr);

private slots:
    void updateTime();

private:
    QVBoxLayout* m_layout;
    QLabel* m_timeLabel;
    QLabel* m_dateLabel;
    QTimer* m_timer;
};

class WinDockBarOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit WinDockBarOverlay(QWidget* parent = nullptr);
    ~WinDockBarOverlay();

    void toggleVisibility();

    void updateAudioInfo();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QHBoxLayout* m_mainLayout;
    VolumeMixerWidget* m_volumeMixer;
    ClockWidget* m_clockWidget;
    QPropertyAnimation* m_fadeAnimation;
    QGraphicsOpacityEffect* m_opacityEffect;
    bool m_visible;
};

class WinDockBarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WinDockBarWidget(QWidget* parent = nullptr);
    ~WinDockBarWidget();

    WinDockBarOverlay* m_overlay;

#ifdef Q_OS_WIN
private:
    void registerHotkey();
    void unregisterHotkey();
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

    static LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static HHOOK m_keyboardHook;
    static WinDockBarWidget* m_instance;
#endif
};

class WinDockBarExtension : public ExtensionInterface
{
public:
    WinDockBarExtension() = default;
    ~WinDockBarExtension() = default;

    QString name() const override { return "WinDockBar"; }
    QString version() const override { return "1.0"; }
    QWidget* createWidget(QWidget* parent = nullptr) override;
};

REGISTER_EXTENSION(WinDockBarExtension)

#endif // WINDOCKBAR_H