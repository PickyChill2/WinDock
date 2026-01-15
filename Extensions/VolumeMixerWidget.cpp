#include "VolumeMixerWidget.h"
#include <QProcess>
#include <QDebug>

#ifdef Q_OS_WIN
#include <comdef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <shellapi.h>

// Явное определение PROPERTYKEY для имени устройства
const PROPERTYKEY PKEY_Device_FriendlyName = {
    { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } },
    14
};
#endif

VolumeMixerWidget::VolumeMixerWidget(QWidget* parent)
    : QWidget(parent)
{
#ifdef Q_OS_WIN
    m_pEnumerator = nullptr;
    m_pDevice = nullptr;
    m_pEndpointVolume = nullptr;
    m_pSessionManager = nullptr;
#endif

    // Получаем начальные значения из AudioDeviceManager
    AudioDeviceManager* audioManager = AudioDeviceManager::instance();
    m_currentVolume = audioManager->getVolume();
    m_currentDeviceId = audioManager->getCurrentDeviceId();

    QString widgetStyle = QString(
        "background-color: rgba(%1, %2, %3, %4);"
        "border: 1px solid rgba(%5, %6, %7, %8);"
        "border-radius: 10px;"
        "padding: %9px;"
    ).arg(WinDockBarConstants::SECONDARY_BACKGROUND_COLOR().red())
     .arg(WinDockBarConstants::SECONDARY_BACKGROUND_COLOR().green())
     .arg(WinDockBarConstants::SECONDARY_BACKGROUND_COLOR().blue())
     .arg(WinDockBarConstants::WIDGET_BACKGROUND_ALPHA)
     .arg(WinDockBarConstants::BORDER_COLOR().red())
     .arg(WinDockBarConstants::BORDER_COLOR().green())
     .arg(WinDockBarConstants::BORDER_COLOR().blue())
     .arg(WinDockBarConstants::BORDER_ALPHA)
     .arg(WinDockBarConstants::WIDGET_CONTENTS_MARGINS);

    setStyleSheet(widgetStyle);
    setMinimumWidth(WinDockBarConstants::VOLUME_MIXER_MIN_WIDTH + WinDockBarConstants::MIXER_ICON_SIZE);
    setMaximumWidth(WinDockBarConstants::VOLUME_MIXER_MAX_WIDTH + WinDockBarConstants::MIXER_ICON_SIZE);
    setMinimumHeight(WinDockBarConstants::VOLUME_MIXER_MIN_HEIGHT);

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(
        WinDockBarConstants::WIDGET_CONTENTS_MARGINS,
        WinDockBarConstants::WIDGET_CONTENTS_MARGINS,
        WinDockBarConstants::WIDGET_CONTENTS_MARGINS,
        WinDockBarConstants::WIDGET_CONTENTS_MARGINS
    );
    m_mainLayout->setSpacing(WinDockBarConstants::WIDGET_SPACING);

    // Device selection
    QLabel* deviceLabel = new QLabel("Output Device:");
    deviceLabel->setStyleSheet(QString(
        "color: %1; font-size: 14px; font-weight: bold; font-family: %2; background: transparent;"
    ).arg(WinDockBarConstants::TEXT_COLOR().name(), WinDockBarConstants::FONT_FAMILY()));
    m_mainLayout->addWidget(deviceLabel);

    m_deviceComboBox = new QComboBox();
    m_deviceComboBox->setStyleSheet(QString(
        "QComboBox {"
        "    background: rgba(%1, %2, %3, 150);"
        "    color: %4;"
        "    border: 1px solid rgba(%5, %6, %7, %8);"
        "    border-radius: 5px;"
        "    padding: 8px;"
        "    font-family: %9;"
        "    font-size: %10px;"
        "}"
        "QComboBox::drop-down {"
        "    border: none;"
        "}"
        "QComboBox::down-arrow {"
        "    image: none;"
        "    border-left: 5px solid transparent;"
        "    border-right: 5px solid transparent;"
        "    border-top: 5px solid %4;"
        "    width: 0px;"
        "    height: 0px;"
        "}"
        "QComboBox QAbstractItemView {"
        "    background: rgba(%1, %2, %3, 220);"
        "    color: %4;"
        "    border: 1px solid rgba(%5, %6, %7, %8);"
        "    selection-background-color: rgba(%4, %4, %4, 50);"
        "    font-family: %9;"
        "    font-size: %10px;"
        "}"
    ).arg(WinDockBarConstants::PRIMARY_BACKGROUND_COLOR().red())
     .arg(WinDockBarConstants::PRIMARY_BACKGROUND_COLOR().green())
     .arg(WinDockBarConstants::PRIMARY_BACKGROUND_COLOR().blue())
     .arg(WinDockBarConstants::TEXT_COLOR().name())
     .arg(WinDockBarConstants::BORDER_COLOR().red())
     .arg(WinDockBarConstants::BORDER_COLOR().green())
     .arg(WinDockBarConstants::BORDER_COLOR().blue())
     .arg(WinDockBarConstants::BORDER_ALPHA)
     .arg(WinDockBarConstants::FONT_FAMILY())
     .arg(WinDockBarConstants::FONT_SIZE_NORMAL));
    m_mainLayout->addWidget(m_deviceComboBox);

    m_mainLayout->addSpacing(10);

    // Volume controls
    QHBoxLayout* volumeLayout = new QHBoxLayout();
    volumeLayout->setSpacing(10);

    // Volume icon
    m_volumeIcon = new QLabel();
    m_volumeIcon->setFixedSize(
        WinDockBarConstants::VOLUME_ICON_SIZE,
        WinDockBarConstants::VOLUME_ICON_SIZE
    );
    m_volumeIcon->setStyleSheet("background: transparent; border: none;");

    // Horizontal volume slider
    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(m_currentVolume);
    m_volumeSlider->setMinimumWidth(WinDockBarConstants::VOLUME_SLIDER_MIN_WIDTH);
    m_volumeSlider->setStyleSheet(QString(
        "QSlider::groove:horizontal {"
        "    background: rgba(%1, %2, %3, %4);"
        "    height: 8px;"
        "    border-radius: 4px;"
        "}"
        "QSlider::handle:horizontal {"
        "    background: %5;"
        "    width: 20px;"
        "    height: 20px;"
        "    border-radius: 10px;"
        "    margin: -6px 0;"
        "}"
        "QSlider::sub-page:horizontal {"
        "    background: %5;"
        "    border-radius: 4px;"
        "}"
    ).arg(WinDockBarConstants::BORDER_COLOR().red())
     .arg(WinDockBarConstants::BORDER_COLOR().green())
     .arg(WinDockBarConstants::BORDER_COLOR().blue())
     .arg(WinDockBarConstants::BORDER_ALPHA)
     .arg(WinDockBarConstants::ACCENT_COLOR().name()));

    // Volume value
    m_volumeValue = new QLabel(QString("%1%").arg(m_currentVolume));
    m_volumeValue->setStyleSheet(QString(
        "color: %1; font-size: 16px; font-weight: bold; font-family: %2; "
        "background: transparent; min-width: 40px;"
    ).arg(WinDockBarConstants::TEXT_COLOR().name(), WinDockBarConstants::FONT_FAMILY()));
    m_volumeValue->setAlignment(Qt::AlignCenter);

    volumeLayout->addWidget(m_volumeIcon);
    volumeLayout->addWidget(m_volumeSlider, 1);
    volumeLayout->addWidget(m_volumeValue);

    m_mainLayout->addLayout(volumeLayout);
    m_mainLayout->addSpacing(15);

    // Application mixer section
    m_appsLabel = new QLabel("Application Volume:");
    m_appsLabel->setStyleSheet(QString(
        "color: %1; font-size: 14px; font-weight: bold; font-family: %2; background: transparent;"
    ).arg(WinDockBarConstants::TEXT_COLOR().name(), WinDockBarConstants::FONT_FAMILY()));
    m_mainLayout->addWidget(m_appsLabel);

    // Scroll area for apps
    m_appsScrollArea = new QScrollArea();
    m_appsScrollArea->setStyleSheet("background: transparent; border: none;");
    m_appsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_appsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_appsScrollArea->setWidgetResizable(true);
    m_appsScrollArea->setMaximumHeight(200);

    m_appsContainer = new QWidget();
    m_appsContainer->setStyleSheet("background: transparent;");

    m_appsLayout = new QVBoxLayout(m_appsContainer);
    m_appsLayout->setContentsMargins(5, 5, 5, 5);
    m_appsLayout->setSpacing(5);

    m_appsScrollArea->setWidget(m_appsContainer);
    m_mainLayout->addWidget(m_appsScrollArea);

    // Подключаемся к сигналам AudioDeviceManager
    connect(audioManager, &AudioDeviceManager::volumeChanged,
            this, &VolumeMixerWidget::onSystemVolumeChanged);
    connect(audioManager, &AudioDeviceManager::deviceChanged,
            this, &VolumeMixerWidget::onSystemDeviceChanged);

    // Connect signals
    connect(m_volumeSlider, &QSlider::valueChanged, this, &VolumeMixerWidget::onVolumeSliderValueChanged);
    connect(m_deviceComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VolumeMixerWidget::onDeviceComboBoxChanged);

    // Setup audio and timer for session updates
    setupAudio();
    updateAudioDevices();
    updateVolumeIcon(m_currentVolume);

    m_sessionUpdateTimer = new QTimer(this);
    connect(m_sessionUpdateTimer, &QTimer::timeout, this, &VolumeMixerWidget::updateAudioSessions);
    m_sessionUpdateTimer->start(2000); // Update every 2 seconds

    // Initial session setup
    updateAudioSessions();
}

