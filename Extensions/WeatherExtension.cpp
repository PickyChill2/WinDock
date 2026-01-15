#include "WeatherExtension.h"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QInputDialog>
#include <QUrlQuery>
#include <QDir>
#include <QDebug>
#include <QVBoxLayout>
#include <QAction>
#include <QLocale>
#include <QWidgetAction>
#include <QLabel>
#include "../TopPanelConstants.h"

WeatherWidget::WeatherWidget(QWidget* parent)
    : QWidget(parent)
    , m_weatherLabel(nullptr)
    , m_updateTimer(nullptr)
    , m_networkManager(nullptr)
    , m_contextMenu(nullptr)
    , m_forecastMenu(nullptr)
{
    // Устанавливаем фиксированную высоту, соответствующую высоте панели
    setFixedHeight(TopPanelConstants::PANEL_HEIGHT);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 0, 0);
    layout->setSpacing(5);
    layout->setAlignment(Qt::AlignVCenter);

    m_weatherLabel = new QLabel(this);
    // Используем настройки из WeatherExtension.h
    m_weatherLabel->setStyleSheet(
        QString("color: %1; "
                "font-size: %2px; "
                "font-family: '%3'; "
                "background: transparent; "
                "padding: 0px; "
                "margin: 0px;")
            .arg(WeatherSettings::PANEL_TEXT_COLOR)
            .arg(WeatherSettings::PANEL_FONT_SIZE)
            .arg(WeatherSettings::FONT_FAMILY)
    );
    m_weatherLabel->setText("Загрузка погоды...");
    m_weatherLabel->setAlignment(Qt::AlignVCenter);
    m_weatherLabel->setMinimumWidth(200);

    layout->addWidget(m_weatherLabel);

    // Инициализация сетевого менеджера
    m_networkManager = new QNetworkAccessManager(this);

    // Загрузка настроек
    loadSettings();

    // Таймер для обновления погоды каждые 10 минут
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &WeatherWidget::updateWeather);
    m_updateTimer->start(600000); // 10 минут

    // Контекстное меню (правая кнопка)
    m_contextMenu = new QMenu(this);
    m_contextMenu->setStyleSheet(
    QString("QMenu {"
            "   background-color: %1;"
            "   color: %2;"
            "   border: 2px solid %3;"
            "   border-radius: %4px;"
            "   font-size: %5px;"
            "   font-family: '%6';"
            "   font-weight: bold;"
            "   padding: 10px;"
            "}"
            "QMenu::item {"
            "   padding: %7px 20px;"
            "   border-radius: %8px;"
            "   font-size: %5px;"
            "   font-family: '%6';"
            "   font-weight: bold;"
            "   margin: 2px;"
            "}"
            "QMenu::item:selected {"
            "   background-color: %9;"
            "}")
        .arg(WeatherSettings::MENU_BACKGROUND_COLOR)
        .arg(WeatherSettings::MENU_TEXT_COLOR)
        .arg(WeatherSettings::MENU_BORDER_COLOR)
        .arg(WeatherSettings::MENU_BORDER_RADIUS)
        .arg(WeatherSettings::MENU_FONT_SIZE)
        .arg(WeatherSettings::FONT_FAMILY)
        .arg(WeatherSettings::MENU_ITEM_PADDING)
        .arg(WeatherSettings::MENU_ITEM_BORDER_RADIUS)
        .arg(WeatherSettings::MENU_SELECTED_BACKGROUND_COLOR)
);

    QAction* selectCityAction = new QAction("🏙️ Выбрать город", this);
    connect(selectCityAction, &QAction::triggered, this, &WeatherWidget::selectCity);
    m_contextMenu->addAction(selectCityAction);

    QAction* refreshAction = new QAction("🔄 Обновить погоду", this);
    connect(refreshAction, &QAction::triggered, this, &WeatherWidget::updateWeather);
    m_contextMenu->addAction(refreshAction);

    // Меню прогноза (левая кнопка)
    m_forecastMenu = new QMenu(this);
    m_forecastMenu->setStyleSheet(
    QString("QMenu {"
            "   background-color: %1;"
            "   color: %2;"
            "   border: 2px solid %3;"
            "   border-radius: %4px;"
            "   font-size: %5px;"
            "   font-family: '%6';"
            "   font-weight: bold;"
            "   padding: 10px;"
            "}"
            "QMenu::item {"
            "   padding: 0px;"
            "   margin: 0px;"
            "   background: transparent;"
            "}"
            "QMenu::item:selected {"
            "   background-color: %7;"
            "}"
            "QMenu::separator {"
            "   height: 2px;"
            "   background: %3;"
            "   margin: 8px 10px;"
            "   border-radius: 1px;"
            "}")
        .arg(WeatherSettings::MENU_BACKGROUND_COLOR)
        .arg(WeatherSettings::MENU_TEXT_COLOR)
        .arg(WeatherSettings::MENU_BORDER_COLOR)
        .arg(WeatherSettings::MENU_BORDER_RADIUS)
        .arg(WeatherSettings::MENU_FONT_SIZE)
        .arg(WeatherSettings::FONT_FAMILY)
        .arg(WeatherSettings::MENU_SELECTED_BACKGROUND_COLOR)
);

    // Первое обновление
    QTimer::singleShot(100, this, &WeatherWidget::updateWeather);
}

