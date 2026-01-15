#ifndef BASECONSTANTS_H
#define BASECONSTANTS_H

#include <QColor>
#include <QString>

namespace BaseConstants {
    // Основные цвета
    const QColor PRIMARY_BACKGROUND_COLOR = QColor(35, 35, 35);
    const QColor SECONDARY_BACKGROUND_COLOR = QColor(40, 40, 40);
    const QColor BORDER_COLOR = QColor(100, 100, 100);
    const QColor TEXT_COLOR = QColor(255, 255, 255);
    const QColor ACCENT_COLOR = QColor(255, 255, 255);

    // Прозрачность
    const int BACKGROUND_ALPHA = 200;
    const int BORDER_ALPHA = 100;
    const int SHADOW_ALPHA = 80;

    // Шрифты
    const QString FONT_FAMILY = "segoe ui";
    const int FONT_SIZE_NORMAL = 11;
    const int FONT_SIZE_LARGE = 16;

    // Тени
    const int SHADOW_BLUR_RADIUS = 20;
    const int SHADOW_OFFSET = 5;

    const int BLUR_RADIUS = 10;

}

#endif // BASECONSTANTS_H