VolumeMixerWidget::~VolumeMixerWidget()
{
#ifdef Q_OS_WIN
    cleanupAudio();
    cleanupAudioSessions();
#endif
}

#ifdef Q_OS_WIN
void VolumeMixerWidget::cleanupAudio()
{
    if (m_pEndpointVolume) {
        m_pEndpointVolume->Release();
        m_pEndpointVolume = nullptr;
    }
    if (m_pDevice) {
        m_pDevice->Release();
        m_pDevice = nullptr;
    }
    if (m_pEnumerator) {
        m_pEnumerator->Release();
        m_pEnumerator = nullptr;
    }
}

void VolumeMixerWidget::cleanupAudioSessions()
{
    if (m_pSessionManager) {
        m_pSessionManager->Release();
        m_pSessionManager = nullptr;
    }
}
#endif

void VolumeMixerWidget::setupAudio()
{
#ifdef Q_OS_WIN
    CoInitialize(NULL);

    m_pEnumerator = nullptr;
    m_pDevice = nullptr;
    m_pEndpointVolume = nullptr;
    m_pSessionManager = nullptr;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
                                  CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                  (void**)&m_pEnumerator);
    if (SUCCEEDED(hr) && m_pEnumerator) {
        // Используем текущее устройство из AudioDeviceManager
        if (!m_currentDeviceId.isEmpty()) {
            hr = m_pEnumerator->GetDevice(m_currentDeviceId.toStdWString().c_str(), &m_pDevice);
        } else {
            // Если deviceId пустой, получаем устройство по умолчанию
            hr = m_pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_pDevice);
            if (SUCCEEDED(hr) && m_pDevice) {
                LPWSTR deviceId = nullptr;
                if (SUCCEEDED(m_pDevice->GetId(&deviceId))) {
                    m_currentDeviceId = QString::fromWCharArray(deviceId);
                    CoTaskMemFree(deviceId);
                }
            }
        }

        if (SUCCEEDED(hr) && m_pDevice) {
            hr = m_pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL,
                                    NULL, (void**)&m_pEndpointVolume);
            if (SUCCEEDED(hr) && m_pEndpointVolume) {
                float currentVolume = 0.0f;
                if (SUCCEEDED(m_pEndpointVolume->GetMasterVolumeLevelScalar(&currentVolume))) {
                    m_currentVolume = static_cast<int>(currentVolume * 100);
                    m_volumeSlider->blockSignals(true);
                    m_volumeSlider->setValue(m_currentVolume);
                    m_volumeSlider->blockSignals(false);
                    m_volumeValue->setText(QString("%1%").arg(m_currentVolume));
                }
            }

            // Activate session manager
            hr = m_pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                                    NULL, (void**)&m_pSessionManager);
        }
    }