WeatherWidget::~WeatherWidget()
{
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
}

QString WeatherWidget::getSettingsFilePath()
{
    QDir extensionsDir(QCoreApplication::applicationDirPath());

    #ifdef QT_DEBUG
        if (extensionsDir.dirName() == "cmake-build-debug") {
            extensionsDir.cdUp();
        }
    #endif

    extensionsDir.cd("extensions");
    return extensionsDir.absoluteFilePath("city.txt");
}

void WeatherWidget::loadSettings()
{
    QString filePath = getSettingsFilePath();
    QFile file(filePath);

    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        m_city = in.readLine().trimmed();
        file.close();

        if (m_city.isEmpty()) {
            m_city = "Moscow";
        }
    } else {
        m_city = "Moscow";
        saveSettings();
    }
}

void WeatherWidget::saveSettings()
{
    QString filePath = getSettingsFilePath();
    QFile file(filePath);

    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << m_city;
        file.close();
    }
}

void WeatherWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        showForecastMenu();
    } else if (event->button() == Qt::RightButton) {
        m_contextMenu->exec(event->globalPosition().toPoint());
    }
    QWidget::mousePressEvent(event);
}

void WeatherWidget::updateWeather()
{
    if (m_city.isEmpty()) {
        m_weatherLabel->setText("🏙️ Город не выбран");
        return;
    }

    m_apiKey = "???";

    if (m_apiKey == "YOUR_API_KEY_HERE") {
        m_weatherLabel->setText("🔑 Установите API ключ");
        return;
    }

    // Запрос текущей погоды
    QUrl currentUrl("http://api.openweathermap.org/data/2.5/weather");
    QUrlQuery currentQuery;
    currentQuery.addQueryItem("q", m_city);
    currentQuery.addQueryItem("appid", m_apiKey);
    currentQuery.addQueryItem("units", "metric");
    currentQuery.addQueryItem("lang", "ru");
    currentUrl.setQuery(currentQuery);

    QNetworkRequest currentRequest;
    currentRequest.setUrl(currentUrl);
    QNetworkReply* currentReply = m_networkManager->get(currentRequest);
    connect(currentReply, &QNetworkReply::finished, this, [this, currentReply]() {
        this->onWeatherDataReceived(currentReply);
    });

    // Запрос прогноза на 5 дней
    updateForecast();
}

void WeatherWidget::updateForecast()
{
    QUrl forecastUrl("http://api.openweathermap.org/data/2.5/forecast");
    QUrlQuery forecastQuery;
    forecastQuery.addQueryItem("q", m_city);
    forecastQuery.addQueryItem("appid", m_apiKey);
    forecastQuery.addQueryItem("units", "metric");
    forecastQuery.addQueryItem("cnt", "40"); // 5 дней * 8 интервалов в день
    forecastUrl.setQuery(forecastQuery);

    QNetworkRequest forecastRequest;
    forecastRequest.setUrl(forecastUrl);
    QNetworkReply* forecastReply = m_networkManager->get(forecastRequest);
    connect(forecastReply, &QNetworkReply::finished, this, [this, forecastReply]() {
        this->onForecastDataReceived(forecastReply);
    });
}

void WeatherWidget::onWeatherDataReceived(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();

        if (obj.contains("cod")) {
            int code = obj["cod"].toInt();

            if (code == 200) {
                if (obj.contains("main") && obj.contains("weather") && obj.contains("name")) {
                    QJsonObject main = obj["main"].toObject();
                    QJsonArray weatherArray = obj["weather"].toArray();
                    QJsonObject weather = weatherArray[0].toObject();

                    QString temperature = QString::number(main["temp"].toDouble(), 'f', 1);
                    QString description = weather["description"].toString();
                    QString icon = weather["icon"].toString();
                    QString cityName = obj["name"].toString();

                    updateWeatherDisplay(cityName, temperature, description, icon);
                } else {
                    m_weatherLabel->setText("❌ Ошибка: неполные данные");
                }
            } else {
                QString errorMessage = obj.contains("message") ?
                    obj["message"].toString() : "Неизвестная ошибка";
                m_weatherLabel->setText("❌ Ошибка: " + errorMessage);

                if (code == 404) {
                    QTimer::singleShot(2000, this, &WeatherWidget::selectCity);
                }
            }
        } else {
            m_weatherLabel->setText("❌ Ошибка: неверный формат ответа");
        }
    } else {
        m_weatherLabel->setText("❌ Ошибка сети: " + reply->errorString());
    }

    reply->deleteLater();
}

