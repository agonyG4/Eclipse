#pragma once

#include "ContextMenuController.hpp"

#include <QRect>

class ApplicationLauncher;
class DesktopEntryCatalog;
class DockController;
namespace Astrea::StatusNotifier {
class StatusNotifierService;
class DBusMenuModel;
}

namespace Astrea::Shell {

class DesktopContextMenuProvider final {
public:
    DesktopContextMenuProvider(ApplicationLauncher *launcher,
                               DesktopEntryCatalog *catalog = nullptr)
        : m_launcher(launcher), m_catalog(catalog)
    {
    }

    bool present(ContextMenuController *controller, const QPoint &point,
                 const QString &outputKey) const;

private:
    ApplicationLauncher *m_launcher = nullptr;
    DesktopEntryCatalog *m_catalog = nullptr;
};

class DockContextMenuProvider final {
public:
    DockContextMenuProvider(DockController *dock, DesktopEntryCatalog *catalog,
                            ApplicationLauncher *launcher)
        : m_dock(dock), m_catalog(catalog), m_launcher(launcher)
    {
    }

    bool present(ContextMenuController *controller, const QString &desktopFileName,
                 const QRect &itemRectangle, const QString &outputKey) const;

private:
    DockController *m_dock = nullptr;
    DesktopEntryCatalog *m_catalog = nullptr;
    ApplicationLauncher *m_launcher = nullptr;
};

class TrayContextMenuAdapter final {
public:
    explicit TrayContextMenuAdapter(Astrea::StatusNotifier::StatusNotifierService *service)
        : m_service(service)
    {
    }

    bool present(ContextMenuController *controller, const QString &itemKey,
                 const ContextMenuAnchor &anchor, const QString &outputKey) const;

private:
    Astrea::StatusNotifier::StatusNotifierService *m_service = nullptr;
};

} // namespace Astrea::Shell
