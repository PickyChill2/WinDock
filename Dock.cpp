#include "Dock.h"
#include "DockConstants.h"
#include "DockContextConstants.h"
#include "DockAnimationManager.h"
#include "WindowPreviewDialog.h"
#include "StartMenu.h"
#include "DockMenuAppManager.h"
#include "ManualProcessDialog.h"
#include "WindowButtonManager.h"

#include <QApplication>
#include <QDebug>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLinearGradient>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWindow>
#include <QPointer>
#include <QClipboard>

#ifdef Q_OS_WIN
#include <psapi.h>
#include <tlhelp32.h>
#include <windows.h>
#include <winuser.h>
#endif

#ifdef Q_OS_WIN
// Глобальная переменная для хранения данных EnumWindows
struct EnumWindowsData {
    QString processName;
    QList<QPair<HWND, QString>> windows;
};

// Callback-функция для EnumWindows
BOOL CALLBACK EnumWindowsProc2(HWND hwnd, LPARAM lParam) {
    EnumWindowsData* data = reinterpret_cast<EnumWindowsData*>(lParam);

    // Проверяем, видимо ли окно
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    // Получаем заголовок окна
    wchar_t windowTitle[256];
    int titleLength = GetWindowTextW(hwnd, windowTitle, 255);

    // Получаем стили окна
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);

    // Пропускаем определенные типы окон
    if (exStyle & WS_EX_TOOLWINDOW) {
        return TRUE; // Пропускаем тултипы и всплывающие окна
    }

    // Пропускаем окна без заголовка (кроме некоторых исключений)
    if (titleLength == 0 && !(style & WS_CHILD)) {
        // Для окон без заголовка проверяем, не являются ли они диалогами
        if (!(style & WS_DLGFRAME)) {
            return TRUE;
        }
    }

    QString title = QString::fromWCharArray(windowTitle);

    // Пропускаем системные окна
    if (title.isEmpty() ||
        title == "Program Manager" ||
        title.startsWith("MSCTFIME UI") ||
        title == "Default IME" ||
        title.contains("OleMainThreadWndName") ||
        title == "Windows Input Experience" ||
        title == "Shell_TrayWnd" ||
        title == "DDE Server Window" ||
        title == "Start" ||
        title == "Application Manager") {
        return TRUE;
    }

    // Получаем ID процесса окна
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);

    // Открываем процесс для получения его имени
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess) {
        wchar_t exePath[MAX_PATH];
        if (GetModuleFileNameExW(hProcess, NULL, exePath, MAX_PATH)) {
            QString processExeName = QFileInfo(QString::fromWCharArray(exePath)).fileName();
            QString fullExePath = QString::fromWCharArray(exePath);

            // Специальная обработка для taskmgr.exe
            if (processExeName.compare("taskmgr.exe", Qt::CaseInsensitive) == 0) {
                // Для диспетчера задач проверяем определенные заголовки
                if (!title.isEmpty() &&
                    title != "Task Manager" &&
                    !title.contains("Диспетчер задач", Qt::CaseInsensitive)) {
                    // Возможно, это одно из скрытых окон диспетчера задач
                    if (title.contains("Task Manager", Qt::CaseInsensitive) ||
                        title.contains("Performance", Qt::CaseInsensitive) ||
                        title.contains("Производительность", Qt::CaseInsensitive)) {
                        // Это окно диспетчера задач
                    } else {
                        CloseHandle(hProcess);
                        return TRUE;
                    }
                }
            }

            // Сравниваем имя процесса с искомым
            if (processExeName.compare(data->processName, Qt::CaseInsensitive) == 0) {
                data->windows.append(qMakePair(hwnd, title));
            }
        }
        CloseHandle(hProcess);
    }

    return TRUE;
}

QPoint DockItem::getIconPosition() const
{
    // Возвращаем позицию иконки в глобальных координатах
    return mapToGlobal(rect().center());
}

// Функция для поиска всех окон по имени процесса
QList<QPair<HWND, QString>> FindAllWindowsByProcess(const QString& processName)
{
    EnumWindowsData data;
    data.processName = processName;

    // Перечисляем все окна
    EnumWindows(EnumWindowsProc2, reinterpret_cast<LPARAM>(&data));

    //qDebug() << "Found" << data.windows.size() << "windows for process:" << processName;
    for (const auto& window : data.windows) {
        //qDebug() << "  -" << window.second;
    }

    return data.windows;
}
#endif

// DockItem implementation
DockItem::DockItem(const QIcon& icon, const QString& name, const QString& executablePath, QWidget* parent)
    : QWidget(parent), m_name(name), m_executablePath(executablePath), m_scale(1.0), m_iconPos(0, 0),
m_isRunning(true),
m_isRunningApp(false),
m_isTaskView(name == "Task View")
{
    setFixedSize(DockConstants::ICON_SIZE + DockConstants::ICON_SPACING,
                DockConstants::ICON_SIZE + DockConstants::ICON_SPACING + DockConstants::RUNNING_DOT_SIZE + DockConstants::RUNNING_DOT_MARGIN_BOTTOM);

    // Для кнопки Task View делаем прозрачный фон
    if (m_isTaskView) {
        setAttribute(Qt::WA_TranslucentBackground);
        setStyleSheet("background: transparent; border: none;");
    }

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(DockConstants::ICON_SIZE, DockConstants::ICON_SIZE);
    m_iconLabel->setAlignment(Qt::AlignCenter);

    // Для кнопки Task View настраиваем прозрачность
    if (m_isTaskView) {
        m_iconLabel->setAttribute(Qt::WA_TranslucentBackground);
        m_iconLabel->setStyleSheet("background: transparent; border: none;");
    }

    // Устанавливаем иконку
    if (!icon.isNull()) {
        m_iconLabel->setPixmap(icon.pixmap(DockConstants::ICON_SIZE, DockConstants::ICON_SIZE));
    } else {
        // Создаем иконку для демонстрации
        QPixmap pixmap(DockConstants::ICON_SIZE, DockConstants::ICON_SIZE);

        if (m_isTaskView) {
            // Для кнопки Task View - прозрачный фон с эмодзи динозавра
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            painter.setFont(QFont("Segoe UI Emoji", 20, QFont::Normal));
            painter.setPen(QColor(255, 255, 255)); // Белый цвет для эмодзи
            painter.drawText(pixmap.rect(), Qt::AlignCenter, "🦖");
        } else {
            // Для обычных приложений - цветные иконки
            QColor iconColor;
            if (name == "File Manager") iconColor = QColor(70, 130, 200);
            else if (name == "Browser") iconColor = QColor(220, 80, 60);
            else if (name == "Mail") iconColor = QColor(50, 180, 100);
            else if (name == "Music") iconColor = QColor(180, 70, 180);
            else if (name == "Settings") iconColor = QColor(230, 170, 50);
            else iconColor = QColor(150, 150, 150);

            pixmap.fill(iconColor);
        }
        m_iconLabel->setPixmap(pixmap);
    }


    m_iconLabel->move(DockConstants::ICON_SPACING / 2, DockConstants::ICON_SPACING / 2);

    // Устанавливаем подсказку по умолчанию
    m_toolTipText = name;
    setToolTip(m_toolTipText);

    // Настройка анимации
    m_scaleAnimation = new QPropertyAnimation(this, "scale");
    m_scaleAnimation->setDuration(DockConstants::HOVER_ANIMATION_DURATION);

    m_posAnimation = new QPropertyAnimation(this, "iconPos");
    m_posAnimation->setDuration(DockConstants::HOVER_ANIMATION_DURATION);
}

DockItem::DockItem(const QIcon& icon, const QString& name, const QString& executablePath, bool isRunningApp, QWidget* parent)
    : QWidget(parent), m_name(name), m_executablePath(executablePath), m_scale(1.0), m_iconPos(0, 0),
      m_isRunning(false), m_isRunningApp(isRunningApp), m_isTaskView(false)
{
    setFixedSize(DockConstants::ICON_SIZE + DockConstants::ICON_SPACING,
                 DockConstants::ICON_SIZE + DockConstants::ICON_SPACING + DockConstants::RUNNING_DOT_SIZE + DockConstants::RUNNING_DOT_MARGIN_BOTTOM);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(DockConstants::ICON_SIZE, DockConstants::ICON_SIZE);
    m_iconLabel->setAlignment(Qt::AlignCenter);

    // Устанавливаем иконку
    if (!icon.isNull()) {
        m_iconLabel->setPixmap(icon.pixmap(DockConstants::ICON_SIZE, DockConstants::ICON_SIZE));
    } else {
        // Создаем стандартную иконку для приложения
        QPixmap pixmap(DockConstants::ICON_SIZE, DockConstants::ICON_SIZE);
        pixmap.fill(QColor(100, 100, 100));
        m_iconLabel->setPixmap(pixmap);
    }

    m_iconLabel->move(DockConstants::ICON_SPACING / 2, DockConstants::ICON_SPACING / 2);

    // Для запущенных приложений устанавливаем подсказку с именем процесса
    if (isRunningApp) {
        QString processName = QFileInfo(executablePath).fileName();
        m_toolTipText = QString("%1\n%2").arg(name).arg(processName);
    } else {
        m_toolTipText = name;
    }
    setToolTip(m_toolTipText);

    // Настройка анимации
    m_scaleAnimation = new QPropertyAnimation(this, "scale");
    m_scaleAnimation->setDuration(DockConstants::HOVER_ANIMATION_DURATION);

    m_posAnimation = new QPropertyAnimation(this, "iconPos");
    m_posAnimation->setDuration(DockConstants::HOVER_ANIMATION_DURATION);
}

