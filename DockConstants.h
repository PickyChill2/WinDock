#ifndef DOCKCONSTANTS_H
#define DOCKCONSTANTS_H

#include <QSize>
#include <QColor>
#include <QString>
#include <QRect>
#include "BaseConstants.h"

namespace DockConstants {
    // Основные размеры
    const int DOCK_HEIGHT = 65;
    const int DOCK_MARGIN_BOTTOM = 15;
    const int ICON_SIZE = 38;
    const int ICON_SPACING = 10;
    const int DOCK_CORNER_RADIUS = 20;
    const int VOLUMEOVERLAY_CORNER_RADIUS = 10;

    const int DOCK_CONTEX_HEIGHT = DOCK_HEIGHT + 15;

    const int MAX_WINDOWS_HEIGHT = DOCK_HEIGHT - 10;

    // Цвета (используем из BaseConstants)
    const QColor DOCK_BACKGROUND_COLOR = QColor(
        BaseConstants::PRIMARY_BACKGROUND_COLOR.red(),
        BaseConstants::PRIMARY_BACKGROUND_COLOR.green(),
        BaseConstants::PRIMARY_BACKGROUND_COLOR.blue(),
        BaseConstants::BACKGROUND_ALPHA
    );

    const QColor DOCK_BORDER_COLOR = QColor(
        BaseConstants::BORDER_COLOR.red(),
        BaseConstants::BORDER_COLOR.green(),
        BaseConstants::BORDER_COLOR.blue(),
        BaseConstants::BORDER_ALPHA
    );

    // Анимации
    const int ANIMATION_DURATION = 200;
    const int HOVER_ANIMATION_DURATION = 150;
    const qreal HOVER_SCALE_FACTOR = 1.2;

    // Размеры окна
    const QSize MAIN_WINDOW_SIZE = QSize(800, 600);
    const QString APPLICATION_NAME = "Dock";

    // Индикатор запущенного приложения
    const int RUNNING_DOT_SIZE = 6;
    const QColor RUNNING_DOT_COLOR = BaseConstants::ACCENT_COLOR;
    const int RUNNING_DOT_MARGIN_BOTTOM = 5;

    // Тени
    const int SHADOW_BLUR_RADIUS = BaseConstants::SHADOW_BLUR_RADIUS;
    const QColor SHADOW_COLOR = QColor(0, 0, 0, BaseConstants::SHADOW_ALPHA);
    const int SHADOW_OFFSET = BaseConstants::SHADOW_OFFSET;

    // Автоматическое скрытие
    const int ACTIVATION_ZONE_HEIGHT = DOCK_HEIGHT + 20; // Высота зоны активации дока (от нижнего края экрана)
    const int AUTO_HIDE_CHECK_INTERVAL = 100; // Интервал проверки автоскрытия (мс)
    const int ACTIVATION_BOTTOM_FIX = 20;
    const int ACTIVATION_BOTTOM_SPACING = 100;
}
#endif // DOCKCONSTANTS_H