#ifndef MULTIDESKTOPMANAGER_H
#define MULTIDESKTOPMANAGER_H

#include "CommonDefines.h"
#include <QObject>
#include <QMap>
#include <QList>
#include <QScreen>
#include <QSettings>

// Forward declarations
class TopPanel;
class BackgroundSettingsDialog;
class DesktopBackground;

class MultiDesktopManager : public QObject
{
    Q_OBJECT

public:
    // Singleton pattern
    static MultiDesktopManager& instance();

    // Запрещаем копирование
    MultiDesktopManager(const MultiDesktopManager&) = delete;
    MultiDesktopManager& operator=(const MultiDesktopManager&) = delete;

    // Основные методы инициализации и управления
    void initialize();
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }
    void updateDesktopState();

    // Методы для работы с настройками фона
    void setBackgroundForScreen(QScreen* screen, const ScreenSettings& settings);
    void setBackgroundForAllScreens(const ScreenSettings& settings);
    ScreenSettings getBackgroundForScreen(QScreen* screen) const;

    // Методы для настройки параметров фона
    void setBackgroundAlignment(QScreen* screen, const QString& alignment);
    void setBackgroundScaling(QScreen* screen, const QString& scaling);
    void setBackgroundScaleFactor(QScreen* screen, qreal scaleFactor);
    void setVideoLoop(QScreen* screen, bool loop);
    void setVideoSound(QScreen* screen, bool enable);

    // Методы для работы с диалогами
    void showBackgroundSettingsDialog(QScreen* screen = nullptr);
    void transferFocusToTopPanel();

    // Методы для работы с TopPanel
    void setPrimaryTopPanel(TopPanel* panel);
    TopPanel* getPrimaryTopPanel() const;

    // Геттеры для списка фонов
    QList<DesktopBackground*> getDesktopBackgrounds() const { return m_desktopBackgrounds; }

    // Геттеры для настроек видео
    bool useHardwareAcceleration() const { return m_useHardwareAcceleration; }
    int videoDecodingThreads() const { return m_videoDecodingThreads; }
    bool limitVideoFps() const { return m_limitVideoFps; }
    int targetFps() const { return m_targetFps; }
    bool useVideoCache() const { return m_useVideoCache; }
    int videoCacheSize() const { return m_videoCacheSize; }

    // Методы для управления видео
    void resetAllVideoPlayback();
    void quickStartAllVideos();

private:
    // Приватный конструктор для singleton
    explicit MultiDesktopManager(QObject* parent = nullptr);
    ~MultiDesktopManager();

    // Приватные методы управления
    void createDesktopForScreen(QScreen* screen);
    void removeDesktopForScreen(QScreen* screen);
    void loadSettings();
    void saveSettings();
    void optimizeVideoSettings();
    void updateBackgroundForScreen(QScreen* screen);
    void clearAllDesktopBackgrounds();
    void initializeVideoBackgrounds();

private slots:
    // Слоты для обработки событий экранов
    void handleScreenAdded(QScreen* screen);
    void handleScreenRemoved(QScreen* screen);
    void onSettingsDialogClosed();

private:
    // Члены данных
    QList<DesktopBackground*> m_desktopBackgrounds;
    QMap<QString, ScreenSettings> m_screenSettings;
    TopPanel* m_primaryTopPanel;
    BackgroundSettingsDialog* m_settingsDialog;
    bool m_enabled;

    // Оптимизации для видео
    bool m_useHardwareAcceleration;
    int m_videoDecodingThreads;
    bool m_limitVideoFps;
    int m_targetFps;
    bool m_useVideoCache;
    int m_videoCacheSize;
};

#endif // MULTIDESKTOPMANAGER_H