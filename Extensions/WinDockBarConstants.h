// WinDockBarConstants.h
#ifndef WINDOCKBARCONSTANTS_H
#define WINDOCKBARCONSTANTS_H

#include <QColor>


class WinDockBarConstants
{
public:
    // Прозрачности
    static const int OVERLAY_ALPHA = 180;
    static const int WIDGET_BACKGROUND_ALPHA = 220;
    static const int BORDER_ALPHA = 150;

    // Размеры виджетов
    static const int VOLUME_MIXER_MIN_WIDTH = 300;
    static const int VOLUME_MIXER_MAX_WIDTH = 400;
    static const int VOLUME_MIXER_MIN_HEIGHT = 300;
    static const int VOLUME_MIXER_MAX_HEIGHT = 400;

    static const int CLOCK_MIN_WIDTH = 200;
    static const int CLOCK_MAX_WIDTH = 280;
    static const int CLOCK_MIN_HEIGHT = 80;

    // Отступы
    static const int OVERLAY_MARGINS = 30;
    static const int WIDGET_CONTENTS_MARGINS = 15;
    static const int WIDGET_SPACING = 15;

    // Размеры элементов
    static const int VOLUME_ICON_SIZE = 32;
    static const int VOLUME_SLIDER_MIN_WIDTH = 120;
    static const int MIXER_ICON_SIZE = 70;

    // Цвета (можно наследовать от BaseConstants или задать свои)
    static QColor PRIMARY_BACKGROUND_COLOR() { return QColor(45, 45, 45); }
    static QColor SECONDARY_BACKGROUND_COLOR() { return QColor(60, 60, 60); }
    static QColor BORDER_COLOR() { return QColor(100, 100, 100); }
    static QColor TEXT_COLOR() { return QColor(240, 240, 240); }
    static QColor ACCENT_COLOR() { return QColor(0, 120, 215); }

    // Шрифты
    static const char* FONT_FAMILY() { return "Segoe UI"; }
    static const int FONT_SIZE_NORMAL = 12;
    static const int FONT_SIZE_LARGE = 18;
};

#endif // WINDOCKBARCONSTANTS_H