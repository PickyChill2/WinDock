#ifndef VIDEORENDERER_H
#define VIDEORENDERER_H

#include "MediaRenderer.h"
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QTimer>
#include <QScreen>
#include <QProcess>
#include <QProgressBar>
#include "CommonDefines.h"

class VideoRenderer : public MediaRenderer
{
    Q_OBJECT

public:
    explicit VideoRenderer(QScreen* targetScreen, QWidget* parent = nullptr);
    ~VideoRenderer();

    void initialize() override;
    void show() override;
    void hide() override;
    void resize(const QSize& size) override;
    void paint(QPainter* painter, const QRect& rect) override;

    void setSettings(const ScreenSettings& settings) override;
    void setAlignment(const QString& alignment) override;
    void setScaling(const QString& scaling) override;
    void setScaleFactor(qreal scaleFactor) override;

    bool isInitialized() const override { return m_videoInitialized; }
    bool isPlaying() const override;
    void reset() override;

    void setLoop(bool loop);
    void setSound(bool enable);
    void startImmediately();
    void ensurePlaying();
    void updateGeometry();

    QVideoWidget* videoWidget() const { return m_videoWidget; }
    QMediaPlayer* mediaPlayer() const { return m_mediaPlayer; }

public slots:
    void handleSystemWake();
    void checkStatus();
    void handleApplicationStateChange(Qt::ApplicationState state);

signals:
    void videoError(const QString& filePath);

private slots:
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onErrorOccurred(QMediaPlayer::Error error, const QString &errorString);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);

    // Новые слоты для конвертации
    void updateConversionProgress();
    void handleConversionFinished(int exitCode, const QString& outputPath);

private:
    void setupVideoPlayer();
    void createDirectVideoPlayer();  // ДОБАВЛЕНО
    void configureVideoWidget();     // ДОБАВЛЕНО
    void setupHardwareAcceleration();
    void optimizeVideoPlayback();
    void optimizeFor4KVideo();
    void checkVideoCodec();
    void checkVideoCodecFallback();
    QRect applyAlignmentToRect(const QRect& contentRect);
    bool isHighResolution(const QSize& size) const;

    // Исправлен тип возвращаемого значения на void (согласно .cpp)
    void convertH264ToVP9(const QString& inputPath, const QSize& videoSize);

    int m_retryCount = 0;

    QScreen* m_targetScreen;
    ScreenSettings m_settings;
    QSize m_currentSize;
    QWidget* m_parentWidget;

    QMediaPlayer* m_mediaPlayer = nullptr;
    QVideoWidget* m_videoWidget = nullptr;

    // Таймеры
    QTimer* m_wakeupTimer = nullptr;
    QTimer* m_statusCheckTimer = nullptr;
    QTimer* m_optimizationTimer = nullptr;
    QTimer* m_geometryUpdateTimer = nullptr;
    QTimer* m_delayedInitTimer = nullptr;

    // Процессы и UI
    QProcess* m_ffmpegProcess = nullptr;
    QProgressBar* m_progressBar = nullptr;

    bool m_wasSleeping = false;
    bool m_videoOptimized = false;
    bool m_videoInitialized = false;

    int m_videoErrorCount = 0;
    qint64 m_videoDuration = 0;

    QString m_currentVideoPath;
    QString m_convertedVideoPath;
};

#endif // VIDEORENDERER_H