#ifndef IMAGERENDERER_H
#define IMAGERENDERER_H

#include "MediaRenderer.h"
#include <QPixmap>

class ImageRenderer : public MediaRenderer
{
    Q_OBJECT

public:
    explicit ImageRenderer(QWidget* parent = nullptr);
    ~ImageRenderer();

    void initialize() override;
    void show() override;
    void hide() override;
    void resize(const QSize& size) override;
    void paint(QPainter* painter, const QRect& rect) override;

    void setSettings(const ScreenSettings& settings) override;
    void setAlignment(const QString& alignment) override;
    void setScaling(const QString& scaling) override;
    void setScaleFactor(qreal scaleFactor) override;

    bool isInitialized() const override { return m_initialized; }
    bool isPlaying() const override { return true; }
    void reset() override;

private:
    void loadAndScaleImage();
    void applyScaling();
    QRect applyAlignmentToRect(const QRect& contentRect);

    ScreenSettings m_settings;
    QPixmap m_backgroundPixmap;
    bool m_initialized;
    QSize m_currentSize;
    QWidget* m_parentWidget;
};

#endif // IMAGERENDERER_H