void DockItem::setScale(qreal scale)
{
    m_scale = scale;
    m_iconLabel->setFixedSize(DockConstants::ICON_SIZE * scale, DockConstants::ICON_SIZE * scale);
    update();
}

void DockItem::setIconPos(const QPointF& pos)
{
    m_iconPos = pos;
    m_iconLabel->move(pos.toPoint());
}

void DockItem::setRunning(bool running)
{
    if (m_isRunning != running) {
        m_isRunning = running;
        update();
    }
}

void DockItem::setToolTipText(const QString& tooltip)
{
    m_toolTipText = tooltip;
    setToolTip(m_toolTipText);
}

void DockItem::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event)

    m_scaleAnimation->stop();
    m_scaleAnimation->setStartValue(m_scale);
    m_scaleAnimation->setEndValue(DockConstants::HOVER_SCALE_FACTOR);
    m_scaleAnimation->start();

    QPointF newPos = QPointF((DockConstants::ICON_SIZE - DockConstants::ICON_SIZE * DockConstants::HOVER_SCALE_FACTOR) / 2,
                            (DockConstants::ICON_SIZE - DockConstants::ICON_SIZE * DockConstants::HOVER_SCALE_FACTOR) / 2);
    m_posAnimation->stop();
    m_posAnimation->setStartValue(m_iconLabel->pos());
    m_posAnimation->setEndValue(newPos);
    m_posAnimation->start();
}

void DockItem::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)

    m_scaleAnimation->stop();
    m_scaleAnimation->setStartValue(m_scale);
    m_scaleAnimation->setEndValue(1.0);
    m_scaleAnimation->start();

    QPointF originalPos = QPointF(DockConstants::ICON_SPACING / 2, DockConstants::ICON_SPACING / 2);
    m_posAnimation->stop();
    m_posAnimation->setStartValue(m_iconLabel->pos());
    m_posAnimation->setEndValue(originalPos);
    m_posAnimation->start();
}

#ifdef Q_OS_WIN
void DockItem::showWindowSelectionMenu()

{
    QString executableName = QFileInfo(m_executablePath).fileName();
    QString pathToLaunch = m_executablePath;

    // ОСОБАЯ ОБРАБОТКА ДЛЯ ПРОВОДНИКА
    if (isExplorer()) {
        executableName = "explorer.exe";
        pathToLaunch = "explorer.exe";
    }

    // Проверяем маппинг процессов
    Dock* dock = qobject_cast<Dock*>(parent() ? parent()->parent() : nullptr);
    if (dock) {
        QString mappedExecutable = dock->getMappedExecutable(executableName);
        if (!mappedExecutable.isEmpty()) {
            qDebug() << "Found process mapping for window selection:" << executableName << "->" << mappedExecutable;
            executableName = mappedExecutable;
            pathToLaunch = dock->findExecutablePath(mappedExecutable);
            if (pathToLaunch.isEmpty()) {
                pathToLaunch = m_executablePath; // fallback
            }
        }
    }

    qDebug() << "Looking for windows of:" << executableName;

    QList<QPair<HWND, QString>> windows = FindAllWindowsByProcess(executableName);
    qDebug() << "Found windows:" << windows.size();

    if (windows.isEmpty()) {
        // Запускаем новое окно приложения
        // Для проводника используем параметр /e,
        if (isExplorer()) {
            QProcess::startDetached(pathToLaunch, {"/e,"});
        } else {
            QProcess::startDetached(pathToLaunch);
        }
        return;
    }

    // Получаем позицию иконки
    QPoint iconPos = getIconPosition();

    // Создаем и показываем диалог выбора окон, передавая позицию иконки
    WindowPreviewDialog* dialog = new WindowPreviewDialog(windows, iconPos);
    connect(dialog, &WindowPreviewDialog::windowSelected, this, &DockItem::activateWindow);
    connect(dialog, &WindowPreviewDialog::newWindowRequested, [this, pathToLaunch]() {
        // Для проводника используем параметр /e,
        if (isExplorer()) {
            QProcess::startDetached(pathToLaunch, {"/e,"});
        } else {
            QProcess::startDetached(pathToLaunch);
        }
    });

    dialog->exec();
    dialog->deleteLater();
}

void DockItem::activateWindow(HWND hwnd)
{
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }

    // Активируем окно
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    BringWindowToTop(hwnd);

    // Фокусируем окно
    SetFocus(hwnd);

    // Дополнительные действия для гарантии активации
    SwitchToThisWindow(hwnd, TRUE);
}
#endif

void DockItem::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Специальная обработка для кнопки Win+Tab
        if (m_isTaskView) {
            emit clicked();
            return;
        }

        qDebug() << "Clicked on:" << m_name << "Path:" << m_executablePath;
        qDebug() << "Is running:" << m_isRunning << "Is running app:" << m_isRunningApp;

#ifdef Q_OS_WIN
        // Для проводника используем специальную логику
        if (isExplorer()) {
            qDebug() << "Explorer detected, using special logic";

            QString executableName = "explorer.exe";
            QString pathToLaunch = "explorer.exe";

            // Проверяем маппинг процессов
            Dock* dock = qobject_cast<Dock*>(parent() ? parent()->parent() : nullptr);
            if (dock) {
                QString mappedExecutable = dock->getMappedExecutable(executableName);
                if (!mappedExecutable.isEmpty()) {
                    qDebug() << "Found process mapping:" << executableName << "->" << mappedExecutable;
                    executableName = mappedExecutable;
                    pathToLaunch = dock->findExecutablePath(mappedExecutable);
                    if (pathToLaunch.isEmpty()) {
                        pathToLaunch = "explorer.exe"; // fallback
                    }
                }
            }

            // Всегда проверяем существующие окна проводника
            QList<QPair<HWND, QString>> windows = FindAllWindowsByProcess(executableName);

            if (windows.isEmpty()) {
                // Нет окон - запускаем новое окно проводника с параметром /e,
                qDebug() << "No explorer windows found, launching new file manager";
                QProcess::startDetached(pathToLaunch, {"/e,"});
            } else if (windows.size() == 1) {
                // Одно окно - активируем его
                qDebug() << "Single explorer window found, activating it";
                activateWindow(windows.first().first);
            } else {
                // Несколько окон - показываем меню выбора
                qDebug() << "Multiple explorer windows found, showing selection menu";
                showWindowSelectionMenu();
            }
            return;
        }

        // Для запущенных приложений проверяем количество окон
        if (m_isRunning || m_isRunningApp) {
            qDebug() << "Application is running, checking windows...";

            QString executableName = QFileInfo(m_executablePath).fileName();
            QString pathToLaunch = m_executablePath;

            // Проверяем маппинг процессов
            Dock* dock = qobject_cast<Dock*>(parent() ? parent()->parent() : nullptr);
            if (dock) {
                QString mappedExecutable = dock->getMappedExecutable(executableName);
                if (!mappedExecutable.isEmpty()) {
                    qDebug() << "Found process mapping:" << executableName << "->" << mappedExecutable;
                    executableName = mappedExecutable;
                    pathToLaunch = dock->findExecutablePath(mappedExecutable);
                    if (pathToLaunch.isEmpty()) {
                        pathToLaunch = m_executablePath; // fallback
                    }
                }
            }

            QList<QPair<HWND, QString>> windows = FindAllWindowsByProcess(executableName);

            if (windows.isEmpty()) {
                // Нет окон - запускаем приложение
                qDebug() << "No windows found, launching new instance:" << pathToLaunch;
                QProcess::startDetached(pathToLaunch);
            } else if (windows.size() == 1) {
                // Одно окно - активируем его
                qDebug() << "Single window found, activating it";
                activateWindow(windows.first().first);
            } else {
                // Несколько окон - показываем меню выбора
                qDebug() << "Multiple windows found, showing selection menu";
                showWindowSelectionMenu();
            }
            return;
        } else {
            qDebug() << "Application is not running, launching new instance";
        }
#endif

        // Если приложение не запущено, запускаем новое
        bool started = QProcess::startDetached(m_executablePath);

        if (!started) {
            qDebug() << "Failed to launch:" << m_executablePath;
            QMessageBox::warning(nullptr, "Ошибка",
                               QString("Не удалось запустить приложение:\n%1").arg(m_executablePath));
        }
    }
}

