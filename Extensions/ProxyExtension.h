#ifndef PROXYEXTENSION_H
#define PROXYEXTENSION_H

#include "..//ExtensionManager.h"
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSettings>
#include <QMenu>
#include <QMouseEvent>
#include <QProcess>
#include <QApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class ProxyWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProxyWidget(QWidget* parent = nullptr);
    ~ProxyWidget();

protected:
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void toggleProxy();
    void updateProxyStatus();
    void openProxySettings();
    void enableProxy();
    void disableProxy();
    void fetchCountryCode();
    void onCountryCodeReceived(QNetworkReply* reply);

private:
    void setProxyEnabled(bool enabled);
    bool isProxyEnabled();
    QString getCurrentProxy();
    void updateContextMenu();
    void tryNextService();

    QLabel* m_proxyLabel;
    QPushButton* m_toggleButton;
    QMenu* m_contextMenu;
    QAction* m_enableAction;
    QAction* m_disableAction;
    QAction* m_settingsAction;
    QAction* m_checkAction;
    bool m_proxyEnabled;
    QString m_currentProxy;
    QString m_countryCode;
    QNetworkAccessManager* m_networkManager;
    int m_currentService;
};

class ProxyExtension : public ExtensionInterface
{
public:
    ProxyExtension() = default;
    ~ProxyExtension() = default;

    QString name() const override { return "Proxy"; }
    QString version() const override { return "1.0"; }
    QWidget* createWidget(QWidget* parent = nullptr) override;

    // Указываем, что это расширение должно отображаться только в верхней панели
    ExtensionDisplayLocation displayLocation() const override {
        return ExtensionDisplayLocation::TopPanel;
    }
};

REGISTER_EXTENSION_CONDITIONAL(ProxyExtension)

#endif // PROXYEXTENSION_H