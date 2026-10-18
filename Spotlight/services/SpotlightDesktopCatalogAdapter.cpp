#include "services/SpotlightDesktopCatalogAdapter.hpp"

#include "apps/DesktopEntryCatalog.hpp"

SpotlightDesktopCatalogAdapter::SpotlightDesktopCatalogAdapter(
    DesktopEntryCatalog &catalog, QObject *parent)
    : SpotlightCatalogSource(parent)
    , m_catalog(catalog)
{
    connect(&m_catalog, &DesktopEntryCatalog::indexUpdated,
            this, &SpotlightCatalogSource::updated);
}

QJsonArray SpotlightDesktopCatalogAdapter::snapshotJson() const
{
    return m_catalog.snapshotJson();
}

QStringList SpotlightDesktopCatalogAdapter::watchedDirectories() const
{
    return m_catalog.watchedDirectories();
}
