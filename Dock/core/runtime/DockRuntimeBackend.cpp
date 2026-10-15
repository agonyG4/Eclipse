#include "core/runtime/DockRuntimeBackend.hpp"

using namespace Astrea::Dock;

DockRuntimeBackend::DockRuntimeBackend(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<DockRuntimeWindow>();
    qRegisterMetaType<DockRuntimeSnapshot>();
    qRegisterMetaType<DockRuntimeAction>();
    qRegisterMetaType<DockRuntimeActionResult>();
    qRegisterMetaType<DockRuntimeActionError>();
    qRegisterMetaType<DockApplicationRuntimeState>();
    qRegisterMetaType<DockApplicationRuntimeProjection>();
}
