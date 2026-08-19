#include "core/SpotlightController.hpp"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QDebug>
#include <QUrl>

SpotlightController::SpotlightController(const SpotlightRuntimePaths &paths, QObject *parent)
    : SpotlightController(paths, nullptr, nullptr, parent) {
}

SpotlightController::SpotlightController(const SpotlightRuntimePaths &paths,
                                         DesktopEntryCatalog *catalog,
                                         ApplicationLauncher *launcher,
                                         QObject *parent)
    : QObject(parent)
    , m_paths(paths)
    , m_catalog(catalog)
    , m_ownedLauncher(launcher ? nullptr : std::make_unique<ApplicationLauncher>(paths.astreaLaunch()))
    , m_launcher(launcher ? launcher : m_ownedLauncher.get()) {
    m_searchDebounce.setSingleShot(true);
    m_searchDebounce.setInterval(kSearchDebounceMs);
    connect(&m_searchDebounce, &QTimer::timeout, this, &SpotlightController::runSearch);

    m_closeAnimationTimer.setSingleShot(true);
    m_closeAnimationTimer.setInterval(kCloseAnimationMs);
    connect(&m_closeAnimationTimer, &QTimer::timeout, this, [this] {
        if (m_open) return;
        m_surfaceVisible = false;
        emit surfaceVisibleChanged();
    });

    m_weatherRefreshTimer.setInterval(kWeatherRefreshIntervalMs);
    connect(&m_weatherRefreshTimer, &QTimer::timeout, this, &SpotlightController::refreshWeather);

    connect(&m_results, &QAbstractItemModel::modelReset, this, [this] {
        int next = m_results.resultCount() > 0 ? 0 : -1;
        if (m_selectedIndex != next) {
            m_selectedIndex = next;
            emit selectedIndexChanged();
        }
        emit resultCountChanged();
    });

    // Connect launcher signals
    connect(m_launcher, &ApplicationLauncher::launchSucceeded,
            this, &SpotlightController::onLaunchSucceeded);
    connect(m_launcher, &ApplicationLauncher::launchFailed,
            this, &SpotlightController::onLaunchFailed);
    connect(m_launcher, &ApplicationLauncher::launchTimedOut,
            this, [this](const QString &id) {
        emit launchFailed(QStringLiteral("Launch timed out: ") + id);
        show();
    });

    setWeatherEnabled(true);

    if (m_catalog) {
        connect(m_catalog, &DesktopEntryCatalog::indexUpdated, this, [this] {
            QString error;
            if (!m_backend.setCatalog(m_catalog->snapshotJson(), &error)) {
                qWarning("Shared catalog update failed: %s", qPrintable(error));
                return;
            }
            if (!m_query.isEmpty())
                runSearch();
        });
    }

    // Startup weather warmup (only if enabled, even while hidden)
    QTimer::singleShot(500, this, [this]() {
        if (m_weatherEnabled && !m_gameModeActive)
            refreshWeather();
    });
}

bool SpotlightController::init(const QString &locale, QString *errorOut) {
    m_locale = locale;
    const bool created = m_catalog
        ? m_backend.createWithCatalog(m_paths.astreaRoot(), locale, m_catalog->snapshotJson(), errorOut)
        : m_backend.create(m_paths.astreaRoot(), locale, errorOut);
    if (!created)
        return false;
    return true;
}

bool SpotlightController::setLocale(const QString &locale, QString *errorOut) {
    if (m_locale == locale)
        return true;

    const QString previousQuery = m_query;
    const int previousIndex = m_selectedIndex;
    m_locale = locale;
    const bool created = m_catalog
        ? m_backend.createWithCatalog(m_paths.astreaRoot(), locale, m_catalog->snapshotJson(), errorOut)
        : m_backend.create(m_paths.astreaRoot(), locale, errorOut);
    if (!created)
        return false;

    reloadIndex();
    if (!previousQuery.isEmpty())
        m_pendingQuery = previousQuery;
    runSearch();
    if (previousIndex >= 0 && previousIndex < m_results.resultCount())
        setSelectedIndex(previousIndex);
    return true;
}