void DockItem::contextMenuEvent(QContextMenuEvent* event)
{
    // Для кнопки Task View показываем StartMenu при правом клике
    if (m_isTaskView) {
        // Создаем и показываем StartMenu
        StartMenu *startMenu = new StartMenu();
        startMenu->setAttribute(Qt::WA_DeleteOnClose);
        startMenu->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::WindowStaysOnTopHint);

        // Подключаемся к сигналу уничтожения для отладки
        connect(startMenu, &StartMenu::aboutToDestroy, []() {
            qDebug() << "StartMenu destroyed from Task View context menu";
        });

        startMenu->showAtPosition(mapToGlobal(rect().center()));
        return;
    }

    // Для запущенных приложений показываем меню с опцией скрытия
    if (m_isRunningApp) {
        QMenu menu;
        // Устанавливаем флаги для меню, чтобы оно отображалось поверх
        menu.setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::WindowStaysOnTopHint);

        // Устанавливаем стиль для меню
        menu.setStyleSheet(
            "QMenu {"
            "    background-color: #2D2D2D;"
            "    border: 1px solid #404040;"
            "    border-radius: 8px;"
            "    padding: 4px;"
            "}"
            "QMenu::item {"
            "    background-color: transparent;"
            "    color: white;"
            "    padding: 6px 12px;"
            "    border-radius: 4px;"
            "    margin: 2px;"
            "}"
            "QMenu::item:selected {"
            "    background-color: #404040;"
            "}"
        );

        QAction* hideAction = menu.addAction("Скрыть приложение");

        // Добавляем действие для копирования имени процесса
        QString processName = QFileInfo(m_executablePath).fileName();
        QAction* copyProcessAction = menu.addAction("Скопировать: " + processName);

        // ДОБАВЛЕНО: Заполняем контекстное меню расширениями для запущенных приложений
        ExtensionManager::instance().populateDockContextMenu(&menu,
            DockContextMenuType::RunningAppItem, this);

        // Получаем экран через Dock
        QWidget* dockWidget = this;
        while (dockWidget && !qobject_cast<Dock*>(dockWidget->parentWidget())) {
            dockWidget = dockWidget->parentWidget();
        }

        if (dockWidget && dockWidget->parentWidget()) {
            Dock* dock = qobject_cast<Dock*>(dockWidget->parentWidget());
            if (dock) {
                QScreen* screen = dock->getTargetScreen();
                if (!screen) {
                    screen = QGuiApplication::primaryScreen();
                }

                if (screen) {
                    // Вычисляем позицию меню относительно нижней границы экрана
                    QRect screenGeometry = screen->geometry();
                    int menuHeight = menu.sizeHint().height();
                    int menuY = screenGeometry.bottom() - DockConstants::DOCK_CONTEX_HEIGHT - menuHeight - 5;

                    QPoint globalPos = mapToGlobal(event->pos());
                    QPoint menuPos(globalPos.x(), menuY);

                    QAction* selectedAction = menu.exec(menuPos);
                    if (selectedAction == hideAction) {
                        emit hideRequested();
                    } else if (selectedAction == copyProcessAction) {
                        // Копируем имя процесса в буфер обмена
                        QClipboard* clipboard = QApplication::clipboard();
                        clipboard->setText(processName);
                        qDebug() << "Process name copied to clipboard:" << processName;
                    }
                    // ДОБАВЛЕНО: Обработка действий от расширений происходит автоматически
                    // через сигналы, подключенные в расширениях
                    return;
                }
            }
        }

        // Fallback: если не удалось найти док, показываем в обычной позиции
        QAction* selectedAction = menu.exec(mapToGlobal(event->pos()));
        if (selectedAction == hideAction) {
            emit hideRequested();
        } else if (selectedAction == copyProcessAction) {
            // Копируем имя процесса в буфер обмена
            QClipboard* clipboard = QApplication::clipboard();
            clipboard->setText(processName);
            qDebug() << "Process name copied to clipboard:" << processName;
        }
        // ДОБАВЛЕНО: Обработка действий от расширений
    } else {
        // Для закрепленных приложений убираем опцию удаления
        QMenu menu;
        menu.setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::WindowStaysOnTopHint);
        menu.setStyleSheet(
            "QMenu {"
            "    background-color: #2D2D2D;"
            "    border: 1px solid #404040;"
            "    border-radius: 8px;"
            "    padding: 4px;"
            "}"
            "QMenu::item {"
            "    background-color: transparent;"
            "    color: white;"
            "    padding: 6px 12px;"
            "    border-radius: 4px;"
            "    margin: 2px;"
            "}"
            "QMenu::item:selected {"
            "    background-color: #404040;"
            "}"
        );

        // УБРАНО: QAction* removeAction = menu.addAction("Удалить из дока");

        // ДОБАВЛЕНО: Заполняем контекстное меню расширениями для обычных элементов дока
        ExtensionManager::instance().populateDockContextMenu(&menu,
            DockContextMenuType::DockItem, this);

        // Получаем экран через Dock
        QWidget* dockWidget = this;
        while (dockWidget && !qobject_cast<Dock*>(dockWidget->parentWidget())) {
            dockWidget = dockWidget->parentWidget();
        }

        if (dockWidget && dockWidget->parentWidget()) {
            Dock* dock = qobject_cast<Dock*>(dockWidget->parentWidget());
            if (dock) {
                QScreen* screen = dock->getTargetScreen();
                if (!screen) {
                    screen = QGuiApplication::primaryScreen();
                }

                if (screen) {
                    // Вычисляем позицию меню относительно нижней границы экрана
                    QRect screenGeometry = screen->geometry();
                    int menuHeight = menu.sizeHint().height();
                    int menuY = screenGeometry.bottom() - DockConstants::DOCK_CONTEX_HEIGHT - menuHeight - 5;

                    QPoint globalPos = mapToGlobal(event->pos());
                    QPoint menuPos(globalPos.x(), menuY);

                    menu.exec(menuPos);
                    // УБРАНО: обработка removeAction
                    return;
                }
            }
        }

        // Fallback: обычное позиционирование
        menu.exec(mapToGlobal(event->pos()));
        // УБРАНО: обработка removeAction
    }
}

void DockItem::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    // Для кнопки Task View не рисуем ничего (полностью прозрачная)
    if (m_isTaskView) {
        return;
    }

    // Рисуем белую точку, если приложение запущено
    if (m_isRunning) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(DockConstants::RUNNING_DOT_COLOR);

        int dotX = (width() - DockConstants::RUNNING_DOT_SIZE) / 2;
        int dotY = height() - DockConstants::RUNNING_DOT_SIZE - DockConstants::RUNNING_DOT_MARGIN_BOTTOM;

        painter.drawEllipse(dotX, dotY, DockConstants::RUNNING_DOT_SIZE, DockConstants::RUNNING_DOT_SIZE);
    }
}

bool DockItem::isExplorer() const
{
    QString executableName = QFileInfo(m_executablePath).fileName().toLower();
    return executableName == "explorer.exe";
}

