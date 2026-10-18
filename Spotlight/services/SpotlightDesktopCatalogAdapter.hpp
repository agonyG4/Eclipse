#pragma once

#include "core/SpotlightCatalogSource.hpp"

class DesktopEntryCatalog;

class SpotlightDesktopCatalogAdapter final : public SpotlightCatalogSource {
public:
    explicit SpotlightDesktopCatalogAdapter(DesktopEntryCatalog &catalog,
                                             QObject *parent = nullptr);

    QJsonArray snapshotJson() const override;
    QStringList watchedDirectories() const override;

private:
    DesktopEntryCatalog &m_catalog;
};
