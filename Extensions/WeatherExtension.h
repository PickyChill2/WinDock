#ifndef WEATHEREXTENSION_H
#define WEATHEREXTENSION_H

#include "../ExtensionManager.h"
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMenu>
#include <QSettings>
#include <QMouseEvent>
#include <QFile>
#include <QTextStream>

// Настройки внешнего вида
namespace WeatherSettings {
    const int PANEL_FONT_SIZE = 12;           // Размер шрифта в верхней панели
    const int MENU_FONT_SIZE = 13;            // Уменьшен размер шрифта во всплывающем меню (было 14)
    const QString FONT_FAMILY = "segoe ui";   // Шрифт
    const int MENU_WIDTH = 320;               // Ширина всплывающего меню
    const int MENU_ITEM_PADDING = 12;         // Внутренние отступы элементов меню
    const int MENU_BORDER_RADIUS = 15;        // Скругление углов меню
    const int MENU_ITEM_BORDER_RADIUS = 8;    // Скругление углов элементов меню

    // Цвета
    const QString PANEL_TEXT_COLOR = "white";                    // Цвет текста в верхней панели
    const QString MENU_BACKGROUND_COLOR = "#2b2b2b";            // Цвет фона меню
    const QString MENU_TEXT_COLOR = "white";                    // Цвет текста в меню
    const QString MENU_BORDER_COLOR = "#555";                   // Цвет границы меню
    const QString MENU_SELECTED_BACKGROUND_COLOR = "#3a3a3a";   // Цвет фона выбранного элемента
    const QString DAY_OF_WEEK_COLOR = "#ff4646";                // Цвет дня недели (красный)
}

class WeatherWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WeatherWidget(QWidget* parent = nullptr);
    ~WeatherWidget();

protected:
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void updateWeather();
    void onWeatherDataReceived(QNetworkReply* reply);
    void onForecastDataReceived(QNetworkReply* reply);
    void selectCity();

private:
    void loadSettings();
    void saveSettings();
    void updateWeatherDisplay(const QString& cityName, const QString& temperature, const QString& description, const QString& icon);
    void showForecastMenu();
    void updateForecast();
    QString translateCityToEnglish(const QString& russianCity);
    QString getSettingsFilePath();
    QString getWeatherIcon(const QString& iconCode);
    QString getRussianDayOfWeek(const QDate& date);

private:
    QLabel* m_weatherLabel;
    QTimer* m_updateTimer;
    QNetworkAccessManager* m_networkManager;
    QMenu* m_contextMenu;
    QMenu* m_forecastMenu;
    QString m_city;
    QString m_apiKey;
    QList<QVariantMap> m_forecastData;
};

class WeatherExtension : public ExtensionInterface
{
public:
    WeatherExtension() = default;
    ~WeatherExtension() = default;

    QString name() const override { return "Weather"; }
    QString version() const override { return "1.0"; }
    QWidget* createWidget(QWidget* parent = nullptr) override;

    // Указываем, что это расширение должно отображаться только в верхней панели
    ExtensionDisplayLocation displayLocation() const override {
        return ExtensionDisplayLocation::TopPanel;
    }
};

// Регистрация расширения
REGISTER_EXTENSION_CONDITIONAL(WeatherExtension)

#endif // WEATHEREXTENSION_H