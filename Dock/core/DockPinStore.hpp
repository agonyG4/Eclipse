#pragma once

#include <QString>
#include <QStringList>

class DockPinStore {
public:
    virtual ~DockPinStore() = default;

    virtual bool writePins(const QStringList &pins, QString *errorOut = nullptr) = 0;
};
