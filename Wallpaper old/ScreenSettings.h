#ifndef SCREENSETTINGS_H
#define SCREENSETTINGS_H

#include <QString>

struct ScreenSettings {
    QString backgroundImage;
    QString alignment;  // center, left, right
    QString scaling;    // fill, fit, stretch, center, tile, zoom
    qreal scaleFactor;  // 1.0, 1.25, 1.5, etc.
    bool isVideo;       // флаг, указывающий что это видео
    bool loopVideo;     // зацикливать видео
    bool enableSound;   // включить звук

    ScreenSettings() : alignment("center"), scaling("fill"), scaleFactor(1.0),
                      isVideo(false), loopVideo(true), enableSound(false) {}
};

#endif // SCREENSETTINGS_H