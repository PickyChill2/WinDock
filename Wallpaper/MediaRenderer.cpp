#include "MediaRenderer.h"
#include <QDebug>

MediaRenderer::MediaRenderer(QWidget* parent)
    : QObject(parent)
    , m_parentWidget(parent)
{
    qDebug() << "MediaRenderer created";
}

MediaRenderer::~MediaRenderer()
{
    qDebug() << "MediaRenderer destroyed";
}