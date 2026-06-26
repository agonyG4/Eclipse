#include "core/SpotlightController.hpp"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QDebug>

SpotlightController::SpotlightController(const SpotlightRuntimePaths &paths, QObject *parent)
    : QObject(parent), m_paths(paths), m_launcher(paths.astreaLaunch()) {
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
    connect(&m_launcher, &ApplicationLauncher::launchSucceeded,
            this, &SpotlightController::onLaunchSucceeded);
    connect(&m_launcher, &ApplicationLauncher::launchFailed,
            this, &SpotlightController::onLaunchFailed);
    connect(&m_launcher, &ApplicationLauncher::launchTimedOut,
            this, [this](const QString &id) {
        emit launchFailed(QStringLiteral("Launch timed out: ") + id);
        show();
    });

    setWeatherEnabled(true);

    // Startup weather warmup (only if enabled, even while hidden)
    QTimer::singleShot(500, this, [this]() {
        if (m_weatherEnabled && !m_gameModeActive)
            refreshWeather();
    });
}

bool SpotlightController::init(const QString &locale, QString *errorOut) {
    m_locale = locale;
    if (!m_backend.create(m_paths.astreaRoot(), locale, errorOut))
        return false;
    return true;
}

bool SpotlightController::setLocale(const QString &locale, QString *errorOut) {
    if (m_locale == locale)
        return true;

    const QString previousQuery = m_query;
    const int previousIndex = m_selectedIndex;
    m_locale = locale;
    if (!m_backend.create(m_paths.astreaRoot(), locale, errorOut))
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
    isOpen() ? close() : show();
}

void SpotlightController::setQuery(const QString &q) {
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
    if (row < 0 || row >= m_results.resultCount()) return;
    const auto &item = m_results.resultAt(row);

    // Record usage immediately on activation (before launch)
    QString err;
    m_backend.recordLaunch(item.id, &err);
    if (!err.isEmpty())
        qWarning("Failed to record launch: %s", qPrintable(err));

    close();
    m_launcher.launchDesktop(item.id, item.desktopFileName, item.exec, item.name,
                             item.icon, item.desktopFilePath);
}

void SpotlightController::reloadIndex() {
    m_results.clear();
    QString err;
    m_backend.reload(&err);
    if (!err.isEmpty())
        qWarning("Reload error: %s", qPrintable(err));
}

QJsonArray SpotlightController::watchedDirectories() {
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
    m_weatherStatusText = m_weatherReady ? QStringLiteral("Atualizando") : QStringLiteral("Carregando");
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
            applyWeatherError(QStringLiteral("Sem dados"));
            return;
        }
        QByteArray out = proc->readAllStandardOutput();
        QJsonDocument doc = QJsonDocument::fromJson(out);
        if (!doc.isObject()) {
            applyWeatherError(QStringLiteral("JSON inválido"));
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
        applyWeatherError(QStringLiteral("Falha ao conectar"));
    });

    m_weatherProc = proc;
    m_weatherProc->setProgram(m_paths.weatherCli());
    m_weatherProc->setArguments({QStringLiteral("summary")});
    m_weatherProc->start();
}

void SpotlightController::applyWeather(const QJsonObject &data) {
    m_weatherReady = true;
    m_weatherLoading = false;
    m_weatherTemp = data.value(QStringLiteral("temp")).toInt();
    m_weatherCity = data.value(QStringLiteral("city")).toString();
    m_weatherCondition = data.value(QStringLiteral("condition")).toString();
    m_weatherStatusText.clear();

    QString condition = m_weatherCondition.toLower();
    QString assetRoot = QStringLiteral("file://") + m_paths.weatherAssetDir() + QStringLiteral("/");
    if (condition.contains(QStringLiteral("trovoada")))
        m_weatherIconSource = QUrl(assetRoot + QStringLiteral("thunderstorm.png"));
    else if (condition.contains(QStringLiteral("chuva gelada")) || condition.contains(QStringLiteral("garoa gelada")))
        m_weatherIconSource = QUrl(assetRoot + QStringLiteral("freezing_rain.png"));
    else if (condition.contains(QStringLiteral("chuva forte")) || condition.contains(QStringLiteral("garoa forte")) || condition.contains(QStringLiteral("pancadas fortes")))
        m_weatherIconSource = QUrl(assetRoot + QStringLiteral("heavy_rain.png"));
    else if (condition.contains(QStringLiteral("garoa")) || condition.contains(QStringLiteral("chuva leve")) || condition.contains(QStringLiteral("pancadas")))
        m_weatherIconSource = QUrl(assetRoot + QStringLiteral("light_rain.png"));
    else if (condition.contains(QStringLiteral("chuva")))
        m_weatherIconSource = QUrl(assetRoot + QStringLiteral("rain.png"));
    else if (condition.contains(QStringLiteral("névoa")) || condition.contains(QStringLiteral("nevoa")))
        m_weatherIconSource = QUrl(assetRoot + QStringLiteral("mist.png"));
    else if (condition.contains(QStringLiteral("nublado")))
        m_weatherIconSource = QUrl(assetRoot + QStringLiteral("cloudy.png"));
    else if (condition.contains(QStringLiteral("parcialmente")) || condition.contains(QStringLiteral("principalmente")))
        m_weatherIconSource = QUrl(assetRoot + QStringLiteral("partially_cloudy.png"));
    else
        m_weatherIconSource = QUrl(assetRoot + QStringLiteral("clear.png"));

    m_lastWeatherFetch = QDateTime::currentDateTimeUtc();
    emit weatherChanged();
}

void SpotlightController::applyWeatherError(const QString &error) {
    m_weatherLoading = false;
    m_weatherReady = false;
    m_weatherStatusText = error.isEmpty() ? QStringLiteral("Sem dados") : error;
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
    if (!enabled && isOpen())
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
        applyWeatherError(QStringLiteral("Desativado"));
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
