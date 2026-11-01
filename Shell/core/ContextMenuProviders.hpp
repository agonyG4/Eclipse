#pragma once

#include "ContextMenuController.hpp"

#include <QRect>
#include <QString>
#include <QVariantMap>

#include <functional>

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

// Returns the dock's minimized entry for windowId, or an empty map when the
// window is no longer minimized in the Dock.
QVariantMap dockMinimizedWindowEntry(const DockController *dock, const QString &windowId);

class DockContextMenuProvider final {
public:
    // Resolves a catalog key and English fallback to display text.
    using Translator = std::function<QString(const QString &key, const QString &fallback)>;

    DockContextMenuProvider(DockController *dock, DesktopEntryCatalog *catalog,
                            ApplicationLauncher *launcher)
        : m_dock(dock), m_catalog(catalog), m_launcher(launcher)
    {
    }

    void setTranslator(Translator translator) { m_translate = std::move(translator); }

    bool present(ContextMenuController *controller, const QString &taskKey,
                 const QRect &itemRectangle, const QString &outputKey) const;
    bool presentWindow(ContextMenuController *controller, const QString &taskKey,
                       const QString &windowId, const QRect &itemRectangle,
                       const QString &outputKey) const;

private:
    QString translate(const QString &key, const QString &fallback) const;

    DockController *m_dock = nullptr;
    DesktopEntryCatalog *m_catalog = nullptr;
    ApplicationLauncher *m_launcher = nullptr;
    Translator m_translate;
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
