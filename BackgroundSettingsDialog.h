#ifndef BACKGROUNDSETTINGSDIALOG_H
#define BACKGROUNDSETTINGSDIALOG_H

#include <QDialog>
#include <QScreen>


class MultiDesktopManager;

class BackgroundSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BackgroundSettingsDialog(MultiDesktopManager* manager, QWidget* parent = nullptr);
    ~BackgroundSettingsDialog();

    void setCurrentScreen(QScreen* screen);

private slots:
    void changeBackground();
    void clearBackground();
    void applySettings();
    void applyToAllScreens();
    void onScreenSelectionChanged(int index);
    void onScalingTypeChanged(int index);
    void updateVideoOptionsVisibility();
    void restartDock();

private:
    void setupUI();
    void loadCurrentScreenSettings();
    void updateScalingOptions();

    MultiDesktopManager* m_manager;
    QScreen* m_currentScreen;

    class QComboBox* m_screenCombo;
    class QComboBox* m_alignmentCombo;
    class QComboBox* m_scalingTypeCombo;
    class QComboBox* m_scaleFactorCombo;
    class QPushButton* m_changeButton;
    class QPushButton* m_clearButton;
    class QPushButton* m_applyButton;
    class QPushButton* m_applyToAllButton;
    class QPushButton* m_restartDockButton;

    // Новые элементы для видео
    class QCheckBox* m_loopVideoCheckBox;
    class QCheckBox* m_enableSoundCheckBox;
    class QGroupBox* m_videoOptionsGroup;
};

#endif // BACKGROUNDSETTINGSDIALOG_H