#ifndef MEDIARENDERER_H
#define MEDIARENDERER_H

#include "CommonDefines.h"
#include <QObject>
#include <QWidget>
#include <QPainter>
#include <QRect>
#include <QSize>

class MediaRenderer : public QObject
{
    Q_OBJECT

public:
    explicit MediaRenderer(QWidget* parent = nullptr);
    virtual ~MediaRenderer();

    // Основные методы рендеринга
    virtual void initialize() = 0;
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void resize(const QSize& size) = 0;
    virtual void paint(QPainter* painter, const QRect& rect) = 0;

    // Методы для настройки
    virtual void setSettings(const ScreenSettings& settings) = 0;
    virtual void setAlignment(const QString& alignment) = 0;
    virtual void setScaling(const QString& scaling) = 0;
    virtual void setScaleFactor(qreal scaleFactor) = 0;

    // Информационные методы
    virtual bool isInitialized() const = 0;
    virtual bool isPlaying() const = 0;
    virtual void reset() = 0;

protected:
    QWidget* m_parentWidget;
};

#endif // MEDIARENDERER_H