#ifndef SEARCHWINDOW_H
#define SEARCHWINDOW_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLabel>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QKeyEvent>
#include <QDebug>
#include <QString>
#include <QRegularExpression>
#include <QSettings>
#include <cmath>
#include <vector>
#include <stack>
#include <QApplication>
#include <QPropertyAnimation>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QCheckBox>
#include <QMenu>
#include <QAction>

#include "SearchResult.h"
#include "ApplicationSearcher.h"

class SearchWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SearchWindow(QWidget *parent = nullptr);
    ~SearchWindow();

    void showAtScreen(QScreen *screen);
    void activateSearch();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onSearchTextChanged(const QString &text);
    void onResultItemClicked(QListWidgetItem *item);
    void hideAndClose();
    void updateResults();
    void onSettingsClicked();
    void onAddPathClicked();
    void onRemovePathClicked();
    void onRemoveAllPathsClicked();
    void onCloseSettingsClicked();
    void onOpenFileLocation();

private:
    void setupUI();
    void setupSettingsUI();
    void calculatePosition(QScreen *screen);
    void addSearchResult(const SearchResult &result);
    void clearResults();
    bool isMathExpression(const QString &text);
    QString calculateMathExpression(const QString &expression);
    double eval(double a, double b, char op);
    int precedence(char op);
    bool isOperator(char c);
    void saveCustomSearchPaths();
    void loadCustomSearchPaths();
    void refreshSearchPaths();
    void showSettingsPanel();
    void hideSettingsPanel();
    void refreshSettingsList();
    bool isPathInList(const QString &path);
    void saveUseExplorerSetting();
    void loadUseExplorerSetting();
    void showContextMenuForItem(const QPoint &pos);

    bool isSearchEnabled();

    // Основные виджеты
    QWidget *m_mainWidget;
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_searchLayout;
    QLineEdit *m_searchEdit;
    QPushButton *m_webSearchButton;
    QPushButton *m_pcSearchButton;
    QPushButton *m_settingsButton;
    QListWidget *m_resultsList;
    QLabel *m_statusLabel;
    QGraphicsDropShadowEffect *m_shadowEffect;

    // Виджеты для настроек (отдельное окно)
    QWidget *m_settingsWidget;
    QCheckBox *m_useExplorerCheckBox;
    QListWidget *m_pathsList;
    QPushButton *m_addPathButton;
    QPushButton *m_removePathButton;
    QPushButton *m_removeAllPathsButton;
    QPushButton *m_closeSettingsButton;

    // Контекстное меню
    QMenu *m_contextMenu;
    QAction *m_openLocationAction;
    QString m_contextMenuPath;

    ApplicationSearcher *m_appSearcher;
    QTimer *m_searchTimer;

    QSize m_originalWindowSize;
    QString m_originalSearchEditText;
    bool m_wereResultsVisible = false;

    QStringList m_customSearchPaths;
    bool m_isClosing = false;
    bool m_showResults = true;
    bool m_isSettingsVisible = false;
    bool m_useExplorer = true;
};

#endif // SEARCHWINDOW_H