#include "ImageRenderer.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QUrl>

ImageRenderer::ImageRenderer(QWidget* parent)
    : MediaRenderer(parent)
    , m_initialized(false)
{
    m_parentWidget = parent;

    // Инициализируем m_currentSize размером родительского виджета
    if (parent) {
        m_currentSize = parent->size();
    } else {
        m_currentSize = QSize(1920, 1080);
    }
}

ImageRenderer::~ImageRenderer()
{
}

void ImageRenderer::initialize()
{
    loadAndScaleImage();
    m_initialized = true;
}

void ImageRenderer::show()
{
    // Для изображений ничего не делаем при показе
}

void ImageRenderer::hide()
{
    // Для изображений ничего не делаем при скрытии
}

void ImageRenderer::resize(const QSize& size)
{
    if (m_currentSize != size) {
        m_currentSize = size;
        loadAndScaleImage();
    }
}

void ImageRenderer::paint(QPainter* painter, const QRect& rect)
{
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    if (!m_backgroundPixmap.isNull()) {
        QRect targetRect = rect;
        QRect sourceRect = m_backgroundPixmap.rect();

        if (m_settings.scaling == "tile") {
            painter->drawTiledPixmap(rect, m_backgroundPixmap);
        } else if (m_settings.scaling == "stretch") {
            painter->drawPixmap(targetRect, m_backgroundPixmap, sourceRect);
        } else {
            if (m_settings.scaling == "fill") {
                // Для fill масштабируем изображение с сохранением пропорций, заполняя всю область
                // и обрезая края, если это необходимо
                if (!m_backgroundPixmap.isNull()) {
                    // Создаем временный QPixmap с правильным масштабированием
                    QPixmap scaledPixmap = m_backgroundPixmap.scaled(
                        rect.size(),
                        Qt::KeepAspectRatioByExpanding,
                        Qt::SmoothTransformation
                    );

                    // Определяем область для обрезки с учетом выравнивания
                    int x = 0, y = 0;

                    if (scaledPixmap.width() > rect.width()) {
                        if (m_settings.alignment == "left") {
                            x = 0;
                        } else if (m_settings.alignment == "right") {
                            x = scaledPixmap.width() - rect.width();
                        } else { // center
                            x = (scaledPixmap.width() - rect.width()) / 2;
                        }
                    }

                    if (scaledPixmap.height() > rect.height()) {
                        if (m_settings.alignment == "top") {
                            y = 0;
                        } else if (m_settings.alignment == "bottom") {
                            y = scaledPixmap.height() - rect.height();
                        } else { // center
                            y = (scaledPixmap.height() - rect.height()) / 2;
                        }
                    }

                    sourceRect = QRect(x, y, rect.width(), rect.height());
                    painter->drawPixmap(targetRect, scaledPixmap, sourceRect);
                }
            } else if (m_settings.scaling == "fit" || m_settings.scaling == "zoom") {
                // Для fit и zoom центрируем изображение с учетом выравнивания
                targetRect = QRect(0, 0, m_backgroundPixmap.width(), m_backgroundPixmap.height());
                targetRect = applyAlignmentToRect(targetRect);
                painter->drawPixmap(targetRect, m_backgroundPixmap, sourceRect);
            } else if (m_settings.scaling == "center") {
                // Для center позиционируем изображение с учетом выравнивания
                targetRect = QRect(0, 0,
                                 qMin(m_backgroundPixmap.width(), rect.width()),
                                 qMin(m_backgroundPixmap.height(), rect.height()));
                targetRect = applyAlignmentToRect(targetRect);
                painter->drawPixmap(targetRect, m_backgroundPixmap, sourceRect);
            }
        }
    }
}

void ImageRenderer::setSettings(const ScreenSettings& settings)
{
    m_settings = settings;

    // Обновляем размер, если родительский виджет существует
    if (m_parentWidget) {
        m_currentSize = m_parentWidget->size();
    }

    loadAndScaleImage();
}

void ImageRenderer::setAlignment(const QString& alignment)
{
    if (m_settings.alignment != alignment) {
        m_settings.alignment = alignment;

        // Обновляем размер перед перерисовкой
        if (m_parentWidget) {
            m_currentSize = m_parentWidget->size();
        }

        loadAndScaleImage();
    }
}