void SpotlightController::show() {
    if (!m_componentEnabled)
        return;
    m_closeAnimationTimer.stop();
    if (!m_surfaceVisible) {
        m_surfaceVisible = true;
        emit surfaceVisibleChanged();
    }
    if (!m_open) {
        m_open = true;
        emit openChanged();
    }
    if (m_weatherEnabled && !m_gameModeActive) {
        m_weatherRefreshTimer.start();
        if (!m_weatherReady || !m_lastWeatherFetch.isValid() ||
            m_lastWeatherFetch.secsTo(QDateTime::currentDateTimeUtc()) >= kWeatherStaleThresholdSec)
            refreshWeather();
    }
    QTimer::singleShot(0, this, [this] { emit focusRequested(); });
}

void SpotlightController::close() {
    if (!m_open && !m_surfaceVisible) return;
    m_searchDebounce.stop();

    // Cancel any in-flight weather request
    {
        ++m_weatherRequestGen;
        m_weatherLoading = false;
        if (m_weatherProc) {
            if (m_weatherProc->state() != QProcess::NotRunning) {
                m_weatherProc->kill();
            }
            m_weatherProc.clear();
        }
        m_weatherRefreshTimer.stop();
        emit weatherChanged();
    }

    clearSearchState();
    if (m_open) {
        m_open = false;
        emit openChanged();
    }
    m_closeAnimationTimer.start();
}

void SpotlightController::toggle() {
    if (!m_componentEnabled)
        return;
    isOpen() ? close() : show();
}

void SpotlightController::setQuery(const QString &q) {
    if (!m_componentEnabled)
        return;
    if (!isOpen()) show();
    if (m_query != q) {
        m_query = q;
        emit queryChanged();
    }
    m_pendingQuery = q;
    m_searchDebounce.stop();
    runSearch();
}

void SpotlightController::scheduleSearch(const QString &q) {
    if (!m_componentEnabled)
        return;
    if (m_query != q) {
        m_query = q;
        emit queryChanged();
    }
    m_pendingQuery = q;
    m_searchDebounce.start();
}

void SpotlightController::runSearch() {
    QString q = m_pendingQuery.trimmed();
    if (q.isEmpty()) {
        m_results.clear();
        return;
    }

    QString error;
    QJsonArray results = m_backend.search(q, 6, &error);
    if (!error.isEmpty()) {
        qWarning("Spotlight search error: %s", qPrintable(error));
    }
    m_results.setResults(results);
}

void SpotlightController::moveSelection(int delta) {
    int count = m_results.resultCount();
    if (count <= 0) return;
    int current = m_selectedIndex < 0 ? 0 : m_selectedIndex;
    setSelectedIndex((current + delta + count) % count);
}

void SpotlightController::setSelectedIndex(int index) {
    int count = m_results.resultCount();
    int bounded = count <= 0 ? -1 : qBound(0, index, count - 1);
    if (m_selectedIndex == bounded) return;
    m_selectedIndex = bounded;
    emit selectedIndexChanged();
}

void SpotlightController::activateCurrent() {
    launch(m_selectedIndex);
}

void SpotlightController::launch(int row) {
    if (!m_componentEnabled)
        return;
    if (row < 0 || row >= m_results.resultCount()) return;
    const SearchResultItem item = m_results.resultAt(row);

    // Record usage immediately on activation (before launch)
    QString err;
    m_backend.recordLaunch(item.id, &err);
    if (!err.isEmpty())
        qWarning("Failed to record launch: %s", qPrintable(err));

    close();
    m_launcher->launchDesktop(item.id, item.desktopFileName, item.exec, item.name,
                              item.icon, item.desktopFilePath);
}

void SpotlightController::reloadIndex() {
    m_results.clear();
    QString err;
    if (m_catalog)
        m_backend.setCatalog(m_catalog->snapshotJson(), &err);
    else
        m_backend.reload(&err);
    if (!err.isEmpty())
        qWarning("Reload error: %s", qPrintable(err));
}

QJsonArray SpotlightController::watchedDirectories() {
    if (m_catalog) {
        QJsonArray result;
        for (const QString &directory : m_catalog->watchedDirectories())
            result.append(directory);
        return result;
    }
    return m_backend.watchedDirectories();
}

