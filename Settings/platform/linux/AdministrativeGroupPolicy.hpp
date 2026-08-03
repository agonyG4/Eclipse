#pragma once

#include <QStringList>

class AdministrativeGroupPolicy final {
public:
    static bool hasAdministrativeGroup(const QStringList &groupNames);
};
