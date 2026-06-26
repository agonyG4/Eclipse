#pragma once

#include "platform/compositor/CompositorBackend.hpp"
#include <memory>

class CompositorBackendFactory {
public:
    static CompositorBackend* createBackend(const QString &requested, QObject *parent = nullptr);
};