void SpotlightController::onLaunchSucceeded(const QString &desktopId) {
    Q_UNUSED(desktopId);
    // Usage already recorded on activation
}

void SpotlightController::onLaunchFailed(const QString &desktopId, const QString &error) {
    Q_UNUSED(desktopId);
    // Log the error but stay closed (don't reopen)
    qWarning("Spotlight launch failed for '%s': %s",
             qPrintable(desktopId), qPrintable(error));
    emit launchFailed(error);
    // Do NOT call show() - matches original behavior
}

void SpotlightController::refreshWeather() {
    if (!m_weatherEnabled || m_weatherLoading || m_gameModeActive) return;
    m_weatherLoading = true;
    ++m_weatherRequestGen;
    int gen = m_weatherRequestGen;
    m_weatherStatusText = m_weatherReady ? QStringLiteral("Updating") : QStringLiteral("Loading");
    emit weatherChanged();

    // Create a fresh QProcess per request to avoid stale callbacks
    auto *proc = new QProcess(this);
    proc->setProperty("generation", gen);

    connect(proc, &QProcess::finished, this, [this, proc, gen](int exitCode, QProcess::ExitStatus status) {
        proc->deleteLater();
        if (gen != m_weatherRequestGen) return;
        if (proc != m_weatherProc) return;
        m_weatherProc.clear();

        m_weatherLoading = false;
        if (exitCode != 0 || status != QProcess::NormalExit) {
            QString stderr = QString::fromUtf8(proc->readAllStandardError()).trimmed();
            if (!stderr.isEmpty())
                qWarning("Weather CLI stderr: %s", qPrintable(stderr));
            applyWeatherError(QStringLiteral("No weather data"));
            return;
        }
        QByteArray out = proc->readAllStandardOutput();
        QJsonDocument doc = QJsonDocument::fromJson(out);
        if (!doc.isObject()) {
            applyWeatherError(QStringLiteral("Invalid weather data"));
            return;
        }
        // Only cache if we're still interested (not closed/disabled)
        if (m_weatherEnabled)
            applyWeather(doc.object());
    });

    connect(proc, &QProcess::errorOccurred, this, [this, proc, gen](QProcess::ProcessError err) {
        Q_UNUSED(err);
        proc->deleteLater();
        if (gen != m_weatherRequestGen) return;
        if (proc != m_weatherProc) return;
        m_weatherProc.clear();

        m_weatherLoading = false;
        applyWeatherError(QStringLiteral("Connection failed"));
    });

    m_weatherProc = proc;
    m_weatherProc->setProgram(m_paths.weatherCli());
    m_weatherProc->setArguments({QStringLiteral("summary")});
    m_weatherProc->start();
}

static QString normalizeWeatherCondition(const QString &condition) {
    const QString c = condition.toLower();
    if (c.contains(QStringLiteral("thunderstorm")) || c.contains(QStringLiteral("trovoada")))
        return QStringLiteral("thunderstorm");
    if (c.contains(QStringLiteral("freezing")) || c.contains(QStringLiteral("chuva gelada")) || c.contains(QStringLiteral("garoa gelada")))
        return QStringLiteral("freezing_rain");
    if (c.contains(QStringLiteral("heavy")) || c.contains(QStringLiteral("chuva forte")) || c.contains(QStringLiteral("garoa forte")) || c.contains(QStringLiteral("pancadas fortes")))
        return QStringLiteral("heavy_rain");
    if (c.contains(QStringLiteral("drizzle")) || c.contains(QStringLiteral("light")) || c.contains(QStringLiteral("garoa")) || c.contains(QStringLiteral("chuva leve")) || c.contains(QStringLiteral("pancadas")))
        return QStringLiteral("light_rain");
    if (c.contains(QStringLiteral("rain")))
        return QStringLiteral("rain");
    if (c.contains(QStringLiteral("mist")) || c.contains(QStringLiteral("fog")) || c.contains(QStringLiteral("névoa")) || c.contains(QStringLiteral("nevoa")))
        return QStringLiteral("mist");
    if (c.contains(QStringLiteral("overcast")) || c.contains(QStringLiteral("cloudy")) || c.contains(QStringLiteral("nublado")))
        return QStringLiteral("cloudy");
    if (c.contains(QStringLiteral("partly")) || c.contains(QStringLiteral("mostly")) || c.contains(QStringLiteral("parcialmente")) || c.contains(QStringLiteral("principalmente")))
        return QStringLiteral("partially_cloudy");
    return QStringLiteral("clear");
}