void WeatherWidget::onForecastDataReceived(QNetworkReply* reply)
{
    m_forecastData.clear();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();

        if (obj.contains("cod") && obj["cod"].toString() == "200") {
            QJsonArray list = obj["list"].toArray();

            // Используем QMap с QDate для правильной сортировки
            QMap<QDate, QVariantMap> dailyData;

            for (const QJsonValue& value : list) {
                QJsonObject item = value.toObject();
                QString dt_txt = item["dt_txt"].toString();
                QDateTime dt = QDateTime::fromString(dt_txt, "yyyy-MM-dd HH:mm:ss");
                QDate date = dt.date();
                QString displayDate = date.toString("dd.MM");
                QString dayOfWeek = getRussianDayOfWeek(date);

                // Определяем тип периода (день/ночь)
                QString period = (dt.time().hour() >= 6 && dt.time().hour() < 18) ? "day" : "night";

                if (!dailyData.contains(date)) {
                    QVariantMap dayData;
                    dayData["date"] = displayDate;
                    dayData["dayOfWeek"] = dayOfWeek;
                    dayData["sortDate"] = date; // Сохраняем для сортировки
                    dailyData[date] = dayData;
                }

                QVariantMap& dayData = dailyData[date];
                QJsonObject main = item["main"].toObject();
                QJsonArray weatherArray = item["weather"].toArray();
                QJsonObject weather = weatherArray[0].toObject();

                // Сохраняем данные для периода
                if (period == "day") {
                    dayData["day_temp"] = main["temp"].toDouble();
                    dayData["day_icon"] = weather["icon"].toString();
                    dayData["day_desc"] = weather["description"].toString();
                } else {
                    // Для ночи берем минимальную температуру
                    double nightTemp = main["temp"].toDouble();
                    if (!dayData.contains("night_temp") || nightTemp < dayData["night_temp"].toDouble()) {
                        dayData["night_temp"] = nightTemp;
                        dayData["night_icon"] = weather["icon"].toString();
                        dayData["night_desc"] = weather["description"].toString();
                    }
                }
            }

            // Преобразуем в список (уже отсортированный по дате)
            for (const QVariantMap& dayData : dailyData) {
                m_forecastData.append(dayData);
            }
        }
    }

    reply->deleteLater();
}

void WeatherWidget::showForecastMenu()
{
    if (m_forecastData.isEmpty()) {
        m_forecastMenu->clear();
        QAction* noDataAction = new QAction("🔄 Загрузка прогноза...", this);
        noDataAction->setEnabled(false);
        m_forecastMenu->addAction(noDataAction);
    } else {
        m_forecastMenu->clear();

        // Добавляем заголовок
        QAction* headerAction = new QAction("📅 Прогноз на 5 дней", this);
        headerAction->setEnabled(false);
        m_forecastMenu->addAction(headerAction);
        m_forecastMenu->addSeparator();

        // Добавляем прогноз для каждого дня с использованием QWidgetAction для цветного текста
        for (const QVariantMap& dayData : m_forecastData) {
            QString date = dayData["date"].toString();
            QString dayOfWeek = dayData["dayOfWeek"].toString();

            // Формируем данные для дня и ночи
            QString dayPart, nightPart;

            // Данные для дня
            if (dayData.contains("day_temp") && dayData["day_temp"].toString() != "N/A") {
                QString dayTemp = QString::number(dayData["day_temp"].toDouble(), 'f', 0);
                QString dayIcon = getWeatherIcon(dayData["day_icon"].toString());
                dayPart = QString("Днём: %1°C %2").arg(dayTemp).arg(dayIcon);
            } else {
                dayPart = "Днём: -";
            }

            // Данные для ночи
            if (dayData.contains("night_temp") && dayData["night_temp"].toString() != "N/A") {
                QString nightTemp = QString::number(dayData["night_temp"].toDouble(), 'f', 0);
                QString nightIcon = getWeatherIcon(dayData["night_icon"].toString());
                nightPart = QString("Ночью: %1°C %2").arg(nightTemp).arg(nightIcon);
            } else {
                nightPart = "Ночью: -";
            }

            // Создаем QLabel с HTML для цветного отображения дня недели
            QLabel* label = new QLabel();
            label->setStyleSheet(QString("background: transparent; color: %1; font-size: %2px; font-family: '%3'; font-weight: bold;")
                .arg(WeatherSettings::MENU_TEXT_COLOR)
                .arg(WeatherSettings::MENU_FONT_SIZE)
                .arg(WeatherSettings::FONT_FAMILY));

            // HTML с явным указанием жирного шрифта для всех элементов
            QString htmlText = QString("<span style='color: %1; font-weight: bold;'>%2</span>/"
                                       "<span style='color: %3; font-weight: bold;'>%4</span>: "
                                       "<span style='color: %1; font-weight: bold;'>%5</span> / "
                                       "<span style='color: %1; font-weight: bold;'>%6</span>")
                                 .arg(WeatherSettings::MENU_TEXT_COLOR)
                                 .arg(date)
                                 .arg(WeatherSettings::DAY_OF_WEEK_COLOR)
                                 .arg(dayOfWeek)
                                 .arg(dayPart)
                                 .arg(nightPart);

            label->setText(htmlText);
            label->setMargin(12);

            // Создаем QWidgetAction и устанавливаем в него QLabel
            QWidgetAction* forecastAction = new QWidgetAction(this);
            forecastAction->setDefaultWidget(label);
            forecastAction->setEnabled(false);
            m_forecastMenu->addAction(forecastAction);
        }
    }

    // Используем настройку ширины меню из WeatherExtension.h
    m_forecastMenu->setFixedWidth(WeatherSettings::MENU_WIDTH);

    // Показываем меню под виджетом погоды
    m_forecastMenu->exec(mapToGlobal(QPoint(0, height() + 5)));
}

