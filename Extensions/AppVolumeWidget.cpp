#include "AppVolumeWidget.h"
#include "WinDockBarConstants.h"
#include <QPainter>
#include <QPainterPath>
#include <QFont>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

AppVolumeWidget::AppVolumeWidget(const QString& appName, const QString& processPath, int volume, QWidget* parent)
    : QWidget(parent), m_appName(appName), m_processPath(processPath), m_volume(volume)
{
    setupUi();
    updateAppIcon();
}

AppVolumeWidget::~AppVolumeWidget()
{
}

void AppVolumeWidget::setupUi()
{
    setStyleSheet("background: transparent;");

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(8, 8, 8, 8);
    m_layout->setSpacing(12);

    // App icon with circular mask
    m_appIcon = new QLabel();
    m_appIcon->setFixedSize(WinDockBarConstants::MIXER_ICON_SIZE, WinDockBarConstants::MIXER_ICON_SIZE); // Increased size
    m_appIcon->setStyleSheet("background: transparent; border: none;");
    m_appIcon->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(m_appIcon);

    // App name
    m_appNameLabel = new QLabel(m_appName);
    m_appNameLabel->setStyleSheet(QString(
        "color: %1; font-size: %2px; font-family: %3; "
        "background: transparent; padding: 2px;"
    ).arg(WinDockBarConstants::TEXT_COLOR().name())
     .arg(WinDockBarConstants::FONT_SIZE_NORMAL)
     .arg(WinDockBarConstants::FONT_FAMILY()));
    m_appNameLabel->setMinimumWidth(80);
    m_appNameLabel->setWordWrap(true);
    m_layout->addWidget(m_appNameLabel);

    // Volume slider
    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(m_volume);
    m_volumeSlider->setStyleSheet(QString(
        "QSlider::groove:horizontal {"
        "    background: rgba(%1, %2, %3, %4);"
        "    height: 6px;"
        "    border-radius: 3px;"
        "}"
        "QSlider::handle:horizontal {"
        "    background: %5;"
        "    width: 16px;"
        "    height: 16px;"
        "    border-radius: 8px;"
        "    margin: -5px 0;"
        "}"
        "QSlider::sub-page:horizontal {"
        "    background: %5;"
        "    border-radius: 3px;"
        "}"
    ).arg(WinDockBarConstants::BORDER_COLOR().red())
     .arg(WinDockBarConstants::BORDER_COLOR().green())
     .arg(WinDockBarConstants::BORDER_COLOR().blue())
     .arg(WinDockBarConstants::BORDER_ALPHA)
     .arg(WinDockBarConstants::ACCENT_COLOR().name()));
    m_layout->addWidget(m_volumeSlider, 1);

    // Volume value
    m_volumeValue = new QLabel(QString("%1%").arg(m_volume));
    m_volumeValue->setStyleSheet(QString(
        "color: %1; font-size: %2px; font-family: %3; "
        "background: transparent; min-width: 35px;"
    ).arg(WinDockBarConstants::TEXT_COLOR().name())
     .arg(WinDockBarConstants::FONT_SIZE_NORMAL)
     .arg(WinDockBarConstants::FONT_FAMILY()));
    m_volumeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_layout->addWidget(m_volumeValue);

    connect(m_volumeSlider, &QSlider::valueChanged, this, &AppVolumeWidget::onVolumeSliderValueChanged);
}

void AppVolumeWidget::updateAppIcon()
{
#ifdef Q_OS_WIN
    if (m_processPath.isEmpty() || m_processPath == "System Sounds") {
        // Use default icon for system sounds
        createDefaultIcon();
        return;
    }

    // Extract icon from executable
    HICON hIcon = nullptr;
    HICON hSmallIcon = nullptr;

    // Try to extract the icon
    UINT result = ExtractIconEx(
        reinterpret_cast<const wchar_t*>(m_processPath.utf16()),
        0,  // First icon
        &hIcon,
        &hSmallIcon,
        1   // Only one icon
    );

    if (result > 0 && hSmallIcon) {
        // Convert HICON to QPixmap
        QPixmap pixmap = QPixmap::fromImage(QImage::fromHICON(hSmallIcon));

        if (!pixmap.isNull()) {
            // Create circular pixmap
            QPixmap circularPixmap(40, 40);
            circularPixmap.fill(Qt::transparent);

            QPainter painter(&circularPixmap);
            painter.setRenderHint(QPainter::Antialiasing, true);

            // Create circular clip path
            QPainterPath path;
            path.addEllipse(0, 0, 40, 40);
            painter.setClipPath(path);

            // Scale and draw the icon
            QPixmap scaledPixmap = pixmap.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            painter.drawPixmap(0, 0, scaledPixmap);

            m_appIcon->setPixmap(circularPixmap);

            DestroyIcon(hSmallIcon);
            if (hIcon) DestroyIcon(hIcon);
            return;
        }

        if (hSmallIcon) DestroyIcon(hSmallIcon);
        if (hIcon) DestroyIcon(hIcon);
    }
#endif

    // Fallback to default icon
    createDefaultIcon();
}

void AppVolumeWidget::createDefaultIcon()
{
    QPixmap pixmap(40, 40);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Draw circular background
    painter.setBrush(WinDockBarConstants::ACCENT_COLOR());
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(2, 2, 36, 36);

    // Draw application initial
    painter.setPen(WinDockBarConstants::TEXT_COLOR());
    painter.setFont(QFont(WinDockBarConstants::FONT_FAMILY(), 14, QFont::Bold));

    QString initial = m_appName.left(1).toUpper();
    if (initial.isEmpty()) initial = "?";

    painter.drawText(pixmap.rect(), Qt::AlignCenter, initial);

    m_appIcon->setPixmap(pixmap);
}

void AppVolumeWidget::onVolumeSliderValueChanged(int value)
{
    m_volume = value;
    m_volumeValue->setText(QString("%1%").arg(value));
    emit volumeChanged(m_appName, value);
}