// Dock implementation
Dock::Dock(QScreen* targetScreen, QWidget* parent)
    : QWidget(parent), m_targetScreen(targetScreen), m_isHidden(false), m_winTabItem(nullptr),
      m_mouseInActivationZone(false), m_updatingRunningApps(false)
{
    qDebug() << "Dock constructor started";

    // Инициализация настроек дока
    m_currentDockTransparency = BaseConstants::BACKGROUND_ALPHA;
    m_currentDockBackgroundColor = BaseConstants::PRIMARY_BACKGROUND_COLOR;
    m_currentDockBorderColor = BaseConstants::BORDER_COLOR;
    m_currentDockCornerRadius = DockConstants::DOCK_CORNER_RADIUS;
    m_currentIconSize = DockConstants::ICON_SIZE;
    m_currentDockHeight = DockConstants::DOCK_HEIGHT;

    // Убираем Qt::WindowDoesNotAcceptFocus и используем другие флаги
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground, false);
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    // Устанавливаем политику фокуса
    setFocusPolicy(Qt::NoFocus);

    // Устанавливаем фильтр событий для отслеживания движения мыши
    qApp->installEventFilter(this);

    // Инициализируем менеджер кнопок окон
    m_buttonManager = &WindowButtonManager::instance();
    m_buttonManager->initialize();

    // Инициализируем m_dockAppManager первым делом
    m_dockAppManager = new DockMenuAppManager(this);
    if (!m_dockAppManager) {
        qDebug() << "Error: Failed to create DockMenuAppManager";
        return;
    }

    loadProcessMapping();
    loadManualProcesses(); // Загружаем ручные процессы

    connect(m_dockAppManager, &DockMenuAppManager::pinnedAppsChanged, this, &Dock::onAppsChanged);

    // Инициализируем остальные компоненты
    m_dockWidget = new QWidget(this);
    if (!m_dockWidget) {
        qDebug() << "Error: Failed to create dock widget";
        return;
    }

    // СОЗДАЕМ ОСНОВНОЙ LAYOUT ДОКА С РАСШИРЕНИЯМИ
    QHBoxLayout* mainDockLayout = new QHBoxLayout(m_dockWidget);
    mainDockLayout->setContentsMargins(0, 0, 0, 0);
    mainDockLayout->setSpacing(0);

    // СОЗДАЕМ ВИДЖЕТЫ ДЛЯ РАСШИРЕНИЙ
    // Левая часть - расширения
    m_leftExtensionsWidget = new QWidget(m_dockWidget);
    m_leftExtensionsWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_leftExtensionsLayout = new QHBoxLayout(m_leftExtensionsWidget);
    m_leftExtensionsLayout->setContentsMargins(10, 0, 5, 0);
    m_leftExtensionsLayout->setSpacing(5);
    m_leftExtensionsLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // Правая часть - расширения
    m_rightExtensionsWidget = new QWidget(m_dockWidget);
    m_rightExtensionsWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_rightExtensionsLayout = new QHBoxLayout(m_rightExtensionsWidget);
    m_rightExtensionsLayout->setContentsMargins(5, 0, 10, 0);
    m_rightExtensionsLayout->setSpacing(5);
    m_rightExtensionsLayout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Основной layout для иконок приложений
    m_layout = new QHBoxLayout();
    m_layout->setSpacing(DockConstants::ICON_SPACING);
    m_layout->setAlignment(Qt::AlignCenter);
    m_layout->setContentsMargins(10, 10, 10, 10);

    // СОБИРАЕМ ОСНОВНОЙ LAYOUT
    mainDockLayout->addWidget(m_leftExtensionsWidget);
    mainDockLayout->addLayout(m_layout);
    mainDockLayout->addWidget(m_rightExtensionsWidget);

    // Убедитесь, что фон установлен
    m_dockWidget->setStyleSheet("background: transparent;");

    // СОЗДАЕМ МЕНЕДЖЕР РАСШИРЕНИЙ
    m_extensionManager = new ExtensionLayoutManager(this);
    qDebug() << "ExtensionLayoutManager created:" << (m_extensionManager != nullptr);

    // ИНИЦИАЛИЗИРУЕМ МЕНЕДЖЕР РАСШИРЕНИЙ ДЛЯ DOCK
    if (m_extensionManager) {
        qDebug() << "Initializing ExtensionLayoutManager for Dock...";
        qDebug() << "Dock widget:" << m_dockWidget;
        qDebug() << "Left layout:" << m_leftExtensionsLayout;
        qDebug() << "Right layout:" << m_rightExtensionsLayout;
        qDebug() << "Icon size:" << m_currentIconSize;

        m_extensionManager->initializeDock(m_dockWidget,
                                         m_leftExtensionsLayout,
                                         m_rightExtensionsLayout,
                                         m_currentIconSize);
        qDebug() << "ExtensionLayoutManager initialized successfully";
    } else {
        qDebug() << "Error: Failed to create ExtensionLayoutManager";
    }

    // Load background extensions (with displayLocation = None)
    ExtensionManager& em = ExtensionManager::instance();
    QList<QString> allExtensions = em.extensionNames();
    for (const QString& extensionName : allExtensions) {
        ExtensionInterface* extension = em.getExtensionInstance(extensionName);
        if (extension && extension->displayLocation() == ExtensionDisplayLocation::None) {
            QWidget* widget = extension->createWidget(this);
            if (widget) {
                m_backgroundExtensions.append(widget);
                qDebug() << "Created background extension:" << extensionName;
            }
        }
    }

    // Настройка тени
    m_shadowEffect = new QGraphicsDropShadowEffect(this);
    m_shadowEffect->setBlurRadius(BaseConstants::BLUR_RADIUS);
    m_shadowEffect->setColor(QColor(0, 0, 0, 60));
    m_shadowEffect->setOffset(0, 3);
    m_dockWidget->setGraphicsEffect(m_shadowEffect);

    // Контекстное меню - используем новый класс
    m_contextMenu = new DockContextMenu(this);

    // Добавляем действие для управления закрепленными приложениями
    m_managePinnedAction = new QAction("Управление закрепленными", this);
    connect(m_managePinnedAction, &QAction::triggered, this, &Dock::showPinnedAppsManager);
    m_contextMenu->addAction(m_managePinnedAction);

    // Добавляем разделитель
    m_contextMenu->addSeparator();

    // Добавляем действие для управления скрытыми приложениями
    m_manageHiddenAction = new QAction("Управление скрытыми", this);
    connect(m_manageHiddenAction, &QAction::triggered, this, &Dock::showHiddenAppsManager);
    m_contextMenu->addAction(m_manageHiddenAction);

    // Таймер для обновления позиции дока
    m_positionTimer = new QTimer(this);
    connect(m_positionTimer, &QTimer::timeout, this, &Dock::updateDockPosition);
    m_positionTimer->start(1000);

    // Таймер для проверки запущенных приложений
    m_runningCheckTimer = new QTimer(this);
    connect(m_runningCheckTimer, &QTimer::timeout, this, &Dock::checkRunningApplications);
    m_runningCheckTimer->start(2000);

    // Таймер для проверки позиции мыши
    m_mouseCheckTimer = new QTimer(this);
    connect(m_mouseCheckTimer, &QTimer::timeout, this, &Dock::checkMousePosition);
    m_mouseCheckTimer->start(100);

    // Таймер для скрытия кнопок окон
    QTimer* buttonHideTimer = new QTimer(this);
    connect(buttonHideTimer, &QTimer::timeout, this, &Dock::hideWindowButtons);
    buttonHideTimer->start(100); // Проверяем каждые 100ms для лучшей реакции

    // Создаем менеджер анимаций
    m_animationManager = new DockAnimationManager(this);
    if (m_animationManager) {
        m_animationManager->initialize();
    }

    // Подключаемся к сигнальному мосту для настроек
    SettingsSignalBridge* bridge = SettingsSignalBridge::instance();

    connect(bridge, &SettingsSignalBridge::dockTransparencyChanged,
            this, &Dock::setDockTransparency);

    connect(bridge, &SettingsSignalBridge::dockBackgroundColorChanged,
            this, &Dock::setDockBackgroundColor);

    connect(bridge, &SettingsSignalBridge::dockBorderColorChanged,
            this, &Dock::setDockBorderColor);

    connect(bridge, &SettingsSignalBridge::dockCornerRadiusChanged,
            this, &Dock::setDockCornerRadius);

    connect(bridge, &SettingsSignalBridge::dockIconSizeChanged,
            this, &Dock::setDockIconSize);

    connect(bridge, &SettingsSignalBridge::dockHeightChanged,
            this, &Dock::setDockHeight);

    // Загружаем сохраненные настройки
    loadDockSettings();

    setupDock();
    updateDockPosition();

    // ЗАМЕЧАНИЕ: Загрузка расширений теперь выполняется автоматически
    // в initializeDock через ExtensionLayoutManager

    // Сразу скрываем док, если активно полноэкранное приложение
    if (m_animationManager && !m_animationManager->shouldShowDock()) {
        setWindowOpacity(0.0);
        m_isHidden = true;
    }

    qDebug() << "Dock constructor completed successfully";
}

Dock::~Dock()
{
    qDebug() << "Dock destructor called";

    // Останавливаем все таймеры
    if (m_positionTimer) {
        m_positionTimer->stop();
        delete m_positionTimer;
        m_positionTimer = nullptr;
    }
    if (m_runningCheckTimer) {
        m_runningCheckTimer->stop();
        delete m_runningCheckTimer;
        m_runningCheckTimer = nullptr;
    }
    if (m_mouseCheckTimer) {
        m_mouseCheckTimer->stop();
        delete m_mouseCheckTimer;
        m_mouseCheckTimer = nullptr;
    }

    // Безопасно очищаем списки
    for (DockItem* item : m_items) {
        if (item) {
            item->disconnect();
            delete item;
        }
    }
    m_items.clear();

    for (DockItem* item : m_runningItems) {
        if (item) {
            item->disconnect();
            delete item;
        }
    }
    m_runningItems.clear();

    // Осторожно удаляем менеджер анимаций
    if (m_animationManager) {
        delete m_animationManager;
        m_animationManager = nullptr;
    }

    // ExtensionLayoutManager будет автоматически удален как дочерний объект
    // благодаря установке родителя в конструкторе

    qDebug() << "Dock destroyed";
}

void Dock::hideWindowButtons()
{
#ifdef Q_OS_WIN
    if (m_buttonManager) {
        m_buttonManager->hideWindowButtons();
    }
#endif
}

void Dock::loadDockSettings()
{
    QSettings settings("MyCompany", "DockApp");

    m_currentDockTransparency = settings.value("Dock/Transparency", 200).toInt();
    m_currentDockBackgroundColor = settings.value("Dock/BackgroundColor", QColor(40, 40, 40, 200)).value<QColor>();
    m_currentDockBorderColor = settings.value("Dock/BorderColor", QColor(80, 80, 80, 255)).value<QColor>();
    m_currentDockCornerRadius = settings.value("Dock/CornerRadius", 15).toInt();
    m_currentIconSize = settings.value("Dock/IconSize", 48).toInt();
    m_currentDockHeight = settings.value("Dock/Height", 70).toInt();

    // Применяем настройки
    update();
    updateDockPosition();
}

void Dock::saveDockSettings()
{
    QSettings settings("MyCompany", "DockApp");

    settings.setValue("Dock/Transparency", m_currentDockTransparency);
    settings.setValue("Dock/BackgroundColor", m_currentDockBackgroundColor);
    settings.setValue("Dock/BorderColor", m_currentDockBorderColor);
    settings.setValue("Dock/CornerRadius", m_currentDockCornerRadius);
    settings.setValue("Dock/IconSize", m_currentIconSize);
    settings.setValue("Dock/Height", m_currentDockHeight);
}

void Dock::loadManualProcesses()
{
    QSettings settings("MyCompany", "DockApp");
    QStringList manualProcesses = settings.value("ManualProcesses").toStringList();
    m_manualProcesses = QSet<QString>(manualProcesses.begin(), manualProcesses.end());

    qDebug() << "Loaded manual processes:" << m_manualProcesses.size();
    for (const QString& process : m_manualProcesses) {
        qDebug() << "  -" << process;
    }
}

void Dock::saveManualProcesses()
{
    QSettings settings("MyCompany", "DockApp");
    settings.setValue("ManualProcesses", QStringList(m_manualProcesses.values()));

    qDebug() << "Saved manual processes:" << m_manualProcesses.size();
}

void Dock::showManualProcessDialog()
{
    ManualProcessDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString processName = dialog.getProcessName();
        addManualProcess(processName);
    }
}

void Dock::addManualProcess(const QString &processName)
{
    if (processName.isEmpty()) return;

    QString normalizedName = processName.toLower();

    if (m_manualProcesses.contains(normalizedName)) {
        QMessageBox::information(this, "Информация",
                               QString("Процесс '%1' уже добавлен для отслеживания").arg(processName));
        return;
    }

    m_manualProcesses.insert(normalizedName);
    saveManualProcesses();

    qDebug() << "Added manual process:" << processName;

    // Немедленно проверяем наличие окон для этого процесса
    updateRunningApps();

    QMessageBox::information(this, "Успех",
                           QString("Процесс '%1' добавлен для отслеживания.\n"
                                  "Иконка появится в доке когда процесс будет запущен.").arg(processName));
}

// Новые слоты для настройки дока
void Dock::setDockTransparency(int transparency)
{
    m_currentDockTransparency = transparency;
    // Обновляем альфа-канал цвета фона
    m_currentDockBackgroundColor.setAlpha(transparency);
    update();
    saveDockSettings();
}

