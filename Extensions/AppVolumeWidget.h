#ifndef APPVOLUMEWIDGET_H
#define APPVOLUMEWIDGET_H

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QString>

class AppVolumeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AppVolumeWidget(const QString& appName, const QString& processPath, int volume, QWidget* parent = nullptr);
    ~AppVolumeWidget();

    signals:
        void volumeChanged(const QString& appName, int volume);

private slots:
    void onVolumeSliderValueChanged(int value);

private:
    void setupUi();
    void updateAppIcon();
    void createDefaultIcon();  // Добавьте эту строку

    QString m_appName;
    QString m_processPath;
    int m_volume;

    QHBoxLayout* m_layout;
    QLabel* m_appIcon;
    QLabel* m_appNameLabel;
    QSlider* m_volumeSlider;
    QLabel* m_volumeValue;
};

#endif // APPVOLUMEWIDGET_H