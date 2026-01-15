#include "ExtensionsMenu.h"
#include <QDebug>
#include <QFile>
#include <QScrollArea>
#include <QTextStream>
#include <QProcess>
#include <QApplication>
#include <QTimer>

ExtensionsMenu::ExtensionsMenu(QWidget* parent) : QMenu(parent), m_settings("MyCompany", "DockApp")
{
    setupUI();
    loadExtensionStates();

    setStyleSheet(QString(
        "QMenu {"
        "    background-color: %1;"
        "    border: 1px solid %2;"
        "    border-radius: 5px;"
        "    padding: 5px;"
        "}"
    ).arg(TopPanelConstants::PANEL_BACKGROUND_COLOR.name(),
          TopPanelConstants::PANEL_BORDER_COLOR.name()));
}

void ExtensionsMenu::setupUI()
{
    QWidget* menuContainer = new QWidget();
    m_mainLayout = new QVBoxLayout(menuContainer);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->setSpacing(8);

    // Заголовок и кнопка перезагрузки
    QHBoxLayout* headerLayout = new QHBoxLayout();

    QLabel* titleLabel = new QLabel("Управление расширениями");
    titleLabel->setStyleSheet(QString(
        "QLabel {"
        "    color: %1;"
        "    font-weight: bold;"
        "    font-size: 12px;"
        "    padding: 5px;"
        "}"
    ).arg(TopPanelConstants::TEXT_COLOR.name()));
    headerLayout->addWidget(titleLabel);

    // Кнопка перезагрузки расширений
    QPushButton* reloadButton = new QPushButton("⟳");
    reloadButton->setFixedSize(20, 20);
    reloadButton->setToolTip("Перезагрузить расширения");
    reloadButton->setStyleSheet(
        "QPushButton {"
        "    background: transparent;"
        "    border: 1px solid gray;"
        "    border-radius: 3px;"
        "    color: white;"
        "}"
        "QPushButton:hover {"
        "    background: rgba(255,255,255,0.1);"
        "}"
    );
    headerLayout->addWidget(reloadButton);
    headerLayout->addStretch();

    m_mainLayout->addLayout(headerLayout);

    // Подключаем кнопку перезагрузки
    connect(reloadButton, &QPushButton::clicked, this, [this]() {
        ExtensionManager::instance().reloadExtensions();
        refreshExtensionsList();
    });

    // Подключаем сигнал изменений расширений
    connect(&ExtensionManager::instance(), &ExtensionManager::extensionsChanged,
            this, &ExtensionsMenu::refreshExtensionsList);

    // Разделитель
    QFrame* separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setStyleSheet(QString("background-color: %1; margin: 5px 0px;")
        .arg(TopPanelConstants::PANEL_BORDER_COLOR.name()));
    m_mainLayout->addWidget(separator);

    // Создаем контейнер для расширений
    m_extensionsContainer = new QWidget();
    m_extensionsLayout = new QVBoxLayout(m_extensionsContainer);
    m_extensionsLayout->setContentsMargins(0, 0, 0, 0);
    m_extensionsLayout->setSpacing(5);

    // Добавляем контейнер в скроллируемую область
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidget(m_extensionsContainer);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFixedHeight(280); // Фиксируем высоту области с расширениями

    m_mainLayout->addWidget(scrollArea);

    // Первоначальная загрузка списка расширений
    refreshExtensionsList();

    // Заменяем подпись о необходимости перезапуска на виджет с кнопкой
    QWidget* restartContainer = new QWidget();
    QVBoxLayout* containerLayout = new QVBoxLayout(restartContainer);
    containerLayout->setContentsMargins(8, 8, 8, 8);
    containerLayout->setSpacing(2);

    // Первая строка текста
    QLabel* restartTextLabel1 = new QLabel("Для применения изменений");
    restartTextLabel1->setStyleSheet(QString(
        "QLabel {"
        "    color: %1;"
        "    font-size: 11px;"
        "    padding: 0;"
        "    background-color: transparent;"
        "}"
    ).arg(TopPanelConstants::TEXT_COLOR.name()));
    containerLayout->addWidget(restartTextLabel1);

    // Вторая строка с кнопкой
    QWidget* restartWidget = new QWidget();
    restartWidget->setStyleSheet("background-color: transparent;"); // Убираем фон у второй строки
    QHBoxLayout* restartLayout = new QHBoxLayout(restartWidget);
    restartLayout->setContentsMargins(0, 0, 0, 0);
    restartLayout->setSpacing(0);

    // Первая часть текста второй строки
    QLabel* restartTextLabel2 = new QLabel("требуется ");
    restartTextLabel2->setStyleSheet(QString(
        "QLabel {"
        "    color: %1;"
        "    font-size: 11px;"
        "    padding: 0;"
        "    background-color: transparent;"
        "}"
    ).arg(TopPanelConstants::TEXT_COLOR.name()));
    restartLayout->addWidget(restartTextLabel2);

    // Кнопка "перезапуск дока"
    QPushButton* restartButton = new QPushButton("перезапуск дока");
    restartButton->setStyleSheet(QString(
        "QPushButton {"
        "    color: %1;"
        "    font-size: 11px;"
        "    padding: 0;"
        "    border: none;"
        "    background-color: transparent;"
        "    text-decoration: none;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(255,255,255,0.2);"
        "    border-radius: 3px;"
        "}"
    ).arg(TopPanelConstants::TEXT_COLOR.name()));
    restartButton->setCursor(Qt::PointingHandCursor);
    restartLayout->addWidget(restartButton);

    restartLayout->addStretch();
    containerLayout->addWidget(restartWidget);

    restartContainer->setStyleSheet(
        "QWidget {"
        "    background-color: rgba(255,255,255,0.1);"
        "    border-radius: 3px;"
        "}"
    );
    restartContainer->setMaximumHeight(45);

    m_mainLayout->addWidget(restartContainer);

    // Подключаем кнопку перезапуска к слоту restartApplication
    connect(restartButton, &QPushButton::clicked, this, &ExtensionsMenu::restartApplication);

    QWidgetAction* widgetAction = new QWidgetAction(this);
    widgetAction->setDefaultWidget(menuContainer);
    addAction(widgetAction);

    // Увеличиваем высоту меню и устанавливаем минимальную высоту
    setFixedSize(250, 400);
    setMinimumHeight(400);
}