void Dock::setDockBackgroundColor(const QColor& color)
{
    m_currentDockBackgroundColor = color;
    m_currentDockBackgroundColor.setAlpha(m_currentDockTransparency);
    update();
    saveDockSettings();
}

void Dock::setDockBorderColor(const QColor& color)
{
    m_currentDockBorderColor = color;
    update();
    saveDockSettings();
}

void Dock::setDockCornerRadius(int radius)
{
    m_currentDockCornerRadius = radius;
    update();
    saveDockSettings();
}

void Dock::setDockIconSize(int size)
{
    m_currentIconSize = size;

    // Обновить размеры всех иконок
    for (DockItem* item : m_items) {
        if (item) {
            item->setFixedSize(m_currentIconSize + DockConstants::ICON_SPACING,
                              m_currentIconSize + DockConstants::ICON_SPACING +
                              DockConstants::RUNNING_DOT_SIZE + DockConstants::RUNNING_DOT_MARGIN_BOTTOM);

            // Обновляем размер иконки внутри элемента
            QLabel* iconLabel = item->findChild<QLabel*>();
            if (iconLabel) {
                iconLabel->setFixedSize(m_currentIconSize, m_currentIconSize);

                // Обновляем пиксмап иконки
                QIcon icon = item->property("originalIcon").value<QIcon>();
                if (icon.isNull()) {
                    // Если иконка не сохранена, пытаемся получить ее из текущего пиксмапа
                    QPixmap currentPixmap = iconLabel->pixmap(Qt::ReturnByValue);
                    if (!currentPixmap.isNull()) {
                        icon = QIcon(currentPixmap);
                        item->setProperty("originalIcon", QVariant::fromValue(icon));
                    }
                }

                if (!icon.isNull()) {
                    iconLabel->setPixmap(icon.pixmap(m_currentIconSize, m_currentIconSize));
                }
            }
        }
    }

    for (DockItem* item : m_runningItems) {
        if (item) {
            item->setFixedSize(m_currentIconSize + DockConstants::ICON_SPACING,
                              m_currentIconSize + DockConstants::ICON_SPACING +
                              DockConstants::RUNNING_DOT_SIZE + DockConstants::RUNNING_DOT_MARGIN_BOTTOM);

            // Обновляем размер иконки внутри элемента
            QLabel* iconLabel = item->findChild<QLabel*>();
            if (iconLabel) {
                iconLabel->setFixedSize(m_currentIconSize, m_currentIconSize);

                // Обновляем пиксмап иконки
                QIcon icon = item->property("originalIcon").value<QIcon>();
                if (icon.isNull()) {
                    QPixmap currentPixmap = iconLabel->pixmap(Qt::ReturnByValue);
                    if (!currentPixmap.isNull()) {
                        icon = QIcon(currentPixmap);
                        item->setProperty("originalIcon", QVariant::fromValue(icon));
                    }
                }

                if (!icon.isNull()) {
                    iconLabel->setPixmap(icon.pixmap(m_currentIconSize, m_currentIconSize));
                }
            }
        }
    }

    updateDockPosition();
    saveDockSettings();
}

void Dock::setDockHeight(int height)
{
    m_currentDockHeight = height;
    updateDockPosition();
    saveDockSettings();
}

void Dock::setTargetScreen(QScreen* screen)
{
    m_targetScreen = screen;
    updateDockPosition();
}

void Dock::addApplication(const QIcon& icon, const QString& name, const QString& executablePath)
{
    DockItem* item = new DockItem(icon, name, executablePath, m_dockWidget);

    // Сохраняем оригинальную иконку для возможного изменения размера
    item->setProperty("originalIcon", QVariant::fromValue(icon));

    connect(item, &DockItem::removeRequested, this, [this, item]() {
        removeApplication(item);
    });

    // Для кнопки Win+Tab подключаем специальный слот
    if (name == "Task View") {
        connect(item, &DockItem::clicked, this, &Dock::executeWinTab);
        m_winTabItem = item;
    }

    m_layout->addWidget(item);
    m_items.append(item);
}

void Dock::loadProcessMapping()
{
    m_processMapping.clear();

    QFile file("process_mapping.txt");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Cannot open process_mapping.txt file";
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        // Пропускаем комментарии и пустые строки
        if (line.startsWith('#') || line.isEmpty()) {
            continue;
        }

        // Разбираем строку формата: process_name=main_executable.exe
        QStringList parts = line.split('=');
        if (parts.size() == 2) {
            QString processName = parts[0].trimmed().toLower();
            QString mainExecutable = parts[1].trimmed();
            m_processMapping[processName] = mainExecutable;
            qDebug() << "Loaded process mapping:" << processName << "->" << mainExecutable;
        }
    }

    file.close();
    qDebug() << "Process mapping loaded, total entries:" << m_processMapping.size();
}

QString Dock::getMappedExecutable(const QString& processName) const
{
    return m_processMapping.value(processName.toLower(), "");
}

void Dock::showContextMenu(const QPoint& pos)
{
    QMenu contextMenu;

    contextMenu.setStyleSheet(
        "QMenu {"
        "    background-color: #2D2D2D;"
        "    border: 1px solid #404040;"
        "    border-radius: 8px;"
        "    padding: 4px;"
        "}"
        "QMenu::item {"
        "    background-color: transparent;"
        "    color: white;"
        "    padding: 6px 12px;"
        "    border-radius: 4px;"
        "    margin: 2px;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #404040;"
        "}"
    );

    // Добавляем действие для управления закрепленными приложениями
    QAction* managePinnedAction = new QAction("Управление закрепленными", &contextMenu);
    connect(managePinnedAction, &QAction::triggered, this, &Dock::showPinnedAppsManager);
    contextMenu.addAction(managePinnedAction);

    // Добавляем действие для ручного добавления процесса
    QAction* addManualProcessAction = new QAction("Добавить процесс для отслеживания", &contextMenu);
    connect(addManualProcessAction, &QAction::triggered, this, &Dock::showManualProcessDialog);
    contextMenu.addAction(addManualProcessAction);

    contextMenu.addSeparator();

    // Добавляем действие для управления скрытыми приложениями
    QAction* manageHiddenAction = new QAction("Управление скрытыми", &contextMenu);
    connect(manageHiddenAction, &QAction::triggered, this, &Dock::showHiddenAppsManager);
    contextMenu.addAction(manageHiddenAction);

    // Получаем текущий экран
    QScreen* screen = m_targetScreen ? m_targetScreen : QGuiApplication::primaryScreen();
    if (!screen) {
        contextMenu.exec(mapToGlobal(pos));
        return;
    }

    // Вычисляем позицию меню относительно нижней границы экрана
    QRect screenGeometry = screen->geometry();
    int menuHeight = contextMenu.sizeHint().height();
    int menuY = screenGeometry.bottom() - DockConstants::DOCK_CONTEX_HEIGHT - menuHeight - 5; // 5px отступ

    // Горизонтальная позиция остается от мыши
    QPoint globalPos = mapToGlobal(pos);
    QPoint menuPos(globalPos.x(), menuY);

    contextMenu.exec(menuPos);
}

void Dock::showHiddenAppsManager()
{
    if (!m_dockAppManager) {
        qDebug() << "Error: Cannot show hidden apps manager - m_dockAppManager is null";
        QMessageBox::warning(this, "Ошибка", "Менеджер приложений не инициализирован.");
        return;
    }

    HiddenAppsDialog* dialog = new HiddenAppsDialog(m_dockAppManager, this);
    if (!dialog) {
        qDebug() << "Error: Failed to create HiddenAppsDialog";
        return;
    }

    dialog->setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    // Позиционируем диалог относительно ВЕРХНЕЙ границы экрана
    QScreen* screen = m_targetScreen ? m_targetScreen : QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();

        // Центрируем диалог по горизонтали
        int dialogX = screenGeometry.left() + (screenGeometry.width() - dialog->width()) / 2;

        // Вычисляем вертикальную позицию относительно ВЕРХНЕЙ границы экрана
        // Диалог показываем над предполагаемой позицией дока
        int dialogY = screenGeometry.bottom() - DockConstants::DOCK_CONTEX_HEIGHT - 50; // 50px над местом где должен быть док

        // Проверяем, что диалог не выходит за верхний край экрана
        if (dialogY < screenGeometry.top()) {
            dialogY = screenGeometry.top() + 50; // Отступ от верхнего края
        }

        dialog->move(dialogX, dialogY);
    } else {
        // Fallback: центрируем относительно родителя
        QPoint center = mapToGlobal(rect().center());
        dialog->move(center.x() - dialog->width() / 2, center.y() - dialog->height() / 2);
    }

    dialog->exec();
}

void Dock::showPinnedAppsManager()
{
    if (!m_dockAppManager) {
        qDebug() << "Error: Cannot show pinned apps manager - m_dockAppManager is null";
        QMessageBox::warning(this, "Ошибка", "Менеджер приложений не инициализирован.");
        return;
    }

    PinnedAppsDialog* dialog = new PinnedAppsDialog(m_dockAppManager, this);
    if (!dialog) {
        qDebug() << "Error: Failed to create PinnedAppsDialog";
        return;
    }

    dialog->setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    // Позиционируем диалог относительно ВЕРХНЕЙ границы экрана
    QScreen* screen = m_targetScreen ? m_targetScreen : QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();

        // Центрируем диалог по горизонтали
        int dialogX = screenGeometry.left() + (screenGeometry.width() - dialog->width()) / 2;

        // Вычисляем вертикальную позицию относительно ВЕРХНЕЙ границы экрана
        // Диалог показываем над предполагаемой позицией дока
        int dialogY = screenGeometry.bottom() - DockConstants::DOCK_CONTEX_HEIGHT - 50; // 50px над местом где должен быть док

        // Проверяем, что диалог не выходит за верхний край экрана
        if (dialogY < screenGeometry.top()) {
            dialogY = screenGeometry.top() + 50; // Отступ от верхнего края
        }

        dialog->move(dialogX, dialogY);
    } else {
        // Fallback: центрируем относительно родителя
        QPoint center = mapToGlobal(rect().center());
        dialog->move(center.x() - dialog->width() / 2, center.y() - dialog->height() / 2);
    }

    dialog->exec();
}

