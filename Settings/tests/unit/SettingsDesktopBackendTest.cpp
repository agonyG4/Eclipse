#include "services/BarClockService.hpp"
#include "services/desktop/ApplicationsBackend.hpp"
#include "services/desktop/ProfileBackend.hpp"
#include "services/desktop/DesktopControllers.hpp"
#include "services/desktop/NativeSettingsSupport.hpp"
#include "services/desktop/RegionCatalog.hpp"
#include "services/desktop/RegionLocation.hpp"
#include "services/desktop/RegionSettings.hpp"
#include "services/desktop/SddmAutologin.hpp"
#include "settings/RegionalPreferences.hpp"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QFutureWatcher>
#include <QHostAddress>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtConcurrentRun>
#include <atomic>
#include <memory>
#include <vector>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>

class FakeSession final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Session")
  public:
    using QObject::QObject;
  public slots:
    void Close() { emit closed(); }
  signals:
    void closed();
};
// Answers like xdg-desktop-portal: session and request handles, then signals.
class FakePortal final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Location")
  public:
    explicit FakePortal(QDBusConnection bus) : m_bus(std::move(bus)) {}
    int responseCode = 0;
    uint accuracy = 0;
    int closedSessions = 0;

  public slots:
    QDBusObjectPath CreateSession(const QVariantMap &options, const QDBusMessage &message)
    {
        accuracy = options.value("accuracy").toUInt();
        const auto path = "/org/freedesktop/portal/desktop/session/" +
                          QString(message.service()).mid(1).replace(u'.', u'_') + "/" +
                          options.value("session_handle_token").toString();
        auto *session = new FakeSession(this);
        m_bus.registerObject(path, session, QDBusConnection::ExportAllSlots);
        connect(session, &FakeSession::closed, this, [this] { ++closedSessions; });
        return QDBusObjectPath(path);
    }
    QDBusObjectPath Start(const QDBusObjectPath &session, const QString &, const QVariantMap &options,
                          const QDBusMessage &message)
    {
        const auto request = "/org/freedesktop/portal/desktop/request/" +
                             QString(message.service()).mid(1).replace(u'.', u'_') + "/" +
                             options.value("handle_token").toString();
        if (responseCode >= 0) {
            auto response = QDBusMessage::createTargetedSignal(
                message.service(), request, "org.freedesktop.portal.Request", "Response");
            response << uint(responseCode) << QVariantMap{};
            m_bus.send(response);
        }
        if (responseCode == 0) {
            auto update = QDBusMessage::createTargetedSignal(
                message.service(), "/org/freedesktop/portal/desktop",
                "org.freedesktop.portal.Location", "LocationUpdated");
            update << QVariant::fromValue(session)
                   << QVariantMap{{"Latitude", -23.5}, {"Longitude", -46.6}, {"Accuracy", 5000.0}};
            m_bus.send(update);
        }
        return QDBusObjectPath(request);
    }

  private:
    QDBusConnection m_bus;
};
class SettingsDesktopBackendTest : public QObject
{
    Q_OBJECT
  private slots:
    void regionalReadDoesNotWrite()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("settings.json");
        const auto state = RegionSettings::read(path);
        QCOMPARE(state["language"].toString(), QString("en_US"));
        QVERIFY(!QFile::exists(path));
    }
    void regionalPatchPreservesUnknownFieldsAndRejectsInvalidValues()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("settings.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"future":42,"region":{"custom":"kept"}})");
        file.close();
        RegionSettings::update(path, "country_code", "JP");
        const auto data = RegionSettings::read(path);
        QCOMPARE(data["future"].toInt(), 42);
        QCOMPARE(data["region"].toMap()["custom"].toString(), QString("kept"));
        QVERIFY_EXCEPTION_THROWN(RegionSettings::update(path, "time_format", "invalid"),
                                 std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(RegionSettings::update(path, "automatic_location", "false"),
                                 std::runtime_error);
    }
    void corruptRegionalFileIsNotOverwritten()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("settings.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{bad");
        file.close();
        QVERIFY_EXCEPTION_THROWN(RegionSettings::update(path, "language", "en_US"),
                                 std::runtime_error);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("{bad"));
    }
    void desktopEntriesRespectPrecedenceAndHiddenMasks()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.filePath("user/applications"));
        QDir().mkpath(dir.filePath("system/applications"));
        auto write = [&](QString path, QByteArray body) {
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly))
                return false;
            return file.write(body) > 0;
        };
        QVERIFY(write(dir.filePath("system/applications/demo.desktop"),
                      "[Desktop Entry]\nType=Application\nName=System demo\nExec=/bin/true\n"));
        QVERIFY(write(dir.filePath("user/applications/demo.desktop"),
                      "[Desktop Entry]\nType=Application\nName=User demo\nExec=/bin/true\n"));
        ApplicationsBackend backend(dir.filePath("user"), {dir.filePath("system")},
                                    dir.filePath("Desktop"));
        auto apps = backend.inventory();
        QCOMPARE(apps.size(), 1);
        QCOMPARE(apps[0].toMap()["name"].toString(), QString("User demo"));
        QCOMPARE(apps[0].toMap()["source"].toString(), QString("User"));
        const auto details = backend.details("demo");
        QVERIFY(!details["privacyNote"].toString().isEmpty());
        QVERIFY(details["privacyItems"].toList().isEmpty());
        QVERIFY(write(dir.filePath("user/applications/demo.desktop"),
                      "[Desktop Entry]\nType=Application\nName=Hidden\nHidden=true\n"));
        QVERIFY(backend.inventory().isEmpty());
        QVERIFY_EXCEPTION_THROWN(backend.details("../../etc/passwd"), std::runtime_error);
    }
    void shortcutNeverOverwritesExistingFile()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.filePath("user/applications"));
        QDir().mkpath(dir.filePath("Desktop"));
        QFile file(dir.filePath("user/applications/demo.desktop"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[Desktop Entry]\nType=Application\nName=Demo\nExec=/bin/true\n");
        file.close();
        ApplicationsBackend backend(dir.filePath("user"), {}, dir.filePath("Desktop"));
        const QString id = backend.inventory()[0].toMap()["id"].toString();
        backend.shortcut(id);
        QVERIFY_EXCEPTION_THROWN(backend.shortcut(id), std::runtime_error);
        QVERIFY(QFile::exists(dir.filePath("Desktop/demo.desktop")));
    }
    void regionalConsumersObserveSavedFormat()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("settings.json");
        RegionalPreferences preferences(path);
        RegionSettings::update(path, "country_code", "US");
        preferences.reload();
        QCOMPARE(preferences.timeFormat(), QString("12h"));
        RegionSettings::update(path, "time_format", "24h");
        preferences.reload();
        QCOMPARE(preferences.timeFormat(), QString("24h"));
    }
    void regionalDefaultClockFollowsTheCountry()
    {
        QCOMPARE(RegionalPreferences::defaultTimeFormat("US"), QString("12h"));
        QCOMPARE(RegionalPreferences::defaultTimeFormat("AU"), QString("12h"));
        QCOMPARE(RegionalPreferences::defaultTimeFormat("BR"), QString("24h"));
        QCOMPARE(RegionalPreferences::defaultTimeFormat("DE"), QString("24h"));
        QCOMPARE(RegionalPreferences::defaultTimeFormat("??"), QString("24h"));
    }
    void countryCatalogIsCompleteTranslatedAndConsumable()
    {
        const auto english = RegionCatalog::countries("en_US");
        const auto portuguese = RegionCatalog::countries("pt_BR");
        QVERIFY2(english.size() >= 240, qPrintable(QString::number(english.size())));
        const auto named = [](const QVariantList &items, const QString &id) {
            for (const auto &item : items)
                if (item.toMap()["id"] == id)
                    return item.toMap()["name"].toString();
            return QString();
        };
        QCOMPARE(named(english, "DE"), QString("Germany"));
        QCOMPARE(named(portuguese, "DE"), QString("Alemanha"));
        // Every listed code must be accepted by the document and its consumers.
        QTemporaryDir dir;
        const auto path = dir.filePath("settings.json");
        for (const auto &item : english)
            QVERIFY(RegionCatalog::isCountryCode(item.toMap()["id"].toString()));
        RegionSettings::update(path, "country_code", "NZ");
        QCOMPARE(RegionSettings::read(path)["region"].toMap()["country_code"].toString(),
                 QString("NZ"));
        RegionalPreferences preferences(path);
        QCOMPARE(preferences.locale().territory(), QLocale::NewZealand);
        QVERIFY_EXCEPTION_THROWN(RegionSettings::update(path, "country_code", "ZZ"),
                                 std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(RegionSettings::update(path, "country_code", "br"),
                                 std::runtime_error);
    }
    void countrySearchFoldsAccentsAndFindsAliases()
    {
        const auto countries = RegionCatalog::countries("pt_BR");
        const auto first = [&](const QString &query) {
            const auto found = RegionCatalog::filter(countries, query);
            return found.isEmpty() ? QString() : found.first().toMap()["id"].toString();
        };
        QCOMPARE(first("brasil"), QString("BR"));
        QCOMPARE(first("Brazil"), QString("BR"));
        QCOMPARE(first("br"), QString("BR"));
        QCOMPARE(first("BRA"), QString("BR"));
        QCOMPARE(first("japao"), QString("JP"));
        QCOMPARE(first(QString::fromUtf8("Japão")), QString("JP"));
        QCOMPARE(first("uk"), QString("GB"));
        QCOMPARE(first("eua"), QString("US"));
        QCOMPARE(first("deutschland"), QString("DE"));
        QVERIFY(RegionCatalog::filter(countries, "xyzzy").isEmpty());
        QCOMPARE(RegionCatalog::filter(countries, "  ").size(), countries.size());
    }
    void timeZoneCatalogComesFromTheTable()
    {
        QTemporaryDir dir;
        const auto table = dir.filePath("zone.tab");
        QFile file(table);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("# fixture\n"
                   "BR\t-2332-04637\tAmerica/Sao_Paulo\tsoutheast\n"
                   "PT\t+3843-00908\tEurope/Lisbon\tmainland\n"
                   "XX\tbroken\n");
        file.close();
        const auto zones = RegionCatalog::timeZones("pt_BR", "Asia/Tokyo", table);
        QStringList ids;
        for (const auto &item : zones)
            ids.append(item.toMap()["id"].toString());
        // The fixture, UTC, and the running system's zone even when unlisted.
        QVERIFY(ids.contains("America/Sao_Paulo"));
        QVERIFY(ids.contains("Europe/Lisbon"));
        QVERIFY(ids.contains("UTC"));
        QVERIFY(ids.contains("Asia/Tokyo"));
        QCOMPARE(ids.size(), 4);
        const auto found = RegionCatalog::filter(zones, "sao paulo");
        QCOMPARE(found.first().toMap()["id"].toString(), QString("America/Sao_Paulo"));
        QCOMPARE(found.first().toMap()["country"].toString(), QString("BR"));
        QCOMPARE(RegionCatalog::filter(zones, "brasil").first().toMap()["id"].toString(),
                 QString("America/Sao_Paulo"));
        const auto match = RegionCatalog::nearestZone(38.7, -9.1, table);
        QCOMPARE(match.country, QString("PT"));
        QCOMPARE(match.zone, QString("Europe/Lisbon"));
        QVERIFY(RegionCatalog::nearestZone(99, 0, table).zone.isEmpty());
        QVERIFY(RegionCatalog::nearestZone(0, 0, dir.filePath("missing")).zone.isEmpty());
    }
    void letterHeadingsFoldAccents()
    {
        const auto named = [](const QString &name) { return QVariantMap{{"name", name}}; };
        const auto rows = RegionCatalog::withLetterHeadings(
            {named("Afeganistão"), named(QString::fromUtf8("África do Sul")), named("Brasil"),
             named(QString::fromUtf8("Égito")), named("Espanha")});
        const auto heading = [&](int index) { return rows[index].toMap()["heading"].toString(); };
        QCOMPARE(heading(0), QString("A"));
        QCOMPARE(heading(1), QString());
        QCOMPARE(heading(2), QString("B"));
        QCOMPARE(heading(3), QString("E"));
        QCOMPARE(heading(4), QString());
    }
    void systemCatalogOffersManyTimeZones()
    {
        if (!QFile::exists(RegionCatalog::zoneTablePath()))
            QSKIP("tzdata is not installed");
        const auto zones = RegionCatalog::timeZones("en_US", {});
        QVERIFY2(zones.size() > 300, qPrintable(QString::number(zones.size())));
        QVERIFY(!RegionCatalog::filter(zones, "utc-3").isEmpty());
        QVERIFY(!RegionCatalog::filter(zones, "tokyo").isEmpty());
    }
    void ipLookupKeepsOnlyCountryAndZoneAndReportsFailures()
    {
        struct Server {
            QTcpServer tcp;
            QByteArray request;
            explicit Server(QByteArray response)
            {
                tcp.listen(QHostAddress::LocalHost);
                QObject::connect(&tcp, &QTcpServer::newConnection, &tcp, [this, response] {
                    auto *socket = tcp.nextPendingConnection();
                    QObject::connect(socket, &QTcpSocket::readyRead, socket,
                                     [this, socket, response] {
                                         request += socket->readAll();
                                         socket->write(response);
                                         socket->disconnectFromHost();
                                     });
                });
            }
            QUrl url() const
            {
                return QUrl(QStringLiteral("http://127.0.0.1:%1/json/").arg(tcp.serverPort()));
            }
        };
        const auto http = [](int code, const QByteArray &body) {
            return "HTTP/1.1 " + QByteArray::number(code) + " X\r\nContent-Length: " +
                   QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        };
        {
            Server server(http(
                200, R"({"ip":"203.0.113.9","country_code":"PT","timezone":"Europe/Lisbon"})"));
            std::atomic_bool cancel{false};
            auto future = QtConcurrent::run(
                [&, url = server.url()] { return RegionLocation::lookupByIp(cancel, url, 3000); });
            QTRY_VERIFY_WITH_TIMEOUT(future.isFinished(), 5000);
            const auto fix = future.result();
            QCOMPARE(fix.status, RegionLocation::Status::Found);
            QCOMPARE(fix.country, QString("PT"));
            QCOMPARE(fix.zone, QString("Europe/Lisbon"));
            QVERIFY(server.request.contains("GET /json/"));
            QVERIFY(!server.request.toLower().contains("cookie"));
        }
        const std::vector<std::pair<QByteArray, RegionLocation::Status>> failures{
            {http(429, "{}"), RegionLocation::Status::Refused},
            {http(200, R"({"error":true,"reason":"RateLimited"})"), RegionLocation::Status::Refused},
            {http(200, "<html>"), RegionLocation::Status::Failed},
            {http(500, "{}"), RegionLocation::Status::NetworkError}};
        for (const auto &[response, expected] : failures) {
            Server server(response);
            std::atomic_bool cancel{false};
            auto future = QtConcurrent::run(
                [&, url = server.url()] { return RegionLocation::lookupByIp(cancel, url, 3000); });
            QTRY_VERIFY_WITH_TIMEOUT(future.isFinished(), 5000);
            QCOMPARE(future.result().status, expected);
        }
        // A closed port is a network failure, not a crash.
        std::atomic_bool cancel{false};
        auto future = QtConcurrent::run(
            [&] { return RegionLocation::lookupByIp(cancel, QUrl("http://127.0.0.1:1/"), 2000); });
        QTRY_VERIFY_WITH_TIMEOUT(future.isFinished(), 5000);
        QCOMPARE(future.result().status, RegionLocation::Status::NetworkError);
    }
    void portalFlowGrantsDeniesAndClosesTheSession()
    {
        if (!QDBusConnection::sessionBus().isConnected())
            QSKIP("no session bus");
        const auto owner = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "fake-portal");
        const auto service =
            QStringLiteral("org.astrea.test.FakePortal%1").arg(QCoreApplication::applicationPid());
        QDBusConnection bus(owner);
        QVERIFY(bus.registerService(service));
        FakePortal portal(bus);
        QVERIFY(bus.registerObject("/org/freedesktop/portal/desktop", &portal,
                                   QDBusConnection::ExportAllSlots));

        RegionLocation::PortalOptions options;
        options.service = service;
        options.requireGeoClue = false;
        options.timeoutMs = 3000;
        const auto locate = [](const RegionLocation::PortalOptions &request) {
            return QtConcurrent::run([request] {
                std::atomic_bool cancel{false};
                return RegionLocation::locateWithPortal(cancel, request);
            });
        };

        portal.responseCode = 0;
        auto granted = locate(options);
        QTRY_VERIFY_WITH_TIMEOUT(granted.isFinished(), 6000);
        QCOMPARE(granted.result().status, RegionLocation::Status::Found);
        QCOMPARE(granted.result().latitude, -23.5);
        QCOMPARE(granted.result().longitude, -46.6);
        QCOMPARE(portal.accuracy, 2u); // City: the catalogue needs nothing finer.
        QCOMPARE(portal.closedSessions, 1);

        portal.responseCode = 1;
        auto denied = locate(options);
        QTRY_VERIFY_WITH_TIMEOUT(denied.isFinished(), 6000);
        QCOMPARE(denied.result().status, RegionLocation::Status::Denied);
        QCOMPARE(portal.closedSessions, 2);

        // No answer at all: the wait ends at the deadline and the session still closes.
        portal.responseCode = -1;
        options.timeoutMs = 200;
        auto silent = locate(options);
        QTRY_VERIFY_WITH_TIMEOUT(silent.isFinished(), 6000);
        QCOMPARE(silent.result().status, RegionLocation::Status::TimedOut);
        QCOMPARE(portal.closedSessions, 3);

        // A portal that isn't running reads as an unavailable service.
        options.service = service + QStringLiteral(".Missing");
        auto missing = locate(options);
        QTRY_VERIFY_WITH_TIMEOUT(missing.isFinished(), 8000);
        QCOMPARE(missing.result().status, RegionLocation::Status::Unavailable);
        QDBusConnection::disconnectFromBus("fake-portal");
    }
    void regionControllerAppliesOnlyWhenAskedAndReportsFailures()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("settings.json");
        auto zone = std::make_shared<QString>("America/Sao_Paulo");
        auto outcome = std::make_shared<SettingsRegionServices::ZoneChange>(
            SettingsRegionServices::ZoneChange::Applied);
        auto locateFix = std::make_shared<RegionLocation::Fix>();
        auto ipCalls = std::make_shared<std::atomic_int>(0);
        locateFix->status = RegionLocation::Status::Unavailable;
        SettingsRegionServices services;
        services.locate = [locateFix](const std::atomic_bool &) { return *locateFix; };
        services.lookupByIp = [ipCalls](const std::atomic_bool &) {
            ++*ipCalls;
            RegionLocation::Fix fix{RegionLocation::Status::Found};
            fix.country = "PT";
            fix.zone = "Europe/Lisbon";
            return fix;
        };
        services.currentTimeZone = [zone] { return *zone; };
        services.setTimeZone = [zone, outcome](const QString &id) {
            if (*outcome == SettingsRegionServices::ZoneChange::Applied)
                *zone = id;
            return *outcome;
        };
        SettingsRegionController controller(path, services);
        const auto status = [&] { return controller.detection()["status"].toString(); };
        controller.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QCOMPARE(controller.state()["currentTimeZone"].toString(), QString("America/Sao_Paulo"));
        QVERIFY(controller.state()["countries"].toList().size() >= 240);
        QVERIFY(!QFile::exists(path)); // Reading and refreshing never write.

        // A missing service fails visibly and never falls back to the network by itself.
        controller.detectLocation();
        QTRY_COMPARE_WITH_TIMEOUT(status(), QString("unavailable"), 5000);
        QCOMPARE(ipCalls->load(), 0);
        QVERIFY(!QFile::exists(path));

        // The explicit lookup only proposes; nothing changes until it is applied.
        controller.detectLocationByIp();
        QTRY_COMPARE_WITH_TIMEOUT(status(), QString("found"), 5000);
        QCOMPARE(ipCalls->load(), 1);
        QCOMPARE(controller.detection()["country"].toString(), QString("PT"));
        QCOMPARE(controller.detection()["zone"].toString(), QString("Europe/Lisbon"));
        QVERIFY(!QFile::exists(path));
        QCOMPARE(*zone, QString("America/Sao_Paulo"));
        controller.dismissDetection();
        QCOMPARE(status(), QString("idle"));
        QVERIFY(!QFile::exists(path));

        controller.detectLocationByIp();
        QTRY_COMPARE_WITH_TIMEOUT(status(), QString("found"), 5000);
        controller.applyDetection();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QCOMPARE(RegionSettings::read(path)["region"].toMap()["country_code"].toString(),
                 QString("PT"));
        QCOMPARE(*zone, QString("Europe/Lisbon"));
        QCOMPARE(controller.state()["timeZoneChange"].toString(), QString("applied"));
        QCOMPARE(status(), QString("idle"));

        // Coordinates become the nearest zone, and are not exposed to QML.
        locateFix->status = RegionLocation::Status::Found;
        locateFix->latitude = 35.0;
        locateFix->longitude = 135.0;
        controller.detectLocation();
        QTRY_COMPARE_WITH_TIMEOUT(status(), QString("found"), 5000);
        QCOMPARE(controller.detection()["country"].toString(), QString("JP"));
        QVERIFY(!controller.detection().contains("latitude"));
        controller.dismissDetection();

        // A denied time-zone change reaches the page and keeps the old zone.
        *outcome = SettingsRegionServices::ZoneChange::Denied;
        controller.setTimeZone("Asia/Tokyo");
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QCOMPARE(controller.state()["timeZoneChange"].toString(), QString("denied"));
        QCOMPARE(controller.state()["currentTimeZone"].toString(), QString("Europe/Lisbon"));
        controller.setTimeZone("Not/AZone");
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QVERIFY(!controller.lastError().isEmpty());
        controller.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);

        // The full country list is grouped by letter; ranked results are not.
        const auto letters = controller.filterCountries({});
        QStringList headings;
        for (const auto &item : letters) {
            const auto heading = item.toMap()["heading"].toString();
            if (!heading.isEmpty())
                headings.append(heading);
        }
        QCOMPARE(headings.first(), QString("A"));
        QCOMPARE(headings.removeDuplicates(), 0); // Each letter opens exactly one group.
        QVERIFY(headings.size() > 20);
        QVERIFY(!letters.first().toMap()["heading"].toString().isEmpty());
        QVERIFY(letters.at(1).toMap()["heading"].toString().isEmpty());
        QVERIFY(controller.filterCountries("brazil").first().toMap()["heading"].toString().isEmpty());

        // The selected country's zones lead an empty search, and only then.
        const auto ordered = controller.filterTimeZones({});
        QCOMPARE(ordered.first().toMap()["country"].toString(), QString("PT"));
        QVERIFY(ordered.first().toMap()["inCountry"].toBool());
        QCOMPARE(controller.filterTimeZones("tokyo").first().toMap()["id"].toString(),
                 QString("Asia/Tokyo"));

        // Blocking geolocation stops the request before it reaches the service.
        NativeSettings::patchObject(path, [](QVariantMap &raw) {
            auto region = raw["region"].toMap();
            region["automatic_location"] = false;
            raw["region"] = region;
        });
        controller.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        controller.detectLocation();
        QCOMPARE(status(), QString("blocked"));
    }
    void sddmWritesOnlyItsOwnDropInAndHonorsMainFilePrecedence()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.filePath("sddm.conf.d"));
        QFile other(dir.filePath("sddm.conf.d/10-theme.conf"));
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.write("[Theme]\nCurrent=example\n");
        other.close();
        SddmAutologin::write("testuser", true, "astrea.desktop", dir.path());
        QCOMPARE(SddmAutologin::read(dir.path(), QString())["User"].toString(),
                 QString("testuser"));
        SddmAutologin::write("testuser", false, QString(), dir.path());
        QVERIFY(SddmAutologin::read(dir.path(), QString())["User"].toString().isEmpty());
        QFile main(dir.filePath("sddm.conf"));
        QVERIFY(main.open(QIODevice::WriteOnly));
        main.write("[Autologin]\nUser=other\n");
        main.close();
        QCOMPARE(SddmAutologin::read(dir.path(), QString())["User"].toString(), QString("other"));
        QVERIFY_EXCEPTION_THROWN(
            SddmAutologin::write("testuser", true, "astrea.desktop", dir.path()),
            std::runtime_error);
        QVERIFY(other.open(QIODevice::ReadOnly));
        QCOMPARE(other.readAll(), QByteArray("[Theme]\nCurrent=example\n"));
    }
    void barClockConsumesSavedRegionalFormat()
    {
        QTemporaryDir dir;
        const auto previous = qgetenv("XDG_CONFIG_HOME");
        qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
        const auto restore = qScopeGuard([previous] {
            if (previous.isNull())
                qunsetenv("XDG_CONFIG_HOME");
            else
                qputenv("XDG_CONFIG_HOME", previous);
        });
        const auto path = dir.filePath("AstreaOS/system/settings.json");
        RegionSettings::update(path, "language", "en_US");
        RegionSettings::update(path, "country_code", "US");
        RegionSettings::update(path, "time_format", "12h");
        BarClockService clock;
        clock.start();
        QVERIFY(QRegularExpression("^[0-9]{2}:[0-9]{2} [AP]M$").match(clock.timeText()).hasMatch());
        RegionSettings::update(path, "time_format", "24h");
        QTRY_VERIFY_WITH_TIMEOUT(
            QRegularExpression("^[0-9]{2}:[0-9]{2}$").match(clock.timeText()).hasMatch(), 6000);
        clock.stop();
    }
    void profileValidationRejectsControlCharacters()
    {
        QCOMPARE(ProfileBackend::validateName("  Jane   Doe  "), QString("Jane Doe"));
        QVERIFY_EXCEPTION_THROWN(ProfileBackend::validateName("bad\nname"), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(ProfileBackend::validateName(""), std::runtime_error);
    }
};
QTEST_GUILESS_MAIN(SettingsDesktopBackendTest)
#include "SettingsDesktopBackendTest.moc"