void ExtensionsMenu::refreshExtensionsList()
{
    // Очищаем текущий список
    QLayoutItem* item;
    while ((item = m_extensionsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_extensionSliders.clear();

    // Получаем ВСЕ доступные расширения из конфига
    QSet<QString> allExtensions = ExtensionManager::instance().getAllAvailableExtensions();
    qDebug() << "All available extensions:" << allExtensions;

    for (const QString& extension : allExtensions) {
        createExtensionItem(extension);
    }

    // Если расширений нет
    if (allExtensions.isEmpty()) {
        QLabel* noExtensionsLabel = new QLabel("Расширения не найдены");
        noExtensionsLabel->setStyleSheet(QString(
            "QLabel {"
            "    color: %1;"
            "    font-size: 11px;"
            "    padding: 10px;"
            "    text-align: center;"
            "}"
        ).arg(TopPanelConstants::TEXT_COLOR.name()));
        noExtensionsLabel->setAlignment(Qt::AlignCenter);
        m_extensionsLayout->addWidget(noExtensionsLabel);
    }
}

void ExtensionsMenu::createExtensionItem(const QString& extensionName)
{
    QWidget* extensionWidget = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(extensionWidget);
    layout->setContentsMargins(5, 2, 5, 2);
    layout->setSpacing(10);

    // Название расширения
    QLabel* nameLabel = new QLabel(extensionName);
    nameLabel->setStyleSheet(QString(
        "QLabel {"
        "    color: %1;"
        "    font-size: 11px;"
        "}"
    ).arg(TopPanelConstants::TEXT_COLOR.name()));
    nameLabel->setMinimumWidth(120);
    layout->addWidget(nameLabel);

    // Слайдер для включения/выключения
    QSlider* slider = new QSlider(Qt::Horizontal);
    slider->setMinimum(0);
    slider->setMaximum(1);
    slider->setFixedWidth(60);
    slider->setStyleSheet(QString(
        "QSlider::groove:horizontal {"
        "    border: 1px solid %1;"
        "    height: 4px;"
        "    background: %2;"
        "    border-radius: 2px;"
        "}"
        "QSlider::handle:horizontal {"
        "    background: %3;"
        "    border: 1px solid %1;"
        "    width: 12px;"
        "    margin: -6px 0;"
        "    border-radius: 6px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "    background: %3;"
        "    border-radius: 2px;"
        "}"
    ).arg(TopPanelConstants::PANEL_BORDER_COLOR.name(),
          TopPanelConstants::PANEL_BACKGROUND_COLOR.name(),
          TopPanelConstants::TEXT_COLOR.name()));

    // Устанавливаем начальное значение на основе того, включено ли расширение
    bool enabled = ExtensionManager::instance().isExtensionEnabled(extensionName);
    slider->setValue(enabled ? 1 : 0);
    m_extensionSliders[extensionName] = slider;

    layout->addWidget(slider);
    layout->addStretch();

    m_extensionsLayout->addWidget(extensionWidget);

    // Подключаем сигнал изменения положения слайдера
    connect(slider, &QSlider::valueChanged, this, [this, extensionName](int value) {
        onExtensionToggled(extensionName, value == 1);
    });
}

void ExtensionsMenu::loadExtensionStates()
{
    // Загружаем состояния расширений из настроек
    QSet<QString> allExtensions = ExtensionManager::instance().getAllAvailableExtensions();
    for (const QString& extension : allExtensions) {
        bool enabled = ExtensionManager::instance().isExtensionEnabled(extension);
        QSlider* slider = m_extensionSliders.value(extension);
        if (slider) {
            slider->setValue(enabled ? 1 : 0);
        }
    }
}

void ExtensionsMenu::saveExtensionState(const QString& extensionName, bool enabled)
{
    m_settings.setValue(QString("Extensions/%1").arg(extensionName), enabled);
    qDebug() << "Saved extension state:" << extensionName << "enabled:" << enabled;
}

void ExtensionsMenu::updateExtensionsConfigFile(const QString& extensionName, bool enabled)
{
    QFile file("extensions.txt");
    if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        qWarning() << "Failed to open extensions.txt for writing";
        return;
    }

    QTextStream in(&file);
    QStringList lines;
    bool extensionFound = false;

    // Читаем все строки файла
    while (!in.atEnd()) {
        QString line = in.readLine();
        QString cleanLine = line.trimmed();

        // Пропускаем строки с "🤡" (заголовки разделов)
        if (cleanLine.startsWith("🤡")) {
            lines.append(line);
            continue;
        }

        // Ищем строку с нашим расширением (игнорируем комментарии в поиске)
        if (cleanLine.startsWith(extensionName + "=") ||
            (cleanLine.startsWith("#") && cleanLine.mid(1).trimmed().startsWith(extensionName + "=")) ||
            cleanLine == extensionName ||
            (cleanLine.startsWith("#") && cleanLine.mid(1).trimmed() == extensionName)) {

            extensionFound = true;

            if (enabled) {
                // Включаем расширение - убираем комментарий если есть
                if (cleanLine.startsWith("#")) {
                    line = line.mid(line.indexOf("#") + 1).trimmed();
                }
            } else {
                // Выключаем расширение - добавляем комментарий
                if (!cleanLine.startsWith("#")) {
                    line = "#" + line;
                }
            }
        }
        lines.append(line);
    }

    // Если расширение не найдено, добавляем его
    if (!extensionFound) {
        QString newLine = extensionName + "=Extensions/" + extensionName + ".cpp,Extensions/" + extensionName + ".h";
        if (!enabled) {
            newLine = "#" + newLine;
        }
        lines.append(newLine);
    }

    // Перезаписываем файл
    file.resize(0);
    for (const QString& line : lines) {
        in << line << "\n";
    }

    file.close();
    qDebug() << "Updated extensions.txt for extension:" << extensionName << "enabled:" << enabled;
}

void ExtensionsMenu::onExtensionToggled(const QString& extensionName, bool enabled)
{
    // Сохраняем состояние в настройки
    saveExtensionState(extensionName, enabled);

    // Обновляем конфигурационный файл
    updateExtensionsConfigFile(extensionName, enabled);

    // Отправляем сигнал о изменении состояния расширения
    emit extensionToggled(extensionName, enabled);

    qDebug() << "Extension toggled in menu:" << extensionName << "enabled:" << enabled;
}

void ExtensionsMenu::restartApplication()
{
    qDebug() << "Restarting application...";

    // Получаем путь к текущему исполняемому файлу
    QString program = QApplication::applicationFilePath();
    QStringList arguments = QApplication::arguments();

    if (arguments.count() > 0) {
        arguments.removeFirst(); // Убираем имя программы из аргументов
    }

    // Запускаем новый экземпляр приложения
    QProcess::startDetached(program, arguments);

    // Закрываем текущее приложение
    QTimer::singleShot(100, []() {
        QApplication::quit();
    });
}