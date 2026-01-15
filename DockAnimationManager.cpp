#include "DockAnimationManager.h"
#include "Dock.h"
#include "DockConstants.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

DockAnimationManager::DockAnimationManager(Dock* dock)
    : QObject(dock)
    , m_dock(dock)
    , m_isHidden(false)
{
    m_hideAnimation = new QPropertyAnimation(m_dock, "windowOpacity", this);
    m_hideAnimation->setDuration(300);
    m_hideAnimation->setStartValue(1.0);
    m_hideAnimation->setEndValue(0.0);

    m_showAnimation = new QPropertyAnimation(m_dock, "windowOpacity", this);
    m_showAnimation->setDuration(300);
    m_showAnimation->setStartValue(0.0);
    m_showAnimation->setEndValue(1.0);

    connect(m_hideAnimation, &QPropertyAnimation::finished, this, &DockAnimationManager::onHideAnimationFinished);
    connect(m_showAnimation, &QPropertyAnimation::finished, this, &DockAnimationManager::onShowAnimationFinished);

    m_autoHideTimer = new QTimer(this);
    connect(m_autoHideTimer, &QTimer::timeout, this, &DockAnimationManager::checkAutoHide);
}

DockAnimationManager::~DockAnimationManager()
{
    stopAnimations();
}

void DockAnimationManager::initialize()
{
    m_autoHideTimer->start(DockConstants::AUTO_HIDE_CHECK_INTERVAL);
}

void DockAnimationManager::startShowAnimation()
{
    if (!shouldShowDock()) {
        return;
    }

    // Если уже виден, ничего не делаем
    if (!m_isHidden && m_dock->windowOpacity() > 0.99) {
        return;
    }

    stopAnimations();

    // Устанавливаем начальное значение для плавности
    if (m_isHidden || m_dock->windowOpacity() < 0.1) {
        m_showAnimation->setStartValue(0.0);
    } else {
        m_showAnimation->setStartValue(m_dock->windowOpacity());
    }

    m_showAnimation->setEndValue(1.0);
    m_showAnimation->start();
    m_isHidden = false;
}

void DockAnimationManager::startHideAnimation()
{
    // Если уже скрыт, ничего не делаем
    if (m_isHidden && m_dock->windowOpacity() < 0.01) {
        return;
    }

    stopAnimations();

    // Устанавливаем начальное значение для плавности
    if (!m_isHidden || m_dock->windowOpacity() > 0.9) {
        m_hideAnimation->setStartValue(1.0);
    } else {
        m_hideAnimation->setStartValue(m_dock->windowOpacity());
    }

    m_hideAnimation->setEndValue(0.0);
    m_hideAnimation->start();
    m_isHidden = true;
}

void DockAnimationManager::stopAnimations()
{
    if (m_hideAnimation->state() == QPropertyAnimation::Running) {
        m_hideAnimation->stop();
        // Принудительно устанавливаем конечное значение
        if (m_isHidden) {
            m_dock->setWindowOpacity(0.0);
        }
    }
    if (m_showAnimation->state() == QPropertyAnimation::Running) {
        m_showAnimation->stop();
        // Принудительно устанавливаем конечное значение
        if (!m_isHidden) {
            m_dock->setWindowOpacity(1.0);
        }
    }
}

bool DockAnimationManager::shouldShowDock() const
{
    return !isFullScreenAppActive();
}

void DockAnimationManager::checkAutoHide()
{
    if (!m_dock) {
        return;
    }

    QPoint globalPos = QCursor::pos();
    QScreen* targetScreen = m_dock->getTargetScreen();

    if (targetScreen && targetScreen->geometry().contains(globalPos)) {
        // Используем метод activationZone() для проверки зоны активации
        QRect activationZone = m_dock->activationZone();
        bool inActivationZone = activationZone.contains(globalPos);
        bool overDock = m_dock->geometry().contains(m_dock->mapFromGlobal(globalPos));

        if ((inActivationZone || overDock) && m_isHidden && shouldShowDock()) {
            startShowAnimation();
        }
        else if (!inActivationZone && !overDock && !m_isHidden) {
            startHideAnimation();
        }
    }
    else {
        // Если курсор не на целевом экране, скрываем док
        if (!m_isHidden) {
            startHideAnimation();
        }
    }
}

