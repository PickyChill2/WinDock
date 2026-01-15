#include "DockContextMenu.h"
#include <QShowEvent>
#include <QHideEvent>
#include <QApplication>
#include <QDebug>

#include "Dock.h"

DockContextMenu::DockContextMenu(QWidget* parent)
    : QMenu(parent)
{
    // Устанавливаем флаги, чтобы меню всегда было поверх всех окон
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::WindowStaysOnTopHint);

    // Устанавливаем стиль
    setStyleSheet(
        "DockContextMenu {"
        "    background-color: #2D2D2D;"
        "    border: 1px solid #404040;"
        "    border-radius: 8px;"
        "    padding: 4px;"
        "}"
        "DockContextMenu::item {"
        "    background-color: transparent;"
        "    color: white;"
        "    padding: 6px 12px;"
        "    border-radius: 4px;"
        "    margin: 2px;"
        "}"
        "DockContextMenu::item:selected {"
        "    background-color: #404040;"
        "}"
        "DockContextMenu::separator {"
        "    height: 1px;"
        "    background-color: #404040;"
        "    margin: 4px 8px;"
        "}"
    );
}

void DockContextMenu::showEvent(QShowEvent* event)
{
    // Гарантируем, что меню будет поверх всех окон
    raise();
    activateWindow();

    // Если родитель - Dock, позиционируем меню относительно нижней границы экрана
    if (parentWidget()) {
        Dock* dock = qobject_cast<Dock*>(parentWidget());
        if (dock) {
            QScreen* screen = dock->getTargetScreen();
            if (!screen) {
                screen = QGuiApplication::primaryScreen();
            }

            if (screen) {
                QRect screenGeometry = screen->geometry();
                int menuY = screenGeometry.bottom() - DockConstants::DOCK_CONTEX_HEIGHT - height() - 5; // 5px отступ

                // Сохраняем текущую горизонтальную позицию
                int currentX = x();

                // Двигаем меню в вычисленную позицию
                move(currentX, menuY);
            }
        }
    }

    // Принудительно обновляем отображение
    update();
    repaint();

    QMenu::showEvent(event);

    qDebug() << "DockContextMenu shown";
}

void DockContextMenu::hideEvent(QHideEvent* event)
{
    QMenu::hideEvent(event);
    qDebug() << "DockContextMenu hidden";
}