#pragma once
#include <QString>
#include <QUrl>
#include <atomic>

// One-shot location requests, each started only by an explicit user action.
namespace RegionLocation
{
enum class Status {
    Found,
    Unavailable,  // No location service on this system.
    Denied,       // The user or the system refused.
    TimedOut,
    Failed,       // The service answered without a usable location.
    NetworkError, // IP lookup could not reach the service.
    Refused,      // IP lookup was refused, usually rate limiting.
    Cancelled,
};
struct Fix {
    Fix(Status result = Status::Failed) : status(result) {}
    Status status;
    double latitude = 0;
    double longitude = 0;
    QString country; // IP lookup only: coordinates are never requested there.
    QString zone;
};
struct PortalOptions {
    QString service = QStringLiteral("org.freedesktop.portal.Desktop");
    bool requireGeoClue = true;
    int timeoutMs = 30000;
};
// City accuracy through the location portal, which fronts GeoClue and owns the
// permission decision. Runs its own event loop, so call from a worker thread.
Fix locateWithPortal(const std::atomic_bool &cancel, const PortalOptions &options = {});

QUrl defaultIpLookupUrl();
// Sends one HTTPS request, which reveals the public IP to the service at `url`.
Fix lookupByIp(const std::atomic_bool &cancel, const QUrl &url = defaultIpLookupUrl(),
               int timeoutMs = 10000);
} // namespace RegionLocation
