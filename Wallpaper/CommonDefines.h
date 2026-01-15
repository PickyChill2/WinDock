#ifndef COMMONDEFINES_H
#define COMMONDEFINES_H

#include <QString>
// Настройки задержек
namespace DesktopBackgroundSettings {
    // Задержки в миллисекундах
    const int INITIAL_VIDEO_DELAY_MS = 1000;           // Задержка перед первой инициализацией видео
    const int FORCED_VIDEO_START_DELAY_MS = 300;      // Задержка перед принудительным стартом видео
    const int VIDEO_STATUS_CHECK_INTERVAL_MS = 3000;  // Интервал проверки статуса видео
    const int VIDEO_GEOMETRY_UPDATE_DELAY_MS = 50;    // Задержка перед обновлением геометрии видео
    const int VIDEO_RESTART_DELAY_MS = 50;           // Задержка перед перезапуском видео
    const int POST_INITIALIZATION_DELAY_MS = 1500;   // Задержка после инициализации перед проверкой
    const int WAKEUP_CHECK_INTERVAL_MS = 1000;       // Интервал проверки пробуждения системы
    const int QUICK_START_DELAY_MS = 100;            // Задержка быстрого старта
    // Настройки FPS
    const int DEFAULT_TARGET_FPS = 30;               // Целевой FPS по умолчанию
    const int MAX_TARGET_FPS = 30;                   // Максимальный FPS
    const int MIN_TARGET_FPS = 5;                    // Минимальный FPS
    const int LOW_POWER_TARGET_FPS = 10;             // FPS для режима низкой нагрузки
    const int VIDEO_UPDATE_INTERVAL_MS = 100;        // Интервал обновления видео (10 FPS)
    // Настройки оптимизации
    const int VIDEO_BUFFER_SIZE = 2;
    const  int VIDEO_BUFFER_SIZE_4K = 5;
    const int VIDEO_DECODING_THREADS = 1;            // Количество потоков декодирования
    const int MAX_VIDEO_ERROR_COUNT = 3;             // Максимальное количество ошибок видео
    const int VIDEO_ERROR_RETRY_DELAY_MS = 1000;     // Задержка перед повторной попыткой при ошибке
}
// Структура для хранения настроек каждого экрана
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
#endif // COMMONDEFINES_H