QString WeatherWidget::getRussianDayOfWeek(const QDate& date)
{
    int dayOfWeek = date.dayOfWeek();

    switch (dayOfWeek) {
        case 1: return "Пн";
        case 2: return "Вт";
        case 3: return "Ср";
        case 4: return "Чт";
        case 5: return "Пт";
        case 6: return "Сб";
        case 7: return "Вс";
        default: return "";
    }
}

QString WeatherWidget::getWeatherIcon(const QString& iconCode)
{
    QHash<QString, QString> weatherIcons = {
        {"01d", "☀️"}, {"01n", "🌙"}, {"02d", "⛅"}, {"02n", "⛅"},
        {"03d", "☁️"}, {"03n", "☁️"}, {"04d", "☁️"}, {"04n", "☁️"},
        {"09d", "🌧️"}, {"09n", "🌧️"}, {"10d", "🌦️"}, {"10n", "🌦️"},
        {"11d", "⛈️"}, {"11n", "⛈️"}, {"13d", "❄️"}, {"13n", "❄️"},
        {"50d", "🌫️"}, {"50n", "🌫️"}
    };

    return weatherIcons.value(iconCode, "🌍");
}

void WeatherWidget::selectCity()
{
    bool ok;
    QString text = QInputDialog::getText(this, tr("Выбор города"),
                                         tr("Введите название города:"),
                                         QLineEdit::Normal, m_city, &ok);
    if (ok && !text.isEmpty()) {
        m_city = translateCityToEnglish(text);
        saveSettings();
        updateWeather();
    }
}

QString WeatherWidget::translateCityToEnglish(const QString& russianCity)
{
    QHash<QString, QString> cityTranslations = {
        {"Москва", "Moscow"},
        {"Санкт-Петербург", "Saint Petersburg"},
        {"Новосибирск", "Novosibirsk"},
        {"Екатеринбург", "Yekaterinburg"},
        {"Казань", "Kazan"},
        {"Нижний Новгород", "Nizhny Novgorod"},
        {"Челябинск", "Chelyabinsk"},
        {"Самара", "Samara"},
        {"Омск", "Omsk"},
        {"Ростов-на-Дону", "Rostov-on-Don"},
        {"Уфа", "Ufa"},
        {"Красноярск", "Krasnoyarsk"},
        {"Воронеж", "Voronezh"},
        {"Пермь", "Perm"},
        {"Волгоград", "Volgograd"},
        {"Ижевск", "Izhevsk"}
    };

    return cityTranslations.value(russianCity, russianCity);
}

void WeatherWidget::updateWeatherDisplay(const QString& cityName, const QString& temperature,
                                       const QString& description, const QString& icon)
{
    QString iconEmoji = getWeatherIcon(icon);
    QString weatherText = QString("%1: %2°C %3 %4")
                             .arg(cityName)
                             .arg(temperature)
                             .arg(iconEmoji)
                             .arg(description);

    m_weatherLabel->setText(weatherText);

    QFontMetrics metrics(m_weatherLabel->font());
    int textWidth = metrics.horizontalAdvance(weatherText) + 20;
    m_weatherLabel->setMinimumWidth(textWidth);
}

QWidget* WeatherExtension::createWidget(QWidget* parent)
{
    return new WeatherWidget(parent);

}