void DockAnimationManager::onShowAnimationFinished()
{
    // Гарантируем, что док полностью показан
    m_dock->setWindowOpacity(1.0);
    m_isHidden = false;
}

void DockAnimationManager::onHideAnimationFinished()
{
    // Гарантируем, что док полностью скрыт
    m_dock->setWindowOpacity(0.0);
    m_isHidden = true;
}

bool DockAnimationManager::isFullScreenAppActive() const
{
#ifdef Q_OS_WIN
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        return false;
    }

    // Проверяем, является ли окно видимым и не свернутым
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) {
        return false;
    }

    // Получаем стили окна
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

    // Проверяем, является ли окно настоящим полноэкранным приложением
    bool isRealFullScreen = (!(style & WS_CAPTION) && !(style & WS_THICKFRAME));

    // Дополнительная проверка для полноэкранных приложений
    if ((style & WS_POPUP) && !(exStyle & WS_EX_WINDOWEDGE)) {
        isRealFullScreen = true;
    }

    // Если это не настоящее полноэкранное приложение, не скрываем док
    if (!isRealFullScreen) {
        return false;
    }

    // Исключаем системные окна (включая рабочий стол) заранее
    const int bufferSize = 256;
    WCHAR className[bufferSize];
    GetClassName(hwnd, className, bufferSize);
    QString classStr = QString::fromWCharArray(className);

    if (classStr == "Progman" || classStr == "WorkerW" ||
        classStr == "Shell_TrayWnd" || classStr == "Button") {
        return false;
    }

    // Проверяем, покрывает ли окно весь экран
    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);

    QScreen* targetScreen = m_dock->getTargetScreen();
    QRect screenGeometry = targetScreen ? targetScreen->geometry() : QGuiApplication::primaryScreen()->geometry();
    QRect windowGeometry(windowRect.left, windowRect.top,
                         windowRect.right - windowRect.left,
                         windowRect.bottom - windowRect.top);

    // Если окно покрывает весь экран или почти весь экран
    int tolerance = 5;
    bool coversScreen =
        (windowGeometry.left() <= screenGeometry.left() + tolerance) &&
        (windowGeometry.top() <= screenGeometry.top() + tolerance) &&
        (windowGeometry.right() >= screenGeometry.right() - tolerance) &&
        (windowGeometry.bottom() >= screenGeometry.bottom() - tolerance);

    if (coversScreen) {
        // Получаем ID процесса
        DWORD processId;
        GetWindowThreadProcessId(hwnd, &processId);

        // Открываем процесс
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
        if (hProcess) {
            WCHAR exePath[MAX_PATH];
            if (GetModuleFileNameEx(hProcess, nullptr, exePath, MAX_PATH)) {
                QString executableName = QFileInfo(QString::fromWCharArray(exePath)).fileName().toLower();
                CloseHandle(hProcess);

                // Проверяем список полноэкранных приложений
                // Чтобы скрывать док для всех полноэкранных приложений, а не только из списка,
                // возвращаем true независимо от списка. Если нужно использовать список как whitelist,
                // раскомментируйте следующую строку и закомментируйте return true ниже.
                // if (m_dock->getFullScreenApps().contains(executableName)) {
                    return true;
                // }
                // return false; // Если не в списке, не скрываем
            } else {
                CloseHandle(hProcess);
                // Если не удалось получить имя, не скрываем
                return false;
            }
        } else {
            // Если не удалось открыть процесс, не скрываем
            return false;
        }
    }

#endif
    return false;
}