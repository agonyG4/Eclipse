#pragma once

#include "core/SpotlightResultsModel.hpp"
#include "platform/rust/RustSpotlightBackend.hpp"
#include "platform/runtime/SpotlightRuntimePaths.hpp"
#include "services/ApplicationLauncher.hpp"
#include "apps/DesktopEntryCatalog.hpp"

#include <QObject>
#include <QTimer>
#include <QProcess>
#include <QPointer>
#include <QDateTime>
#include <QUrl>
#include <QJsonObject>
#include <memory>

class SpotlightController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)
    Q_PROPERTY(bool surfaceVisible READ surfaceVisible NOTIFY surfaceVisibleChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(SpotlightResultsModel *resultsModel READ resultsModel CONSTANT)
    Q_PROPERTY(int resultCount READ resultCount NOTIFY resultCountChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily CONSTANT)
    Q_PROPERTY(bool weatherEnabled READ weatherEnabled NOTIFY weatherEnabledChanged)
    Q_PROPERTY(bool weatherReady READ weatherReady NOTIFY weatherChanged)
    Q_PROPERTY(bool weatherLoading READ weatherLoading NOTIFY weatherChanged)
    Q_PROPERTY(int weatherTemp READ weatherTemp NOTIFY weatherChanged)
    Q_PROPERTY(QString weatherCity READ weatherCity NOTIFY weatherChanged)
    Q_PROPERTY(QString weatherCondition READ weatherCondition NOTIFY weatherChanged)
    Q_PROPERTY(QString weatherStatusText READ weatherStatusText NOTIFY weatherChanged)
    Q_PROPERTY(QUrl weatherIconSource READ weatherIconSource NOTIFY weatherChanged)

public:
    explicit SpotlightController(const SpotlightRuntimePaths &paths, QObject *parent = nullptr);
    SpotlightController(const SpotlightRuntimePaths &paths, DesktopEntryCatalog *catalog,
                        ApplicationLauncher *launcher, QObject *parent = nullptr);

    bool init(const QString &locale = QStringLiteral("en_US"), QString *errorOut = nullptr);
    bool setLocale(const QString &locale, QString *errorOut = nullptr);

    bool isOpen() const { return m_open; }
    bool surfaceVisible() const { return m_surfaceVisible; }
    QString query() const { return m_query; }
    SpotlightResultsModel *resultsModel() { return &m_results; }
    int resultCount() const { return m_results.resultCount(); }
    int selectedIndex() const { return m_selectedIndex; }
    QString fontFamily() const { return QStringLiteral("SF Pro Display"); }
    bool weatherEnabled() const { return m_weatherEnabled; }
    bool weatherReady() const { return m_weatherReady; }
    bool weatherLoading() const { return m_weatherLoading; }
    DesktopEntryCatalog *catalog() const { return m_catalog; }
    ApplicationLauncher *launcher() const { return m_launcher; }
    int weatherTemp() const { return m_weatherTemp; }
    QString weatherCity() const { return m_weatherCity; }
    QString weatherCondition() const { return m_weatherCondition; }
    QString weatherStatusText() const { return m_weatherStatusText; }
    QUrl weatherIconSource() const { return m_weatherIconSource; }

    Q_INVOKABLE void show();
    Q_INVOKABLE void close();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void setQuery(const QString &q);
    Q_INVOKABLE void scheduleSearch(const QString &q);
    Q_INVOKABLE void setSelectedIndex(int index);
    Q_INVOKABLE void moveSelection(int delta);
    Q_INVOKABLE void activateCurrent();
    Q_INVOKABLE void launch(int row);
    Q_INVOKABLE void reloadIndex();
    Q_INVOKABLE void refreshWeather();
    Q_INVOKABLE void ensureConfig();
    QJsonArray watchedDirectories();

    void setGameModeActive(bool active);
    void setComponentEnabled(bool enabled);
    void setWeatherEnabled(bool enabled);
    void applyConfig(const QJsonObject &config);

signals:
    void openChanged();
    void surfaceVisibleChanged();
    void queryChanged();
    void resultCountChanged();
    void selectedIndexChanged();
    void weatherEnabledChanged();
    void weatherChanged();
    void focusRequested();
    void launchFailed(const QString &error);

private slots:
    void runSearch();
    void onLaunchSucceeded(const QString &desktopId);
    void onLaunchFailed(const QString &desktopId, const QString &error);

private:
    void applyWeather(const QJsonObject &data);
    void applyWeatherError(const QString &error);
    void clearSearchState();

    SpotlightRuntimePaths m_paths;
    DesktopEntryCatalog *m_catalog = nullptr;
    std::unique_ptr<ApplicationLauncher> m_ownedLauncher;
    ApplicationLauncher *m_launcher = nullptr;
    RustSpotlightBackend m_backend;
    SpotlightResultsModel m_results;

    QTimer m_searchDebounce;
    QTimer m_closeAnimationTimer;
    QTimer m_weatherRefreshTimer;
    QPointer<QProcess> m_weatherProc;

    bool m_open = false;
    bool m_surfaceVisible = false;
    int m_selectedIndex = -1;
    QString m_query;
    QString m_pendingQuery;
    int m_weatherRequestGen = 0;

    bool m_weatherEnabled = true;
    bool m_weatherReady = false;
    bool m_weatherLoading = false;
    int m_weatherTemp = 0;
    QString m_weatherCity;
    QString m_weatherCondition;
    QString m_weatherStatusText{QStringLiteral("Loading")};
    QUrl m_weatherIconSource;
    QDateTime m_lastWeatherFetch;

    bool m_gameModeActive = false;
    QString m_locale = QStringLiteral("en_US");

    static constexpr int kSearchDebounceMs = 35;
    static constexpr int kWeatherStaleThresholdSec = 30 * 60;
    static constexpr int kWeatherRefreshIntervalMs = 30 * 60 * 1000;
    static constexpr int kCloseAnimationMs = 190;
};
