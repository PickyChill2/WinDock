#ifndef DESKTOPBACKGROUND_H
#define DESKTOPBACKGROUND_H

#include "CommonDefines.h"
#include <QWidget>
#include <QScreen>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QResizeEvent>
#include <QPainter>

// Forward declarations
class MediaRenderer;
class MultiDesktopManager;
class TopPanel;

class DesktopBackground : public QWidget
{
    Q_OBJECT

public:
    explicit DesktopBackground(QScreen* targetScreen, const ScreenSettings& settings, QWidget* parent = nullptr);
    ~DesktopBackground();

    // Методы для установки параметров фона
    void setBackgroundImage(const QString& imagePath);
    void setBackgroundAlignment(const QString& alignment);
    void setBackgroundScaling(const QString& scaling);
    void setBackgroundScaleFactor(qreal scaleFactor);
    void setVideoLoop(bool loop);
    void setVideoSound(bool enable);

    // Геттеры
    QString getBackgroundImage() const { return m_settings.backgroundImage; }
    QScreen* getTargetScreen() const { return m_targetScreen; }
    ScreenSettings getSettings() const { return m_settings; }
    MediaRenderer* getRenderer() const { return m_renderer; }

    // Статический метод для определения типа файла
    static bool isVideoFile(const QString& filePath);

public slots:
    // Слоты для работы с фоном
    void openBackgroundSettings();
    void transferFocusToTopPanel();
    void openAmdSettings();
    void resetVideoPlayback();
    void startVideoImmediately();

protected:
    // Переопределенные события QWidget
    void paintEvent(QPaintEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    // Приватные слоты для внутреннего использования
    void handleSystemWake();
    void checkVideoStatus();
    void handleApplicationStateChange(Qt::ApplicationState state);

private:
    // Приватные методы
    void createRenderer();
    void cleanupRenderer();
    void createContextMenu();
    void applyRendererSettings();
    void setupTimers(); // Объявление добавлено

    // Члены данных
    QScreen* m_targetScreen;
    ScreenSettings m_settings;
    MediaRenderer* m_renderer;

    // Таймеры
    QTimer* m_quickStartTimer;
    QTimer* m_statusCheckTimer;
    QTimer* m_wakeupTimer;

    // Контекстное меню
    QMenu* m_contextMenu;
    QAction* m_settingsAction;
    QAction* m_amdSettingsAction;
};

#endif // DESKTOPBACKGROUND_H