#endif
}

#ifdef Q_OS_WIN
QString VolumeMixerWidget::getDeviceDisplayName(IMMDevice* pDevice)
{
    QString deviceName = "Unknown Device";

    if (!pDevice) return deviceName;

    IPropertyStore* pProps = nullptr;
    HRESULT hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);

    if (SUCCEEDED(hr) && pProps) {
        PROPVARIANT varName;
        PropVariantInit(&varName);

        // Use the explicitly defined PKEY_Device_FriendlyName
        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);

        if (SUCCEEDED(hr) && varName.vt == VT_LPWSTR) {
            deviceName = QString::fromWCharArray(varName.pwszVal);
        }

        PropVariantClear(&varName);
        pProps->Release();
    }

    return deviceName;
}

QString VolumeMixerWidget::getProcessName(DWORD processId, QString& processPath)
{
    QString processName = "Unknown";
    processPath.clear();

    if (processId == 0) {
        processPath = "System Sounds";
        return "System Sounds";
    }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess) {
        WCHAR buffer[MAX_PATH];
        DWORD size = MAX_PATH;
        if (GetModuleFileNameEx(hProcess, NULL, buffer, size)) {
            processPath = QString::fromWCharArray(buffer);
            // Extract just the executable name for display
            processName = processPath;
            int lastSlash = processName.lastIndexOf('\\');
            if (lastSlash != -1) {
                processName = processName.mid(lastSlash + 1);
            }
            // Remove extension
            int lastDot = processName.lastIndexOf('.');
            if (lastDot != -1) {
                processName = processName.left(lastDot);
            }
        }
        CloseHandle(hProcess);
    }

    return processName;
}