void SpotlightController::applyWeather(const QJsonObject &data) {
    m_weatherReady = true;
    m_weatherLoading = false;
    m_weatherTemp = data.value(QStringLiteral("temp")).toInt();
    m_weatherCity = data.value(QStringLiteral("city")).toString();
    m_weatherCondition = data.value(QStringLiteral("condition")).toString();
    m_weatherStatusText.clear();

    const QString assetRoot = QUrl::fromLocalFile(m_paths.weatherAssetDir() + QStringLiteral("/")).toString();
    m_weatherIconSource = QUrl(assetRoot + normalizeWeatherCondition(m_weatherCondition) + QStringLiteral(".png"));

    m_lastWeatherFetch = QDateTime::currentDateTimeUtc();
    emit weatherChanged();
}

void SpotlightController::applyWeatherError(const QString &error) {
    m_weatherLoading = false;
    m_weatherReady = false;
    m_weatherStatusText = error.isEmpty() ? QStringLiteral("No weather data") : error;
    emit weatherChanged();
}

void SpotlightController::ensureConfig() {
    QString error;
    m_backend.ensureConfig(&error);
    if (!error.isEmpty())
        qWarning("Config ensure error: %s", qPrintable(error));
}

void SpotlightController::setGameModeActive(bool active) {
    if (m_gameModeActive == active) return;
    m_gameModeActive = active;
    if (active) {
        // Cancel weather, but do NOT close the spotlight
        ++m_weatherRequestGen;
        m_weatherLoading = false;
        if (m_weatherProc) {
            m_weatherProc->kill();
            m_weatherProc.clear();
        }
        m_weatherRefreshTimer.stop();
        emit weatherChanged();
    } else if (isOpen()) {
        m_weatherRefreshTimer.start();
        if (!m_weatherReady || !m_lastWeatherFetch.isValid() ||
            m_lastWeatherFetch.secsTo(QDateTime::currentDateTimeUtc()) >= kWeatherStaleThresholdSec)
            refreshWeather();
    }
}

void SpotlightController::setComponentEnabled(bool enabled) {
    if (m_componentEnabled == enabled)
        return;
    m_componentEnabled = enabled;
    emit componentEnabledChanged();
    if (!enabled && (isOpen() || m_surfaceVisible))
        close();
}

void SpotlightController::setWeatherEnabled(bool enabled) {
    if (m_weatherEnabled == enabled) return;
    m_weatherEnabled = enabled;
    emit weatherEnabledChanged();

    if (!enabled) {
        m_weatherRefreshTimer.stop();
        if (m_weatherProc) {
            m_weatherProc->kill();
            m_weatherProc.clear();
        }
        m_weatherLoading = false;
        m_weatherReady = false;
        m_weatherRequestGen++;
        applyWeatherError(QStringLiteral("Disabled"));
    } else if (isOpen() && !m_gameModeActive) {
        m_weatherRefreshTimer.start();
        if (!m_weatherReady || !m_lastWeatherFetch.isValid() ||
            m_lastWeatherFetch.secsTo(QDateTime::currentDateTimeUtc()) >= kWeatherStaleThresholdSec)
            refreshWeather();
    }
}

void SpotlightController::applyConfig(const QJsonObject &config) {
    QJsonValue weatherVal = config.value(QStringLiteral("weather"));
    if (weatherVal.isBool())
        setWeatherEnabled(weatherVal.toBool());
}

void SpotlightController::clearSearchState() {
    m_pendingQuery.clear();
    if (!m_query.isEmpty()) {
        m_query.clear();
        emit queryChanged();
    }
    m_results.clear();
}
