#pragma once

#include "apps/ApplicationIdentity.hpp"
#include "apps/DesktopEntryCatalog.hpp"
#include "apps/appidentity/IdentityCache.hpp"
#include "apps/appidentity/SteamMetadataIndex.hpp"

#include <QObject>
#include <QThreadPool>

#include <atomic>

class AppIdentityResolver : public QObject {
    Q_OBJECT
public:
    explicit AppIdentityResolver(QObject *parent = nullptr);
    ~AppIdentityResolver() override;

    void initialize(const QString &customHome = QString(), const QString &customProc = QStringLiteral("/proc"));
    void initialize(DesktopEntryCatalog *catalog, const QString &customProc = QStringLiteral("/proc"));

    AppIdentity resolveSync(const WindowIdentityInput &input);
    void resolveAsync(const WindowIdentityInput &input, quint64 generation);
    AppIdentity resolveDeep(const WindowIdentityInput &input);

    int themeRevision() const { return m_themeRevision.load(std::memory_order_acquire); }
    int desktopIndexRevision() const { return m_desktopIndex ? m_desktopIndex->revision() : 0; }
    int steamIndexRevision() const { return m_steamIndex ? m_steamIndex->revision() : 0; }

signals:
    void identityResolved(const QString &address, const AppIdentity &identity);

private slots:
    void onDeepResolved(const WindowIdentityInput &input, const AppIdentity &identity, quint64 generation);

private:
    AppIdentity resolveFast(const WindowIdentityInput &input);
    AppIdentity resolveAliases(const WindowIdentityInput &input);
    AppIdentity resolveSteamAppId(const WindowIdentityInput &input);
    AppIdentity resolveThemeFallback(const WindowIdentityInput &input);
    AppIdentity resolveDesktopEntry(const WindowIdentityInput &input);

    IdentityCache m_cache;
    DesktopEntryCatalog *m_desktopIndex = nullptr;
    bool m_ownsDesktopIndex = false;
    SteamMetadataIndex *m_steamIndex = nullptr;
    std::atomic<int> m_themeRevision{0};
    QString m_procRoot;
};