void VolumeMixerWidget::setupAudioSessions()
{
    if (!m_pSessionManager) return;

    IAudioSessionEnumerator* pSessionEnumerator = nullptr;
    HRESULT hr = m_pSessionManager->GetSessionEnumerator(&pSessionEnumerator);

    if (SUCCEEDED(hr) && pSessionEnumerator) {
        int sessionCount;
        pSessionEnumerator->GetCount(&sessionCount);

        for (int i = 0; i < sessionCount; i++) {
            IAudioSessionControl* pSessionControl = nullptr;
            hr = pSessionEnumerator->GetSession(i, &pSessionControl);

            if (SUCCEEDED(hr) && pSessionControl) {
                IAudioSessionControl2* pSessionControl2 = nullptr;
                hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);

                if (SUCCEEDED(hr) && pSessionControl2) {
                    DWORD processId;
                    pSessionControl2->GetProcessId(&processId);

                    QString processPath;
                    QString appName = getProcessName(processId, processPath);

                    // Only add if not already present and not a duplicate system sound
                    if (!m_appVolumeWidgets.contains(appName) && appName != "Unknown") {
                        ISimpleAudioVolume* pAudioVolume = nullptr;
                        hr = pSessionControl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pAudioVolume);

                        if (SUCCEEDED(hr) && pAudioVolume) {
                            float volumeLevel;
                            pAudioVolume->GetMasterVolume(&volumeLevel);
                            int volumePercent = static_cast<int>(volumeLevel * 100);

                            AppVolumeWidget* appWidget = new AppVolumeWidget(appName, processPath, volumePercent);
                            connect(appWidget, &AppVolumeWidget::volumeChanged, this, &VolumeMixerWidget::onAppVolumeChanged);
                            m_appsLayout->addWidget(appWidget);
                            m_appVolumeWidgets[appName] = appWidget;

                            pAudioVolume->Release();
                        }
                    }

                    pSessionControl2->Release();
                }
                pSessionControl->Release();
            }
        }
        pSessionEnumerator->Release();
    }
}
#endif

