#include "ContextMenuProviders.hpp"

#include "apps/DesktopEntryCatalog.hpp"
#include "core/DockController.hpp"
#include "launch/ApplicationLauncher.hpp"
#include "statusnotifier/DBusMenuModel.hpp"
#include "statusnotifier/StatusNotifierService.hpp"

#include <QDir>
#include <QLocale>
#include <QSet>

#include <utility>

namespace Astrea::Shell {
namespace {

QString localizedActionName(const DesktopEntryAction &action)
{
    const QLocale locale;
    const QStringList candidates{locale.name(), locale.bcp47Name(), locale.languageToString(locale.language())};
    for (const QString &candidate : candidates) {
        if (action.localizedNames.contains(candidate))
            return action.localizedNames.value(candidate);
    }
    return action.name;
}

ContextMenuAnchor pointAnchor(const QPoint &point)
{
    ContextMenuAnchor anchor;
    anchor.kind = ContextMenuAnchor::Kind::Point;
    anchor.point = point;
    return anchor;
}

ContextMenuAnchor rectangleAnchor(const QRect &rectangle)
{
    ContextMenuAnchor anchor;
    anchor.kind = ContextMenuAnchor::Kind::Rectangle;
    anchor.rectangle = rectangle;
    return anchor;
}

} // namespace

bool DesktopContextMenuProvider::present(ContextMenuController *controller, const QPoint &point,
                                         const QString &outputKey) const
{
    if (!controller || !m_launcher)
        return false;

    QVector<ContextMenuModel::NodeSpec> nodes;
    nodes.append(ContextMenuModel::NodeSpec{
        .token = QStringLiteral("desktop.settings"),
        .label = QStringLiteral("Settings"),
        .icon = QStringLiteral("preferences-system")
    });
    const ContextMenuTarget target{ContextMenuTarget::Kind::Desktop,
                                   QStringLiteral("desktop"), outputKey};
    return controller->present(target, pointAnchor(point), nodes,
                               [this](const QString &token) {
        if (token != QStringLiteral("desktop.settings") || !m_launcher)
            return false;
        ApplicationLaunchRequest request;
        request.desktopFileName = QStringLiteral("astrea-settings.desktop");
        request.desktopId = QStringLiteral("astrea-settings");
        if (m_catalog) {
            const auto record = m_catalog->findByDesktopFileName(request.desktopFileName);
            if (record) {
                request.desktopId = record->id;
                request.exec = record->exec;
                request.appName = record->name;
                request.iconName = record->icon;
                request.desktopFilePath = record->sourceFilePath;
            }
        }
        m_launcher->launchDesktop(request);
        return true;
    });
}

bool DockContextMenuProvider::present(ContextMenuController *controller,
                                      const QString &desktopFileName,
                                      const QRect &itemRectangle,
                                      const QString &outputKey) const
{
    if (!controller || !m_dock)
        return false;
    const int row = m_dock->appModel()->rowForDesktopFileName(desktopFileName);
    const DockAppInfo *item = m_dock->appModel()->itemAt(row);
    if (!item)
        return false;

    QVector<ContextMenuModel::NodeSpec> nodes;
    nodes.append(ContextMenuModel::NodeSpec{
        .token = QStringLiteral("dock.new-window"),
        .label = QStringLiteral("New Window"),
        .icon = item->iconName
    });

    const QVector<Astrea::Typhon::Toplevel> windows =
        m_dock->windowsForDesktopFileName(desktopFileName);
    QHash<QString, QString> windowActions;
    if (!windows.isEmpty()) {
        ContextMenuModel::NodeSpec openWindows;
        openWindows.kind = ContextMenuModel::NodeKind::Submenu;
        openWindows.label = QStringLiteral("Open Windows");
        openWindows.icon = item->iconName;
        for (int index = 0; index < windows.size(); ++index) {
            const auto &window = windows.at(index);
            const QString token = QStringLiteral("dock.window.") + window.id;
            windowActions.insert(token, window.id);
            openWindows.children.append(ContextMenuModel::NodeSpec{
                .token = token,
                .label = window.title.isEmpty() ? QStringLiteral("Untitled Window") : window.title,
                .icon = item->iconName,
                .enabled = true
            });
        }
        nodes.append(std::move(openWindows));
    }

    QHash<QString, DesktopEntryAction> desktopActions;
    if (m_catalog) {
        const auto record = m_catalog->findByDesktopFileName(desktopFileName);
        if (record) {
            for (int index = 0; index < record->actions.size(); ++index) {
                const DesktopEntryAction &action = record->actions.at(index);
                const QString token = QStringLiteral("dock.desktop-action.") + action.id;
                desktopActions.insert(token, action);
                nodes.append(ContextMenuModel::NodeSpec{
                    .token = token,
                    .label = localizedActionName(action),
                    .icon = action.icon,
                });
            }
        }
    }

    if (!windows.isEmpty()) {
        nodes.append(ContextMenuModel::NodeSpec{.kind = ContextMenuModel::NodeKind::Separator});
        nodes.append(ContextMenuModel::NodeSpec{
            .token = QStringLiteral("dock.close"),
            .label = windows.size() == 1 ? QStringLiteral("Close Window")
                                         : QStringLiteral("Close All Windows"),
            .icon = QStringLiteral("window-close"),
            .destructive = true
        });
    }

    nodes.append(ContextMenuModel::NodeSpec{.kind = ContextMenuModel::NodeKind::Separator});
    const bool pinned = item->pinned;
    nodes.append(ContextMenuModel::NodeSpec{
        .token = pinned ? QStringLiteral("dock.unpin") : QStringLiteral("dock.pin"),
        .label = pinned ? QStringLiteral("Unpin from Dock") : QStringLiteral("Pin to Dock"),
        .icon = pinned ? QStringLiteral("list-remove") : QStringLiteral("list-add")
    });

    const ContextMenuTarget target{ContextMenuTarget::Kind::DockApplication,
                                   desktopFileName, outputKey};
    const auto validator = [this, desktopFileName] {
        return m_dock && m_dock->appModel()->rowForDesktopFileName(desktopFileName) >= 0;
    };
    return controller->present(target, rectangleAnchor(itemRectangle), nodes,
                               [this, controller, desktopFileName, windows, windowActions,
                                desktopActions, pinned](const QString &token) {
        if (!m_dock)
            return false;
        if (token == QStringLiteral("dock.new-window"))
            return m_dock->launchNewWindow(desktopFileName);
        if (windowActions.contains(token)) {
            const bool accepted = m_dock->activateWindow(desktopFileName, windowActions.value(token));
            if (!accepted)
                controller->invalidateTarget();
            return accepted;
        }
        if (token == QStringLiteral("dock.close")) {
            bool accepted = false;
            for (const auto &window : windows)
                accepted = m_dock->closeWindow(desktopFileName, window.id) || accepted;
            if (!accepted)
                controller->invalidateTarget();
            return accepted;
        }
        if (desktopActions.contains(token)) {
            if (!m_launcher)
                return false;
            const DesktopEntryAction action = desktopActions.value(token);
            ApplicationLaunchRequest request;
            request.exec = action.exec;
            request.appName = desktopFileName;
            request.iconName = action.icon;
            if (m_catalog) {
                const auto record = m_catalog->findByDesktopFileName(desktopFileName);
                if (record) {
                    request.appName = record->name;
                    if (request.iconName.isEmpty())
                        request.iconName = record->icon;
                    request.desktopFilePath = record->sourceFilePath;
                }
            }
            m_launcher->launchDesktop(request);
            return true;
        }
        if (token == (pinned ? QStringLiteral("dock.unpin") : QStringLiteral("dock.pin")))
            return m_dock->setPinned(desktopFileName, !pinned);
        return false;
    }, validator);
}

bool TrayContextMenuAdapter::present(ContextMenuController *controller, const QString &itemKey,
                                     const QPoint &point, const QString &outputKey) const
{
    if (!controller || !m_service || itemKey.isEmpty()
        || !m_service->hasUsableMenuForItem(itemKey))
        return false;

    controller->setTrayService(m_service);
    m_service->prepareMenuForPresentation(itemKey);
    const ContextMenuTarget target{ContextMenuTarget::Kind::TrayItem, itemKey, outputKey};
    return controller->present(target, pointAnchor(point), {},
                               [this, itemKey](const QString &token) {
        if (!m_service || !token.startsWith(QStringLiteral("tray.node.")))
            return false;
        bool ok = false;
        const int nodeId = token.mid(QStringLiteral("tray.node.").size()).toInt(&ok);
        if (!ok || nodeId <= 0)
            return false;
        auto *model = qobject_cast<Astrea::StatusNotifier::DBusMenuModel *>(
            m_service->menuModelForItem(itemKey));
        if (!model)
            return false;
        const auto node = model->nodeById(nodeId);
        if (node.id != nodeId || !node.visible || !node.enabled || node.separator
            || !node.children.isEmpty())
            return false;
        model->activate(nodeId);
        return true;
    }, [this, itemKey] {
        return m_service && m_service->hasUsableMenuForItem(itemKey)
            && m_service->menuModelForItem(itemKey);
    });
}

} // namespace Astrea::Shell
