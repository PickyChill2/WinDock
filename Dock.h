#ifndef DOCK_H
#define DOCK_H

#include <QWidget>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QMap>
#include <QTimer>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QAction>
#include <QContextMenuEvent>
#include <QSet>
#include <QPointer>
#include <QMenu>
#include "DockConstants.h"
#include "DockMenuAppManager.h"
#include "HiddenAppsDialog.h"
#include "PinnedAppsDialog.h"
#include "DockContextMenu.h"
#include "ExtensionLayoutManager.h"
#include "ExtensionManager.h"
#include "SettingsSignalBridge.h"
#include "WindowButtonManager.h"

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winuser.h>
#endif

// Forward declarations
class DockAnimationManager;
class WindowPreviewDialog;

class DockItem : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal scale READ scale WRITE setScale)
    Q_PROPERTY(QPointF iconPos READ iconPos WRITE setIconPos)
    QPoint getIconPosition() const;

public:
    explicit DockItem(const QIcon& icon, const QString& name, const QString& executablePath, QWidget* parent = nullptr);
    explicit DockItem(const QIcon& icon, const QString& name, const QString& executablePath, bool isRunningApp, QWidget* parent = nullptr);

    qreal scale() const { return m_scale; }
    void setScale(qreal scale);

    QPointF iconPos() const { return m_iconPos; }
    void setIconPos(const QPointF& pos);

    QString getName() const { return m_name; }
    QString getExecutablePath() const { return m_executablePath; }

    void setRunning(bool running);
    bool isRunning() const { return m_isRunning; }
    bool isRunningApp() const { return m_isRunningApp; }

#ifdef Q_OS_WIN
    void activateWindow(HWND hwnd);
    void showWindowSelectionMenu();
#endif

    void setToolTipText(const QString& tooltip);

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

    bool isExplorer() const;

signals:
    void removeRequested();
    void clicked();
    void hideRequested();

private:
    QLabel* m_iconLabel;
    QString m_name;
    QString m_executablePath;
    qreal m_scale;
    QPointF m_iconPos;
    bool m_isRunning;
    bool m_isRunningApp;
    QPropertyAnimation* m_scaleAnimation;
    QPropertyAnimation* m_posAnimation;
    bool m_isTaskView;
    QString m_toolTipText;
};

class Dock : public QWidget
{
    Q_OBJECT

public:
    explicit Dock(QScreen* targetScreen = nullptr, QWidget* parent = nullptr);
    ~Dock();

    void addApplication(const QIcon& icon, const QString& name, const QString& executablePath = "");
    void addRunningApplication(const QIcon& icon, const QString& name, const QString& executablePath);
    void setupDock();
    void setTargetScreen(QScreen* screen);
    void updateRunningApps();
    QString getMappedExecutable(const QString& processName) const;

    // Public getters for DockAnimationManager
    QScreen* getTargetScreen() const { return m_targetScreen; }
    bool isDockHidden() const { return m_isHidden; }

    // Public method to find executable path
    QString findExecutablePath(const QString& executableName);

    // New getter for WinTabItem
    DockItem* getWinTabItem() const { return m_winTabItem; }

    // Методы для загрузки и сохранения настроек
    void loadDockSettings();
    void saveDockSettings();

    // Новый метод для получения зоны активации
    QRect activationZone() const;

public slots:
    void setDockTransparency(int transparency);
    void setDockBackgroundColor(const QColor& color);
    void setDockBorderColor(const QColor& color);
    void setDockCornerRadius(int radius);
    void setDockIconSize(int size);
    void setDockHeight(int height);

protected:
    void paintEvent(QPaintEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void updateDockPosition();
    void addApplicationDialog();
    void removeApplication(DockItem* item);
    void removeRunningApplication(DockItem* item);
    void hideRunningApplication(DockItem* item);
    void onAppsChanged();
    void checkRunningApplications();
    void executeWinTab();
    void checkMousePosition();
    void showHiddenAppsManager();
    void showContextMenu(const QPoint& pos);
    void showManualProcessDialog();
    void addManualProcess(const QString &processName);
    void hideWindowButtons();
    void showPinnedAppsManager();

private:
    QHBoxLayout* m_layout;
    QList<DockItem*> m_items;
    QList<DockItem*> m_runningItems;
    QWidget* m_dockWidget;
    QGraphicsDropShadowEffect* m_shadowEffect;
    QTimer* m_positionTimer;
    QTimer* m_runningCheckTimer;
    QTimer* m_mouseCheckTimer;
    DockMenuAppManager* m_dockAppManager;
    DockContextMenu* m_contextMenu;
    QAction* m_addAppAction;
    QAction* m_manageHiddenAction;
    QAction* m_managePinnedAction;
    QScreen* m_targetScreen;
    QMap<QString, QString> m_processMapping;
    bool m_isHidden;
    DockItem* m_winTabItem;
    QSet<QString> m_hiddenApps;
    bool m_mouseInActivationZone;
    bool m_updatingRunningApps;


    // Менеджер кнопок окон
    WindowButtonManager* m_buttonManager;

    // Расширения для дока
    QWidget* m_leftExtensionsWidget;
    QHBoxLayout* m_leftExtensionsLayout;
    QWidget* m_rightExtensionsWidget;
    QHBoxLayout* m_rightExtensionsLayout;
    QList<QWidget*> m_backgroundExtensions;

    // Менеджер анимаций
    DockAnimationManager* m_animationManager;

    ExtensionLayoutManager* m_extensionManager;

    // Новые переменные для хранения текущих настроек дока
    int m_currentDockTransparency;
    QColor m_currentDockBackgroundColor;
    QColor m_currentDockBorderColor;
    int m_currentDockCornerRadius;
    int m_currentIconSize;
    int m_currentDockHeight;

    // Ручные процессы для отслеживания
    QSet<QString> m_manualProcesses;

    QString getAppType(const QString& processName) const;
    QString getAppDisplayName(const QString& processName) const;
    void clearRunningApps();
    QIcon getAppIcon(const QString& executablePath);
    bool hasWindows(const QString& executableName);
    void loadProcessMapping();
    void loadManualProcesses();
    void saveManualProcesses();
};

#ifdef Q_OS_WIN
QList<QPair<HWND, QString>> FindAllWindowsByProcess(const QString& processName);
#endif

#endif // DOCK_H