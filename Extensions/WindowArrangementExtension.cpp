#include "WindowArrangementExtension.h"
#include "..//WindowArrangementManager.h"
#include <QApplication>
#include <QTimer>
#include <QWidget>

WindowArrangementExtension::WindowArrangementExtension()
    : m_enabled(false)
    , m_settingsTimer(nullptr)
    , m_settings(nullptr)
{

    // Создаем настройки
    m_settings = new QSettings("MyCompany", "DockApp", this);

    // Создаем таймер для проверки настроек
    m_settingsTimer = new QTimer(this);
    m_settingsTimer->setInterval(1000);
    connect(m_settingsTimer, &QTimer::timeout, this, &WindowArrangementExtension::onSettingsChanged);

    // Запускаем таймер проверки настроек
    m_settingsTimer->start();

    // Проверяем настройки сразу
    QTimer::singleShot(1000, this, &WindowArrangementExtension::onSettingsChanged);
}

WindowArrangementExtension::~WindowArrangementExtension()
{

    // Отключаем функционал при уничтожении
    WindowArrangementManager::instance()->setEnabled(false);

    if (m_settingsTimer) {
        m_settingsTimer->stop();
    }
}

QWidget* WindowArrangementExtension::createWidget(QWidget* parent)
{

    // Для этого расширения не создаем виджет, так как оно работает в фоне
    QWidget* widget = new QWidget(parent);
    widget->setObjectName("WindowArrangementExtension_Widget");
    widget->setVisible(false);

    return widget;
}

void WindowArrangementExtension::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;

    // Управляем менеджером
    WindowArrangementManager::instance()->setEnabled(enabled);
}

void WindowArrangementExtension::onSettingsChanged()
{
    // Проверяем, включено ли расширение в ExtensionManager
    ExtensionManager& extensionManager = ExtensionManager::instance();
    bool extensionEnabled = extensionManager.isExtensionEnabled("WindowArrangementExtension");
    
    if (extensionEnabled != m_enabled) {
        setEnabled(extensionEnabled);
    }
}

REGISTER_EXTENSION(WindowArrangementExtension)