#pragma once
#include <QString>

class WineExecutableResolver {
public:
    static QString parseExeStem(const QString &cmdline, const QString &className);
};
