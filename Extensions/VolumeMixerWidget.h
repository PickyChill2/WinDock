#ifndef VOLUMEMIXERWIDGET_H
#define VOLUMEMIXERWIDGET_H

#include "WinDockBarConstants.h"
#include "AppVolumeWidget.h"
#include "../AudioDeviceManager.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QScrollArea>
#include <QTimer>
#include <QMap>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <psapi.h>
#endif

class VolumeMixerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VolumeMixerWidget(QWidget* parent = nullptr);
    ~VolumeMixerWidget();

private slots:
    void onVolumeSliderValueChanged(int value);
    void onDeviceComboBoxChanged(int index);
    void onAppVolumeChanged(const QString& appName, int volume);
    void updateAudioSessions();

    void onSystemVolumeChanged(int volume);
    void onSystemDeviceChanged(const QString& deviceId);

private:
    void setupAudio();
    void updateAudioDevices();
    void setSystemVolume(int volume);
    void updateVolumeIcon(int value);

#ifdef Q_OS_WIN
    void cleanupAudio();
    void cleanupAudioSessions();
    QString getDeviceDisplayName(IMMDevice* pDevice);
    QString getProcessName(DWORD processId);
    void setupAudioSessions();

    IMMDeviceEnumerator* m_pEnumerator;
    IMMDevice* m_pDevice;
    IAudioEndpointVolume* m_pEndpointVolume;
    IAudioSessionManager2* m_pSessionManager;
#endif

    QVBoxLayout* m_mainLayout;
    QLabel* m_volumeIcon;
    QLabel* m_volumeValue;
    QSlider* m_volumeSlider;
    QComboBox* m_deviceComboBox;

    QLabel* m_appsLabel;
    QScrollArea* m_appsScrollArea;
    QWidget* m_appsContainer;
    QVBoxLayout* m_appsLayout;
    QTimer* m_sessionUpdateTimer;

    int m_currentVolume;
    QString m_currentDeviceId;
    QMap<QString, AppVolumeWidget*> m_appVolumeWidgets;
    QString getProcessName(DWORD processId, QString& processPath);
};

#endif // VOLUMEMIXERWIDGET_H