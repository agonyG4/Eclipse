#pragma once

#include "core/navigation/SettingsNavigationEntry.hpp"

#include <QVector>

class SettingsNavigationCatalog final {
public:
    SettingsNavigationCatalog();

    const QVector<SettingsNavigationEntry> &entries() const;

private:
    QVector<SettingsNavigationEntry> m_entries;
};
