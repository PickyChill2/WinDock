#include "PigDockExtension.h"
#include "../DockConstants.h"
#include <QTimer>
#include <QStorageInfo>
#include <QMessageBox>
#include <QDir>
#include <QScreen>
#include <QGuiApplication>
#include <QWindow>
#include <QCursor>
#include <QApplication>
#include <QThread>

#include <windows.h>
#include <shellapi.h>

QString PigDockExtension::name() const
{
    return "TrashDockExtension";
}

QString PigDockExtension::version() const
{
    return "1.0";
}

ExtensionDisplayLocation PigDockExtension::displayLocation() const
{
    return ExtensionDisplayLocation::DockRight;
}

QWidget* PigDockExtension::createWidget(QWidget* parent)
{
    m_trashButton = new QPushButton(parent);
    m_trashButton->setFixedSize(32, 32);

    // Устанавливаем эмодзи свиньи
    m_trashButton->setText("🐷");
    m_trashButton->setToolTip("Корзина\nЛевый клик - открыть\nПравый клик - очистить");

    // Стилизация кнопки
    m_trashButton->setStyleSheet(
        "QPushButton {"
        "    background: transparent;"
        "    border: none;"
        "    border-radius: 6px;"
        "    color: white;"
        "}"
        "QPushButton:hover {"
        "    background: rgba(255, 255, 255, 0.2);"
        "}"
        "QPushButton:pressed {"
        "    background: rgba(255, 255, 255, 0.3);"
        "}"
    );

    // Устанавливаем шрифт с поддержкой эмодзи
    QFont font("Segoe UI Emoji", 28);
    m_trashButton->setFont(font);

    QObject::connect(m_trashButton, &QPushButton::clicked, this, &PigDockExtension::onTrashButtonClicked);

    m_trashButton->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(m_trashButton, &QPushButton::customContextMenuRequested,
            [this](const QPoint& pos) {
                this->showTrashContextMenu(m_trashButton, pos);
            });

    m_trashCheckTimer = new QTimer(this);
    connect(m_trashCheckTimer, &QTimer::timeout, this, &PigDockExtension::checkTrashStatus);
    m_trashCheckTimer->start(2000);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    connect(m_refreshTimer, &QTimer::timeout, this, &PigDockExtension::refreshTrashState);

    QTimer::singleShot(100, this, &PigDockExtension::checkTrashStatus);

    return m_trashButton;
}

void PigDockExtension::populateDockContextMenu(QMenu* menu, DockContextMenuType menuType, DockItem* item)
{
    switch (menuType) {
        case DockContextMenuType::DockGlobal:
        {
            menu->addSeparator();
            QAction* trashGlobalAction = menu->addAction("🐷 Хры");
            QObject::connect(trashGlobalAction, &QAction::triggered, this, &PigDockExtension::openTrash);
            break;
        }
    }
}

void PigDockExtension::onTrashButtonClicked()
{
    openTrash();
}

void PigDockExtension::onSettingsAction()
{
    qDebug() << "Открытие настроек корзины...";
}

void PigDockExtension::onGlobalPigAction()
{
    openTrash();
}

void PigDockExtension::onItemPigAction()
{
    openTrash();
}

void PigDockExtension::checkTrashStatus()
{
    // Просто проверяем статус корзины, но не меняем иконку
    bool empty = isTrashEmpty();
    qDebug() << "Статус корзины: empty =" << empty;

    // Обновляем всплывающую подсказку в зависимости от статуса
    if (empty) {
        m_trashButton->setToolTip("Корзина пуста\nЛевый клик - открыть\nПравый клик - очистить");
    } else {
        m_trashButton->setToolTip("Корзина заполнена\nЛевый клик - открыть\nПравый клик - очистить");
    }
}

void PigDockExtension::showTrashContextMenu(QWidget* parent, const QPoint& pos)
{
    QMenu menu;
    menu.setWindowFlags(Qt::Popup);
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

    QAction* openAction = menu.addAction("Открыть корзину");
    QAction* emptyAction = menu.addAction("Очистить корзину");
    menu.addSeparator();

    QObject::connect(openAction, &QAction::triggered, this, &PigDockExtension::openTrash);
    QObject::connect(emptyAction, &QAction::triggered, this, &PigDockExtension::emptyTrash);

    menu.exec(QCursor::pos());
}

void PigDockExtension::openTrash()
{
    QProcess::startDetached("explorer.exe", QStringList() << "shell:RecycleBinFolder");
    qDebug() << "Корзина открыта";
}

void PigDockExtension::emptyTrash()
{
    QTimer::singleShot(0, this, [this]() {
        bool trashEmpty = isTrashEmpty();

        if (trashEmpty) {
            safeShowEmptyMessage();
            return;
        }

        safeShowConfirmMessage();
    });
}

void PigDockExtension::safeShowEmptyMessage()
{
    qDebug() << "Корзина уже пуста";

    QWidget* activeWindow = QApplication::activeWindow();
    QWidget* parent = activeWindow ? activeWindow : m_trashButton;

    QMessageBox::information(parent, "Корзина", "Корзина уже пуста.");
}

void PigDockExtension::safeShowConfirmMessage()
{
    QWidget* activeWindow = QApplication::activeWindow();
    QWidget* parent = activeWindow ? activeWindow : m_trashButton;

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        parent,
        "Очистка корзины",
        "Вы уверены, что хотите очистить корзину?",
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        QTimer::singleShot(100, this, &PigDockExtension::performTrashEmpty);
    }
}

bool PigDockExtension::isTrashEmpty()
{
    SHQUERYRBINFO rbInfo = {0};
    rbInfo.cbSize = sizeof(SHQUERYRBINFO);

    HRESULT hr = SHQueryRecycleBin(NULL, &rbInfo);
    if (SUCCEEDED(hr)) {
        bool empty = (rbInfo.i64NumItems == 0) && (rbInfo.i64Size == 0);
        qDebug() << "Проверка корзины: items =" << rbInfo.i64NumItems << "size =" << rbInfo.i64Size << "empty =" << empty;
        return empty;
    }

    qDebug() << "Ошибка при проверке корзины:" << hr;
    return true;
}

void PigDockExtension::performTrashEmpty()
{
    qDebug() << "Начинаем очистку корзины...";

    // Используем PowerShell для очистки корзины
    QProcess process;
    QString command = "-Command";
    QString script = "Clear-RecycleBin -Force -ErrorAction SilentlyContinue";

    process.start("powershell.exe", QStringList() << command << script);

    if (process.waitForFinished(10000)) {
        qDebug() << "Корзина очищена через PowerShell, код выхода:" << process.exitCode();

        // Обновляем всплывающую подсказку
        m_trashButton->setToolTip("Корзина пуста\nЛевый клик - открыть\nПравый клик - очистить");

        // Проверяем статус через некоторое время
        QTimer::singleShot(1000, this, [this]() {
            checkTrashStatus();
        });

    } else {
        qDebug() << "Таймаут при очистке корзины";
        process.kill();
    }

    m_refreshTimer->start(2000);
}

void PigDockExtension::refreshTrashState()
{
    qDebug() << "Принудительное обновление состояния корзины";
    checkTrashStatus();
}

// Убрал функции forceUpdateTrashIcon и updateTrashIcon, так как иконка не меняется

// Регистрация расширения
REGISTER_EXTENSION(PigDockExtension)