void Dock::addRunningApplication(const QIcon& icon, const QString& name, const QString& executablePath)
{
    // Проверяем, нет ли уже такого приложения в запущенных
    for (DockItem* item : m_runningItems) {
        if (item->getExecutablePath() == executablePath) {
            return;
        }
    }

    DockItem* item = new DockItem(icon, name, executablePath, true, m_dockWidget);

    // Сохраняем оригинальную иконку
    item->setProperty("originalIcon", QVariant::fromValue(icon));

    // Обновляем подсказку с именем процесса
    QString processName = QFileInfo(executablePath).fileName();
    QString tooltipText = QString("%1\n%2").arg(name).arg(processName);
    item->setToolTipText(tooltipText);

    connect(item, &DockItem::removeRequested, this, [this, item]() {
        removeRunningApplication(item);
    });
    connect(item, &DockItem::hideRequested, this, [this, item]() {
        hideRunningApplication(item);
    });

    m_layout->addWidget(item);
    m_runningItems.append(item);
}

void Dock::setupDock()
{
    // Очищаем текущие элементы
    for (DockItem* item : m_items) {
        m_layout->removeWidget(item);
        delete item;
    }
    m_items.clear();

    // Очищаем запущенные приложения (они будут добавлены заново в checkRunningApplications)
    clearRunningApps();

    // Загружаем приложения из DockMenuAppManager
    QList<DockAppInfo> apps = m_dockAppManager->getPinnedApps();

    for (const DockAppInfo& app : apps) {
        addApplication(app.icon, app.name, app.executablePath);
    }

    // УБРАН РАЗДЕЛИТЕЛЬ

    // Добавляем растягивающийся спейсер перед кнопкой Task View
    m_layout->addStretch();

    // Добавляем кнопку Win+Tab в конец (справа)
    addApplication(QIcon(), "Task View", "");

    // Обновляем позицию после добавления иконок
    QTimer::singleShot(100, this, &Dock::updateDockPosition);

    // Обновляем список запущенных приложений
    updateRunningApps();
}

void Dock::clearRunningApps()
{
    qDebug() << "Clearing running apps...";

    // Останавливаем таймер на время очистки
    m_runningCheckTimer->stop();

    for (DockItem* item : m_runningItems) {
        if (item) {
            // Отключаем все сигналы
            item->disconnect();

            // Удаляем из layout
            m_layout->removeWidget(item);

            // Удаляем объект
            delete item;
        }
    }
    m_runningItems.clear();

    // Перезапускаем таймер
    m_runningCheckTimer->start();

    qDebug() << "Running apps cleared";
}

QString Dock::getAppType(const QString& processName) const
{
    QString lowerName = processName.toLower();

    if (lowerName == "taskmgr.exe") return "Диспетчер задач";
    if (lowerName == "cmd.exe") return "Командная строка";
    if (lowerName == "powershell.exe") return "PowerShell";
    if (lowerName == "explorer.exe") return "Проводник Windows";
    if (lowerName == "msedge.exe") return "Браузер Edge";
    if (lowerName == "chrome.exe") return "Браузер Chrome";
    if (lowerName == "firefox.exe") return "Браузер Firefox";
    if (lowerName == "notepad.exe") return "Блокнот";
    if (lowerName == "calc.exe") return "Калькулятор";
    if (lowerName == "mspaint.exe") return "Paint";
    if (lowerName == "winword.exe") return "Microsoft Word";
    if (lowerName == "excel.exe") return "Microsoft Excel";
    if (lowerName == "powerpnt.exe") return "Microsoft PowerPoint";
    if (lowerName == "outlook.exe") return "Microsoft Outlook";

    return "Приложение";
}

QString Dock::getAppDisplayName(const QString& processName) const
{
    QString lowerName = processName.toLower();

    if (lowerName == "taskmgr.exe") return "Диспетчер задач";
    if (lowerName == "cmd.exe") return "Командная строка";
    if (lowerName == "powershell.exe") return "Windows PowerShell";
    if (lowerName == "explorer.exe") return "Проводник";
    if (lowerName == "msedge.exe") return "Microsoft Edge";
    if (lowerName == "chrome.exe") return "Google Chrome";
    if (lowerName == "firefox.exe") return "Mozilla Firefox";
    if (lowerName == "notepad.exe") return "Блокнот";
    if (lowerName == "calc.exe") return "Калькулятор";
    if (lowerName == "mspaint.exe") return "Paint";

    // Для других приложений убираем расширение и капитализируем первую букву
    QString baseName = QFileInfo(processName).baseName();
    if (!baseName.isEmpty()) {
        baseName[0] = baseName[0].toUpper();
    }
    return baseName;
}