void VolumeMixerWidget::updateAudioSessions()
{
#ifdef Q_OS_WIN
    // Remove old widgets for sessions that no longer exist
    QList<QString> toRemove;
    for (auto it = m_appVolumeWidgets.begin(); it != m_appVolumeWidgets.end(); ++it) {
        bool found = false;

        if (m_pSessionManager) {
            IAudioSessionEnumerator* pSessionEnumerator = nullptr;
            HRESULT hr = m_pSessionManager->GetSessionEnumerator(&pSessionEnumerator);

            if (SUCCEEDED(hr) && pSessionEnumerator) {
                int sessionCount;
                pSessionEnumerator->GetCount(&sessionCount);

                for (int i = 0; i < sessionCount && !found; i++) {
                    IAudioSessionControl* pSessionControl = nullptr;
                    hr = pSessionEnumerator->GetSession(i, &pSessionControl);

                    if (SUCCEEDED(hr) && pSessionControl) {
                        IAudioSessionControl2* pSessionControl2 = nullptr;
                        hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);

                        if (SUCCEEDED(hr) && pSessionControl2) {
                            DWORD processId;
                            pSessionControl2->GetProcessId(&processId);
                            QString processPath;
                            QString currentAppName = getProcessName(processId, processPath);

                            if (currentAppName == it.key()) {
                                found = true;
                            }

                            pSessionControl2->Release();
                        }
                        pSessionControl->Release();
                    }
                }
                pSessionEnumerator->Release();
            }
        }

        if (!found) {
            toRemove.append(it.key());
        }
    }

    // Remove widgets for non-existent sessions
    for (const QString& appName : toRemove) {
        AppVolumeWidget* widget = m_appVolumeWidgets.take(appName);
        if (widget) {
            m_appsLayout->removeWidget(widget);
            widget->deleteLater();
        }
    }

    // Add new sessions
    setupAudioSessions();
#endif
}

void VolumeMixerWidget::updateAudioDevices()
{
    m_deviceComboBox->blockSignals(true);
    m_deviceComboBox->clear();

#ifdef Q_OS_WIN
    if (!m_pEnumerator) {
        CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
                        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                        (void**)&m_pEnumerator);
    }

    if (m_pEnumerator) {
        IMMDeviceCollection* pCollection = nullptr;
        HRESULT hr = m_pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);

        if (SUCCEEDED(hr) && pCollection) {
            UINT count;
            pCollection->GetCount(&count);

            int currentIndex = -1;
            for (UINT i = 0; i < count; i++) {
                IMMDevice* pDevice = nullptr;
                hr = pCollection->Item(i, &pDevice);

                if (SUCCEEDED(hr) && pDevice) {
                    QString deviceName = getDeviceDisplayName(pDevice);

                    LPWSTR deviceId = nullptr;
                    QString currentDeviceId;
                    if (SUCCEEDED(pDevice->GetId(&deviceId))) {
                        currentDeviceId = QString::fromWCharArray(deviceId);
                        CoTaskMemFree(deviceId);
                    }

                    m_deviceComboBox->addItem(deviceName, currentDeviceId);

                    if (currentDeviceId == m_currentDeviceId) {
                        currentIndex = i;
                    }
                    pDevice->Release();
                }
            }

            if (currentIndex >= 0) {
                m_deviceComboBox->setCurrentIndex(currentIndex);
            }
            pCollection->Release();
        }
    }
#endif

    if (m_deviceComboBox->count() == 0) {
        m_deviceComboBox->addItem("No audio devices");
        m_deviceComboBox->setEnabled(false);
    }

    m_deviceComboBox->blockSignals(false);
}

