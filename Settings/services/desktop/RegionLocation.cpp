#include "RegionLocation.hpp"
#include "RegionCatalogFfi.h"
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusVariant>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QTimer>
#include <memory>

namespace
{
constexpr auto portalPath = "/org/freedesktop/portal/desktop";
constexpr auto locationInterface = "org.freedesktop.portal.Location";
constexpr uint accuracyCity = 2;
constexpr qint64 maximumLookupReply = 64 * 1024;

bool geoClueInstalled()
{
    const auto bus = QDBusConnection::systemBus();
    if (!bus.isConnected())
        return false;
    const auto service = QStringLiteral("org.freedesktop.GeoClue2");
    const auto *interface = bus.interface();
    if (!interface)
        return false;
    if (interface->isServiceRegistered(service).value())
        return true;
    const QDBusReply<QStringList> activatable = interface->activatableServiceNames();
    return activatable.isValid() && activatable.value().contains(service);
}
QString senderToken(const QDBusConnection &bus)
{
    return bus.baseService().mid(1).replace(u'.', u'_');
}

class PortalWaiter final : public QObject
{
    Q_OBJECT
  public:
    using QObject::QObject;
    QEventLoop loop;
    RegionLocation::Fix fix;
    bool granted = false;
    QString session;

  public slots:
    void onResponse(uint code, const QVariantMap &)
    {
        if (code == 0) {
            granted = true;
            return;
        }
        fix.status = code == 1 ? RegionLocation::Status::Denied : RegionLocation::Status::Failed;
        loop.quit();
    }
    void onLocation(const QDBusObjectPath &path, const QVariantMap &data)
    {
        if (path.path() != session)
            return;
        fix.latitude = data.value(QStringLiteral("Latitude")).toDouble();
        fix.longitude = data.value(QStringLiteral("Longitude")).toDouble();
        fix.status = RegionLocation::Status::Found;
        loop.quit();
    }
};
} // namespace

namespace RegionLocation
{
Fix locateWithPortal(const std::atomic_bool &cancel, const PortalOptions &options)
{
    if (options.requireGeoClue && !geoClueInstalled())
        return {Status::Unavailable};
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return {Status::Unavailable};

    const auto path = QString::fromLatin1(portalPath);
    const auto interface = QString::fromLatin1(locationInterface);
    const auto suffix = QString::number(QRandomGenerator::global()->generate(), 16);
    const auto sessionToken = QStringLiteral("astrea_session_") + suffix;
    const auto requestToken = QStringLiteral("astrea_request_") + suffix;
    const auto sender = senderToken(bus);
    const auto requestPath = path + QStringLiteral("/request/") + sender + u'/' + requestToken;
    const auto expectedSession = path + QStringLiteral("/session/") + sender + u'/' + sessionToken;

    PortalWaiter waiter;
    waiter.fix.status = Status::TimedOut;
    waiter.session = expectedSession;
    // Subscribe before starting: the portal may answer before Start returns.
    bus.connect(options.service, requestPath, QStringLiteral("org.freedesktop.portal.Request"),
                QStringLiteral("Response"), &waiter, SLOT(onResponse(uint, QVariantMap)));
    bus.connect(options.service, path, interface, QStringLiteral("LocationUpdated"), &waiter,
                SLOT(onLocation(QDBusObjectPath, QVariantMap)));
    const auto disconnect = [&] {
        bus.disconnect(options.service, requestPath, QStringLiteral("org.freedesktop.portal.Request"),
                       QStringLiteral("Response"), &waiter, SLOT(onResponse(uint, QVariantMap)));
        bus.disconnect(options.service, path, interface, QStringLiteral("LocationUpdated"), &waiter,
                       SLOT(onLocation(QDBusObjectPath, QVariantMap)));
    };

    auto create = QDBusMessage::createMethodCall(options.service, path, interface,
                                                 QStringLiteral("CreateSession"));
    create << QVariantMap{{QStringLiteral("session_handle_token"), sessionToken},
                          {QStringLiteral("accuracy"), accuracyCity}};
    const QDBusReply<QDBusObjectPath> created = bus.call(create, QDBus::Block, 5000);
    if (!created.isValid()) {
        disconnect();
        return {Status::Unavailable};
    }
    waiter.session = created.value().path();

    auto start =
        QDBusMessage::createMethodCall(options.service, path, interface, QStringLiteral("Start"));
    start << QVariant::fromValue(created.value()) << QString()
          << QVariantMap{{QStringLiteral("handle_token"), requestToken}};
    const auto started = bus.call(start, QDBus::Block, 5000);
    if (started.type() == QDBusMessage::ErrorMessage) {
        waiter.fix.status = Status::Failed;
    } else if (waiter.fix.status == Status::TimedOut) {
        QTimer deadline;
        deadline.setSingleShot(true);
        QObject::connect(&deadline, &QTimer::timeout, &waiter.loop, &QEventLoop::quit);
        deadline.start(options.timeoutMs);
        QTimer poll;
        QObject::connect(&poll, &QTimer::timeout, &waiter.loop, [&] {
            if (cancel.load()) {
                waiter.fix.status = Status::Cancelled;
                waiter.loop.quit();
            }
        });
        poll.start(100);
        waiter.loop.exec();
    }

    // Stop GeoClue tracking as soon as one fix is in.
    bus.call(QDBusMessage::createMethodCall(options.service, waiter.session,
                                            QStringLiteral("org.freedesktop.portal.Session"),
                                            QStringLiteral("Close")),
             QDBus::Block, 2000);
    disconnect();
    return waiter.fix;
}

QUrl defaultIpLookupUrl() { return QUrl(QStringLiteral("https://ipapi.co/json/")); }

Fix lookupByIp(const std::atomic_bool &cancel, const QUrl &url, int timeoutMs)
{
    QNetworkAccessManager network;
    network.setCookieJar(nullptr);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("AstreaSettings"));
    request.setTransferTimeout(timeoutMs);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    std::unique_ptr<QNetworkReply> reply(network.get(request));

    QEventLoop loop;
    QObject::connect(reply.get(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(reply.get(), &QNetworkReply::readyRead, &loop, [&] {
        if (reply->bytesAvailable() > maximumLookupReply)
            reply->abort();
    });
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (cancel.load())
            reply->abort();
    });
    poll.start(100);
    loop.exec();
    if (cancel.load())
        return {Status::Cancelled};

    const auto httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto body = reply->read(maximumLookupReply);
    if (httpStatus == 403 || httpStatus == 429)
        return {Status::Refused};
    if (reply->error() != QNetworkReply::NoError)
        return {Status::NetworkError};

    Fix fix{Status::Failed};
    const auto parsed = astrea_region_parse_ip_lookup(
        astreaRegionText(body),
        [](void *context, AstreaRegionText country, AstreaRegionText zone) {
            auto *target = static_cast<Fix *>(context);
            target->country = QString::fromUtf8(country.data, qsizetype(country.size));
            target->zone = QString::fromUtf8(zone.data, qsizetype(zone.size));
        },
        &fix);
    fix.status = parsed == 1 ? Status::Found : parsed == 2 ? Status::Refused : Status::Failed;
    return fix;
}
} // namespace RegionLocation

#include "RegionLocation.moc"
