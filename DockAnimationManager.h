#ifndef DOCKANIMATIONMANAGER_H
#define DOCKANIMATIONMANAGER_H

#include <QObject>
#include <QPropertyAnimation>
#include <QTimer>
#include <QFileInfo>
#include <QGuiApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

class Dock;

class DockAnimationManager : public QObject
{
    Q_OBJECT

public:
    explicit DockAnimationManager(Dock* dock);
    ~DockAnimationManager();

    void initialize();
    void startShowAnimation();
    void startHideAnimation();
    void stopAnimations();
    bool shouldShowDock() const;

private slots:
    void checkAutoHide();
    void onShowAnimationFinished();
    void onHideAnimationFinished();

private:
    Dock* m_dock;
    QPropertyAnimation* m_hideAnimation;
    QPropertyAnimation* m_showAnimation;
    QTimer* m_autoHideTimer;
    bool m_isHidden;

    bool isFullScreenAppActive() const;
};

#endif // DOCKANIMATIONMANAGER_H