void Dock::updateRunningApps()
{
    if (m_updatingRunningApps) {
        qDebug() << "updateRunningApps already in progress, skipping";
        return;
    }

    // Проверяем необходимые указатели
    if (!m_dockAppManager || !m_layout || !m_dockWidget) {
        qDebug() << "Error: Required pointers are null in updateRunningApps";
        return;
    }

    m_updatingRunningApps = true;
    //qDebug() << "Starting updateRunningApps...";

    // Временно останавливаем таймер
    if (m_runningCheckTimer) {
        m_runningCheckTimer->stop();
    }

    // Создаем временный список для новых запущенных приложений
    QList<DockItem*> newRunningItems;

#ifdef Q_OS_WIN
    // Получаем список всех процессов
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        qDebug() << "Failed to create process snapshot";
        if (m_runningCheckTimer) m_runningCheckTimer->start();
        m_updatingRunningApps = false;
        return;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        qDebug() << "Failed to get first process";
        CloseHandle(hProcessSnap);
        if (m_runningCheckTimer) m_runningCheckTimer->start();
        m_updatingRunningApps = false;
        return;
    }

    // Собираем имена запущенных процессов, которые имеют окна
    QSet<QString> runningProcessesWithWindows;
    do {
        QString processName = QString::fromWCharArray(pe32.szExeFile);
        if (hasWindows(processName)) {
            runningProcessesWithWindows.insert(processName.toLower());
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);

    // Добавляем запущенные приложения, которые не являются закрепленными и не скрыты
    for (const QString& processName : runningProcessesWithWindows) {
        // Проверяем, не является ли приложение уже закрепленным
        bool isPinned = false;
        for (DockItem* item : m_items) {
            if (!item) continue;

            QString pinnedExeName = QFileInfo(item->getExecutablePath()).fileName().toLower();
            if (pinnedExeName == processName) {
                isPinned = true;
                break;
            }
        }

        // Проверяем, не скрыто ли приложение через DockMenuAppManager
        bool isHidden = m_dockAppManager->isAppHidden(processName);

        // Проверяем, не отображается ли уже это приложение
        bool alreadyDisplayed = false;
        for (DockItem* existingItem : m_runningItems) {
            if (!existingItem) continue;

            QString existingExeName = QFileInfo(existingItem->getExecutablePath()).fileName().toLower();
            if (existingExeName == processName) {
                alreadyDisplayed = true;
                // Сохраняем существующий элемент
                newRunningItems.append(existingItem);
                break;
            }
        }

        if (!isPinned && !isHidden && !alreadyDisplayed) {
            QString fullPath = findExecutablePath(processName);
            if (!fullPath.isEmpty()) {
                QIcon icon = getAppIcon(fullPath);
                QString appName = QFileInfo(processName).baseName();

                // Создаем новый элемент
                DockItem* item = new DockItem(icon, appName, fullPath, true, m_dockWidget);
                if (!item) {
                    qDebug() << "Failed to create DockItem for:" << appName;
                    continue;
                }

                // Сохраняем оригинальную иконку
                item->setProperty("originalIcon", QVariant::fromValue(icon));

                // Устанавливаем подробную подсказку с типом приложения
                QString appType = getAppType(processName);
                QString tooltipText = QString("%1\n%2\n(%3)").arg(appName).arg(processName).arg(appType);
                item->setToolTipText(tooltipText);

                // Используем QPointer для безопасного подключения
                QPointer<DockItem> safeItem(item);

                connect(item, &DockItem::removeRequested, this, [this, safeItem]() {
                    if (safeItem) {
                        removeRunningApplication(safeItem);
                    }
                });

                connect(item, &DockItem::hideRequested, this, [this, safeItem]() {
                    if (safeItem) {
                        hideRunningApplication(safeItem);
                    }
                });

                newRunningItems.append(item);
                m_layout->addWidget(item);

                qDebug() << "Added new running app to dock:" << appName << "(" << processName << ")";
            }
        }
    }

    // Добавляем проверку ручных процессов
    for (const QString& manualProcess : m_manualProcesses) {
        if (hasWindows(manualProcess)) {
            // Проверяем, не отображается ли уже это приложение
            bool alreadyDisplayed = false;
            for (DockItem* existingItem : m_runningItems) {
                if (!existingItem) continue;

                QString existingExeName = QFileInfo(existingItem->getExecutablePath()).fileName().toLower();
                if (existingExeName == manualProcess) {
                    alreadyDisplayed = true;
                    newRunningItems.append(existingItem);
                    break;
                }
            }

            if (!alreadyDisplayed) {
                QString fullPath = findExecutablePath(manualProcess);
                if (fullPath.isEmpty()) {
                    // Если не нашли полный путь, используем только имя процесса
                    fullPath = manualProcess;
                }

                QIcon icon = getAppIcon(fullPath);
                QString appName = QFileInfo(manualProcess).baseName();

                // Создаем новый элемент для ручного процесса
                DockItem* item = new DockItem(icon, appName, fullPath, true, m_dockWidget);
                if (!item) {
                    qDebug() << "Failed to create DockItem for manual process:" << appName;
                    continue;
                }

                // Сохраняем оригинальную иконку
                item->setProperty("originalIcon", QVariant::fromValue(icon));

                // Устанавливаем специальную подсказку для ручных процессов
                QString appType = getAppType(manualProcess);
                QString tooltipText = QString("%1\n%2\n(%3, добавлено вручную)").arg(appName).arg(manualProcess).arg(appType);
                item->setToolTipText(tooltipText);

                QPointer<DockItem> safeItem(item);

                connect(item, &DockItem::removeRequested, this, [this, safeItem]() {
                    if (safeItem) {
                        removeRunningApplication(safeItem);
                    }
                });

                connect(item, &DockItem::hideRequested, this, [this, safeItem, manualProcess]() {
                    if (safeItem) {
                        // Для ручных процессов предлагаем удалить из отслеживания
                        QMessageBox::StandardButton reply = QMessageBox::question(this,
                            "Удалить из отслеживания",
                            QString("Удалить '%1' из списка отслеживаемых процессов?").arg(manualProcess),
                            QMessageBox::Yes | QMessageBox::No);

                        if (reply == QMessageBox::Yes) {
                            m_manualProcesses.remove(manualProcess.toLower());
                            saveManualProcesses();
                            removeRunningApplication(safeItem);
                        }
                    }
                });

                newRunningItems.append(item);
                m_layout->addWidget(item);

                qDebug() << "Added manual process to dock:" << appName << "(" << manualProcess << ")";
            }
        }
    }

    // Проверяем системные приложения, которые нужно всегда отображать
    QStringList systemApps = {
        "taskmgr.exe",     // Диспетчер задач
        "cmd.exe",         // Командная строка
        "powershell.exe",  // PowerShell
        "explorer.exe",    // Проводник Windows
        "msedge.exe",      // Microsoft Edge
        "chrome.exe",      // Google Chrome
        "firefox.exe",     // Firefox
        "notepad.exe",     // Блокнот
        "calc.exe",        // Калькулятор
        "mspaint.exe"      // Paint
    };

    for (const QString& systemApp : systemApps) {
        // Проверяем наличие процесса
        HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hProcessSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32 pe32;
            pe32.dwSize = sizeof(PROCESSENTRY32);

            bool processExists = false;
            if (Process32First(hProcessSnap, &pe32)) {
                do {
                    QString currentProcess = QString::fromWCharArray(pe32.szExeFile);
                    if (currentProcess.compare(systemApp, Qt::CaseInsensitive) == 0) {
                        processExists = true;
                        break;
                    }
                } while (Process32Next(hProcessSnap, &pe32));
            }
            CloseHandle(hProcessSnap);

            if (hasWindows(systemApp)) {
                // Проверяем, не отображается ли уже
                bool alreadyDisplayed = false;
                for (DockItem* existingItem : newRunningItems) {
                    if (!existingItem) continue;
                    QString existingExeName = QFileInfo(existingItem->getExecutablePath()).fileName().toLower();
                    if (existingExeName == systemApp.toLower()) {
                        alreadyDisplayed = true;
                        break;
                    }
                }

                // Проверяем, не закреплено ли
                bool isPinned = false;
                for (DockItem* item : m_items) {
                    if (!item) continue;
                    QString pinnedExeName = QFileInfo(item->getExecutablePath()).fileName().toLower();
                    if (pinnedExeName == systemApp.toLower()) {
                        isPinned = true;
                        break;
                    }
                }

                // Проверяем, не скрыто ли
                bool isHidden = m_dockAppManager->isAppHidden(systemApp);

                if (!alreadyDisplayed && !isPinned && !isHidden) {
                    QString fullPath = findExecutablePath(systemApp);
                    if (fullPath.isEmpty()) {
                        // Попробуем найти в стандартных путях
                        QStringList systemPaths = {
                            "C:\\Windows\\System32\\",
                            "C:\\Windows\\",
                            "C:\\Program Files\\",
                            "C:\\Program Files (x86)\\"
                        };

                        for (const QString& path : systemPaths) {
                            QString testPath = path + systemApp;
                            if (QFile::exists(testPath)) {
                                fullPath = testPath;
                                break;
                            }
                        }
                    }

                    if (!fullPath.isEmpty()) {
                        QIcon icon = getAppIcon(fullPath);
                        QString appName = getAppDisplayName(systemApp);

                        // Создаем новый элемент для системного приложения
                        DockItem* item = new DockItem(icon, appName, fullPath, true, m_dockWidget);
                        if (item) {
                            item->setProperty("originalIcon", QVariant::fromValue(icon));

                            // Устанавливаем подсказку для системного приложения
                            QString appType = getAppType(systemApp);
                            QString tooltipText = QString("%1\n%2\n(%3, системное приложение)").arg(appName).arg(systemApp).arg(appType);
                            item->setToolTipText(tooltipText);

                            QPointer<DockItem> safeItem(item);

                            connect(item, &DockItem::removeRequested, this, [this, safeItem]() {
                                if (safeItem) {
                                    removeRunningApplication(safeItem);
                                }
                            });

                            connect(item, &DockItem::hideRequested, this, [this, safeItem, systemApp]() {
                                if (safeItem) {
                                    hideRunningApplication(safeItem);
                                }
                            });

                            newRunningItems.append(item);
                            m_layout->addWidget(item);
                            qDebug() << "Added system app to dock:" << systemApp << "(" << appName << ")";
                        }
                    }
                }
            }
        }
    }
#endif

    // Удаляем только те элементы, которых больше нет в списке запущенных процессов
    QList<DockItem*> itemsToRemove;
    for (DockItem* existingItem : m_runningItems) {
        if (!existingItem) continue;

        bool stillExists = false;
        for (DockItem* newItem : newRunningItems) {
            if (newItem == existingItem) {
                stillExists = true;
                break;
            }
        }

        if (!stillExists) {
            itemsToRemove.append(existingItem);
        }
    }

    // Безопасно удаляем элементы, которые больше не нужны
    for (DockItem* itemToRemove : itemsToRemove) {
        if (itemToRemove) {
            qDebug() << "Removing running app from dock:" << itemToRemove->getName();

            // Отключаем все сигналы
            itemToRemove->disconnect();

            // Удаляем из layout
            m_layout->removeWidget(itemToRemove);

            // Удаляем из текущего списка
            m_runningItems.removeAll(itemToRemove);

            // Удаляем объект
            delete itemToRemove;
        }
    }

    // Обновляем основной список
    m_runningItems = newRunningItems;

    // Перезапускаем таймер
    if (m_runningCheckTimer) {
        m_runningCheckTimer->start();
    }

    // Обновляем позицию дока
    updateDockPosition();

    m_updatingRunningApps = false;
    //qDebug() << "Finished updateRunningApps";
}

bool Dock::hasWindows(const QString& executableName)
{
#ifdef Q_OS_WIN
    QString processName = QFileInfo(executableName).fileName();

    // Специальная обработка для диспетчера задач
    if (processName.compare("taskmgr.exe", Qt::CaseInsensitive) == 0) {
        // Для taskmgr.exe используем более либеральные критерии поиска
        HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hProcessSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32 pe32;
            pe32.dwSize = sizeof(PROCESSENTRY32);

            if (Process32First(hProcessSnap, &pe32)) {
                do {
                    QString currentProcess = QString::fromWCharArray(pe32.szExeFile);
                    if (currentProcess.compare("taskmgr.exe", Qt::CaseInsensitive) == 0) {
                        CloseHandle(hProcessSnap);
                        return true; // Процесс существует
                    }
                } while (Process32Next(hProcessSnap, &pe32));
            }
            CloseHandle(hProcessSnap);
        }
        return false;
    }

    QList<QPair<HWND, QString>> windows = FindAllWindowsByProcess(processName);

    // Если не нашли окон по точному имени, пробуем найти по частичному совпадению
    if (windows.isEmpty()) {
        // Для ручных процессов можем попробовать альтернативные варианты
        QString lowerName = processName.toLower();

        // Пробуем разные варианты имени процесса
        if (lowerName.contains("arena breakout")) {
            // ... существующий код ...
        }
    }

    // Фильтруем только видимые окна с заголовками
    for (const auto& window : windows) {
        if (IsWindowVisible(window.first) && !window.second.isEmpty()) {
            return true;
        }
    }
#endif
    return false;
}