void ImageRenderer::setScaling(const QString& scaling)
{
    if (m_settings.scaling != scaling) {
        m_settings.scaling = scaling;

        // Обновляем размер перед перерисовкой
        if (m_parentWidget) {
            m_currentSize = m_parentWidget->size();
        }

        loadAndScaleImage();
    }
}

void ImageRenderer::setScaleFactor(qreal scaleFactor)
{
    scaleFactor = qBound(0.1, scaleFactor, 3.0);
    if (!qFuzzyCompare(m_settings.scaleFactor, scaleFactor)) {
        m_settings.scaleFactor = scaleFactor;

        // Обновляем размер перед перерисовкой
        if (m_parentWidget) {
            m_currentSize = m_parentWidget->size();
        }

        if (m_settings.scaling == "zoom") {
            loadAndScaleImage();
        }
    }
}

void ImageRenderer::reset()
{
    // Обновляем размер перед сбросом
    if (m_parentWidget) {
        m_currentSize = m_parentWidget->size();
    }

    loadAndScaleImage();
}

void ImageRenderer::loadAndScaleImage()
{
    if (!m_settings.backgroundImage.isEmpty() && QFile::exists(m_settings.backgroundImage)) {
        QPixmap originalPixmap;
        if (originalPixmap.load(m_settings.backgroundImage)) {
            // Используем актуальный размер
            QSize targetSize = m_currentSize.isValid() ? m_currentSize : QSize(1920, 1080);

            qDebug() << "Loading image:" << m_settings.backgroundImage
                     << "Original size:" << originalPixmap.size()
                     << "Target size:" << targetSize
                     << "Scaling mode:" << m_settings.scaling;

            if (m_settings.scaling == "fill") {
                // Для fill используем KeepAspectRatioByExpanding чтобы заполнить всю область
                m_backgroundPixmap = originalPixmap.scaled(
                    targetSize,
                    Qt::KeepAspectRatioByExpanding,
                    Qt::SmoothTransformation
                );
            } else if (m_settings.scaling == "fit") {
                m_backgroundPixmap = originalPixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            } else if (m_settings.scaling == "stretch") {
                m_backgroundPixmap = originalPixmap.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            } else if (m_settings.scaling == "center" || m_settings.scaling == "tile") {
                m_backgroundPixmap = originalPixmap;
            } else if (m_settings.scaling == "zoom") {
                applyScaling();
            }

            qDebug() << "Image loaded successfully, scaled size:" << m_backgroundPixmap.size();
        } else {
            qDebug() << "Failed to load image:" << m_settings.backgroundImage;
            m_backgroundPixmap = QPixmap();
        }
    } else {
        m_backgroundPixmap = QPixmap();
        if (!m_settings.backgroundImage.isEmpty()) {
            qDebug() << "Image file not found:" << m_settings.backgroundImage;
        }
    }
}

void ImageRenderer::applyScaling()
{
    if (!m_settings.backgroundImage.isEmpty() && QFile::exists(m_settings.backgroundImage)) {
        QPixmap originalPixmap;
        if (originalPixmap.load(m_settings.backgroundImage)) {
            QSize scaledSize = originalPixmap.size() * m_settings.scaleFactor;
            m_backgroundPixmap = originalPixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
}

QRect ImageRenderer::applyAlignmentToRect(const QRect& contentRect)
{
    QRect result = contentRect;

    if (m_currentSize.isEmpty()) {
        return result;
    }

    QRect targetRect = QRect(QPoint(0, 0), m_currentSize);

    // Горизонтальное выравнивание
    if (m_settings.alignment == "left") {
        result.moveLeft(targetRect.left());
    } else if (m_settings.alignment == "right") {
        result.moveRight(targetRect.right());
    } else { // center
        result.moveCenter(QPoint(targetRect.center().x(), result.center().y()));
    }

    // Вертикальное выравнивание
    if (m_settings.alignment == "top") {
        result.moveTop(targetRect.top());
    } else if (m_settings.alignment == "bottom") {
        result.moveBottom(targetRect.bottom());
    } else { // center
        result.moveCenter(QPoint(result.center().x(), targetRect.center().y()));
    }

    return result;
}