void VolumeMixerWidget::onVolumeSliderValueChanged(int value)
{
    m_currentVolume = value;
    m_volumeValue->setText(QString("%1%").arg(value));
    updateVolumeIcon(value);

    // Используем AudioDeviceManager для установки громкости
    AudioDeviceManager::instance()->setVolume(value);
}

void VolumeMixerWidget::setSystemVolume(int volume)
{
#ifdef Q_OS_WIN
    if (m_pEndpointVolume) {
        float fVolume = volume / 100.0f;
        m_pEndpointVolume->SetMasterVolumeLevelScalar(fVolume, NULL);
    }
#endif
}

void VolumeMixerWidget::updateVolumeIcon(int value)
{
    QString iconText;
    if (value == 0) {
        iconText = "🔇";
    } else if (value < 33) {
        iconText = "🔈";
    } else if (value < 66) {
        iconText = "🔉";
    } else {
        iconText = "🔊";
    }
    m_volumeIcon->setText(iconText);
}

void VolumeMixerWidget::onDeviceComboBoxChanged(int index)
{
    if (index >= 0) {
        QString deviceId = m_deviceComboBox->itemData(index).toString();

        if (deviceId == m_currentDeviceId) {
            return;
        }

        // Используем AudioDeviceManager для переключения устройства
        if (AudioDeviceManager::instance()->switchDevice(deviceId)) {
            m_currentDeviceId = deviceId;
        }
    }
}

void VolumeMixerWidget::onAppVolumeChanged(const QString& appName, int volume)
{
#ifdef Q_OS_WIN
    if (!m_pSessionManager) return;

    IAudioSessionEnumerator* pSessionEnumerator = nullptr;
    HRESULT hr = m_pSessionManager->GetSessionEnumerator(&pSessionEnumerator);

    if (SUCCEEDED(hr) && pSessionEnumerator) {
        int sessionCount;
        pSessionEnumerator->GetCount(&sessionCount);

        for (int i = 0; i < sessionCount; i++) {
            IAudioSessionControl* pSessionControl = nullptr;
            hr = pSessionEnumerator->GetSession(i, &pSessionControl);

            if (SUCCEEDED(hr) && pSessionControl) {
                IAudioSessionControl2* pSessionControl2 = nullptr;
                hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);

                if (SUCCEEDED(hr) && pSessionControl2) {
                    DWORD processId;
                    pSessionControl2->GetProcessId(&processId);
                    QString processPath;
                    QString currentAppName = getProcessName(processId, processPath);

                    if (currentAppName == appName) {
                        ISimpleAudioVolume* pAudioVolume = nullptr;
                        hr = pSessionControl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pAudioVolume);

                        if (SUCCEEDED(hr) && pAudioVolume) {
                            float fVolume = volume / 100.0f;
                            pAudioVolume->SetMasterVolume(fVolume, NULL);
                            pAudioVolume->Release();
                        }
                        break;
                    }

                    pSessionControl2->Release();
                }
                pSessionControl->Release();
            }
        }
        pSessionEnumerator->Release();
    }
#endif
}

void VolumeMixerWidget::onSystemVolumeChanged(int volume)
{
    if (volume != m_currentVolume) {
        m_currentVolume = volume;
        m_volumeSlider->blockSignals(true);
        m_volumeSlider->setValue(volume);
        m_volumeSlider->blockSignals(false);
        m_volumeValue->setText(QString("%1%").arg(volume));
        updateVolumeIcon(volume);
    }
}

void VolumeMixerWidget::onSystemDeviceChanged(const QString& deviceId)
{
    if (deviceId != m_currentDeviceId) {
        m_currentDeviceId = deviceId;

        // Обновляем выбранное устройство в комбобоксе
        int index = m_deviceComboBox->findData(deviceId);
        if (index >= 0) {
            m_deviceComboBox->blockSignals(true);
            m_deviceComboBox->setCurrentIndex(index);
            m_deviceComboBox->blockSignals(false);
        }

        // Обновляем сессии для нового устройства
        cleanupAudio();
        setupAudio();
        updateAudioSessions();
    }
}