QString Dock::findExecutablePath(const QString& executableName)
{
#ifdef Q_OS_WIN
    // Ищем полный путь к исполняемому файлу
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        return "";
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return "";
    }

    do {
        QString currentProcess = QString::fromWCharArray(pe32.szExeFile);
        if (currentProcess.compare(executableName, Qt::CaseInsensitive) == 0) {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
            if (hProcess) {
                WCHAR exePath[MAX_PATH];
                if (GetModuleFileNameEx(hProcess, NULL, exePath, MAX_PATH)) {
                    CloseHandle(hProcess);
                    CloseHandle(hProcessSnap);
                    return QString::fromWCharArray(exePath);
                }
                CloseHandle(hProcess);
            }
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
#endif
    return "";
}

QIcon Dock::getAppIcon(const QString& executablePath)
{
    if (executablePath.isEmpty()) {
        return QIcon();
    }
    return m_dockAppManager->extractIconFromExecutable(executablePath);
}

void Dock::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Очищаем область (важно для прозрачности)
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    // Рисуем фон дока только для области m_dockWidget
    QRect dockRect = m_dockWidget->rect();

    // Фон с закругленными углы
    QPainterPath path;
    path.addRoundedRect(dockRect, m_currentDockCornerRadius, m_currentDockCornerRadius);

    painter.setPen(Qt::NoPen);
    painter.setBrush(m_currentDockBackgroundColor);
    painter.drawPath(path);

    // Рисуем границу
    painter.setPen(QPen(m_currentDockBorderColor, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(dockRect.adjusted(1, 1, -1, -1), m_currentDockCornerRadius, m_currentDockCornerRadius);
}

void Dock::updateDockPosition()
{
    if (!m_targetScreen) {
        m_targetScreen = QGuiApplication::primaryScreen();
    }

    if (!m_targetScreen) return;

    QRect screenGeometry = m_targetScreen->geometry();

    int totalItems = m_items.size() + m_runningItems.size();
    int dockWidth = totalItems * (m_currentIconSize + DockConstants::ICON_SPACING)
                    + (totalItems - 1) * DockConstants::ICON_SPACING
                    + 40; // отступы

    int maxWidth = screenGeometry.width() * 0.8;
    dockWidth = qMin(dockWidth, maxWidth);
    dockWidth = qMax(dockWidth, 200); // минимальная ширина

    int x = screenGeometry.x() + (screenGeometry.width() - dockWidth) / 2;
    int y = screenGeometry.y() + screenGeometry.height() - m_currentDockHeight - DockConstants::DOCK_MARGIN_BOTTOM;

    // Убедимся, что координаты не выходят за пределы экрана
    x = qMax(screenGeometry.x(), x);
    y = qMax(screenGeometry.y(), y);

    // Убедимся, что размеры не превышают размеры экрана
    int actualWidth = qMin(dockWidth, screenGeometry.width());
    int actualHeight = qMin(m_currentDockHeight, screenGeometry.height());

    setGeometry(x, y, actualWidth, actualHeight);
    m_dockWidget->setGeometry(0, 0, actualWidth, actualHeight);

    // Гарантируем, что док всегда на верхнем уровне, но без активации
    raise();

    // Принудительное обновление отображения
    update();
}

void Dock::contextMenuEvent(QContextMenuEvent* event)
{
    showContextMenu(event->pos());
}

bool Dock::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseMove) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QPoint globalPos = mouseEvent->globalPosition().toPoint();

        // Проверяем, находится ли мышь над доком
        if (geometry().contains(mapFromGlobal(globalPos))) {
            // Если мышь над доком, гарантируем что он показан
            if (m_isHidden && m_animationManager->shouldShowDock()) {
                m_animationManager->startShowAnimation();
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

QRect Dock::activationZone() const
{
    if (!m_targetScreen) {
        return QRect();
    }

    QRect screenGeometry = m_targetScreen->geometry();
    QRect dockGeometry = geometry();

    // Зона активации имеет ширину дока + небольшой отступ для удобства
    int zoneWidth = dockGeometry.width() + DockConstants::ACTIVATION_BOTTOM_SPACING;
    int zoneHeight = DockConstants::ACTIVATION_ZONE_HEIGHT + DockConstants::ACTIVATION_BOTTOM_FIX;

    int zoneX = dockGeometry.center().x() - zoneWidth / 2;
    int zoneY = screenGeometry.bottom() - DockConstants::ACTIVATION_ZONE_HEIGHT;

    // Ограничиваем зону экраном (по горизонтали)
    zoneX = qMax(screenGeometry.left(), zoneX);
    zoneWidth = qMin(zoneWidth, screenGeometry.width());

    return QRect(zoneX, zoneY, zoneWidth, zoneHeight);
}

void Dock::checkMousePosition()
{
    if (!m_targetScreen) {
        m_targetScreen = QGuiApplication::primaryScreen();
    }

    if (!m_targetScreen) return;

    QPoint globalPos = QCursor::pos();
    QRect screenGeometry = m_targetScreen->geometry();

    // Проверяем, находится ли мышь на том же экране, что и док
    if (!screenGeometry.contains(globalPos)) {
        // Если мышь не на этом экране, то выходим из зоны активации, если были в ней
        if (m_mouseInActivationZone) {
            m_mouseInActivationZone = false;
            m_animationManager->startHideAnimation();
        }
        return;
    }

    // Получаем зону активации для текущего дока
    QRect activationZoneRect = activationZone();

    // Проверяем, находится ли мышь в зоне активации ИЛИ над самим доком
    bool inActivationZone = activationZoneRect.contains(globalPos);
    bool overDock = geometry().contains(mapFromGlobal(globalPos));

    if ((inActivationZone || overDock) && !m_mouseInActivationZone) {
        // Мышь вошла в зону активации или над доком
        m_mouseInActivationZone = true;
        if (m_animationManager->shouldShowDock()) {
            m_animationManager->startShowAnimation();
        }
    }
    else if (!inActivationZone && !overDock && m_mouseInActivationZone) {
        // Мышь вышла из зоны активации и не над доком
        m_mouseInActivationZone = false;
        m_animationManager->startHideAnimation();
    }
}

void Dock::addApplicationDialog()
{
    // Диалог для ввода имени приложения
    QString name = QInputDialog::getText(this, "Добавить приложение",
                                        "Имя приложения:");
    if (name.isEmpty()) {
        return;
    }

    // Диалог для выбора исполняемого файла
    QString executablePath = QFileDialog::getOpenFileName(this,
                                                         "Выберите исполняемого файла",
                                                         "C:/",
                                                         "Executable Files (*.exe);;All Files (*)");
    if (executablePath.isEmpty()) {
        return;
    }

    // Создаем информацию о приложении для Dock
    DockAppInfo app;
    app.name = name;
    app.executablePath = executablePath;
    app.icon = getAppIcon(executablePath);

    // Добавляем в менеджер приложений Dock
    m_dockAppManager->pinApp(app);
}

void Dock::removeApplication(DockItem* item)
{
    int index = m_items.indexOf(item);
    if (index != -1) {
        QString appName = item->getName();
        QMessageBox::StandardButton reply = QMessageBox::question(this, "Удаление приложения",
                                                                QString("Удалить \"%1\" из дока?").arg(appName),
                                                                QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            // Удаляем из менеджера Dock
            m_dockAppManager->unpinApp(item->getExecutablePath());
        }
    }
}

void Dock::removeRunningApplication(DockItem* item)
{
    if (!item) {
        qDebug() << "removeRunningApplication: item is null";
        return;
    }

    QString appName = item->getName();
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Закрыть приложение",
                                                            QString("Закрыть \"%1\"?").arg(appName),
                                                            QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
#ifdef Q_OS_WIN
        // Закрываем приложение
        QString executableName = QFileInfo(item->getExecutablePath()).fileName();
        QList<QPair<HWND, QString>> windows = FindAllWindowsByProcess(executableName);

        for (const auto& window : windows) {
            PostMessage(window.first, WM_CLOSE, 0, 0);
        }

        // Отключаем сигналы и удаляем элемент
        item->disconnect();
        m_layout->removeWidget(item);
        m_runningItems.removeAll(item);
        delete item;

        qDebug() << "Closed app and removed from dock:" << appName;

        // Ждем немного и обновляем список
        QTimer::singleShot(2000, this, &Dock::updateRunningApps);
#endif
    }
}

void Dock::hideRunningApplication(DockItem* item)
{
    if (!item) {
        qDebug() << "hideRunningApplication: item is null";
        return;
    }

    QString appName = item->getName();
    QString executableName = QFileInfo(item->getExecutablePath()).fileName().toLower();

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Скрыть приложение",
                                                            QString("Скрыть \"%1\" из дока?\nПриложение будет скрыто из дока.").arg(appName),
                                                            QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        qDebug() << "Hiding app:" << executableName;

        // Добавляем в список скрытых приложений через DockMenuAppManager
        m_dockAppManager->addHiddenApp(executableName);

        // Отключаем все сигналы
        item->disconnect();

        // Удаляем из layout
        m_layout->removeWidget(item);

        // Удаляем из списка
        m_runningItems.removeAll(item);

        // Удаляем объект
        delete item;

        qDebug() << "Successfully hidden and removed:" << executableName;

        // Обновляем позицию дока
        updateDockPosition();

        // НЕ вызываем updateRunningApps здесь - дождемся следующего цикла таймера
    }
}

void Dock::onAppsChanged()
{
    setupDock();
}

void Dock::checkRunningApplications()
{
#ifdef Q_OS_WIN
    // Обновляем точки для закрепленных приложений
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        return;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return;
    }

    QSet<QString> runningProcesses;
    do {
        QString processName = QString::fromWCharArray(pe32.szExeFile);
        if (hasWindows(processName)) {
            runningProcesses.insert(processName.toLower());
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);

    // Проверяем каждый элемент дока
    for (DockItem* item : m_items) {
        QString executableName = QFileInfo(item->getExecutablePath()).fileName().toLower();
        bool isRunning = runningProcesses.contains(executableName);

        item->setRunning(isRunning);
    }

    // Обновляем список запущенных приложений
    updateRunningApps();
#endif
}

void Dock::executeWinTab()
{
#ifdef Q_OS_WIN
    // Эмулируем нажатие Win+Tab
    keybd_event(VK_LWIN, 0, 0, 0);
    keybd_event(VK_TAB, 0, 0, 0);
    keybd_event(VK_TAB, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
#endif
}