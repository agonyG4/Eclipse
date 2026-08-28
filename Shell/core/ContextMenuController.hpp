#pragma once

#include "ContextMenuModel.hpp"

#include <QObject>
#include <QPointer>
#include <QPoint>
#include <QRect>
#include <QString>

#include <functional>
#include <utility>

namespace Astrea::Shell {

class DesktopContextMenuProvider;
class DockContextMenuProvider;
class TrayContextMenuAdapter;

struct ContextMenuTarget {
    enum class Kind {
        Desktop,
        DockApplication,
        TrayItem,
    };

    Kind kind = Kind::Desktop;
    QString identity;
    QString outputKey;
};

struct ContextMenuAnchor {
    enum class Kind {
        Point,
        Rectangle,
    };

    Kind kind = Kind::Point;
    QPoint point;
    QRect rectangle;
    int preferredTop = 0;
};

class ContextMenuController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(Lifecycle lifecycle READ lifecycle NOTIFY lifecycleChanged)
    Q_PROPERTY(quint64 presentationGeneration READ presentationGeneration NOTIFY presentationChanged)
    Q_PROPERTY(bool hasActivePresentation READ hasActivePresentation NOTIFY presentationChanged)
    Q_PROPERTY(ContextMenuModel *model READ model CONSTANT)
    Q_PROPERTY(QString targetIdentity READ targetIdentity NOTIFY presentationChanged)
    Q_PROPERTY(int targetKind READ targetKind NOTIFY presentationChanged)
    Q_PROPERTY(QObject *trayService READ trayService NOTIFY presentationChanged)
    Q_PROPERTY(QString outputKey READ outputKey NOTIFY presentationChanged)
    Q_PROPERTY(int anchorKind READ anchorKind NOTIFY presentationChanged)
    Q_PROPERTY(QPoint anchorPoint READ anchorPoint NOTIFY presentationChanged)
    Q_PROPERTY(QRect anchorRectangle READ anchorRectangle NOTIFY presentationChanged)
    Q_PROPERTY(bool debugEnabled READ debugEnabled CONSTANT)

public:
    enum class Lifecycle {
        Closed,
        Open,
        Closing,
    };
    Q_ENUM(Lifecycle)

    using ActivationHandler = std::function<bool(const QString &token)>;
    using TargetValidator = std::function<bool()>;
    using ActionAuthorizer = std::function<bool(const QString &token)>;

    explicit ContextMenuController(QObject *parent = nullptr);

    Lifecycle lifecycle() const { return m_lifecycle; }
    quint64 presentationGeneration() const { return m_presentationGeneration; }
    bool hasActivePresentation() const { return m_lifecycle != Lifecycle::Closed; }
    ContextMenuModel *model() const { return m_model.get(); }
    QString targetIdentity() const { return m_target.identity; }
    int targetKind() const { return static_cast<int>(m_target.kind); }
    QObject *trayService() const { return m_trayService.data(); }
    QString outputKey() const { return m_target.outputKey; }
    int anchorKind() const { return static_cast<int>(m_anchor.kind); }
    QPoint anchorPoint() const { return m_anchor.point; }
    QRect anchorRectangle() const { return m_anchor.rectangle; }
    bool debugEnabled() const;
    const ContextMenuTarget &target() const { return m_target; }

    bool present(const ContextMenuTarget &target,
                 const QVector<ContextMenuModel::NodeSpec> &nodes,
                 ActivationHandler activation,
                 TargetValidator targetValidator = {},
                 ActionAuthorizer actionAuthorizer = {});
    bool present(const ContextMenuTarget &target, const ContextMenuAnchor &anchor,
                 const QVector<ContextMenuModel::NodeSpec> &nodes,
                 ActivationHandler activation, TargetValidator targetValidator = {},
                 ActionAuthorizer actionAuthorizer = {});
    void setDesktopProvider(DesktopContextMenuProvider *provider) { m_desktopProvider = provider; }
    void setDockProvider(DockContextMenuProvider *provider) { m_dockProvider = provider; }
    void setTrayProvider(TrayContextMenuAdapter *provider) { m_trayProvider = provider; }
    void setTrayService(QObject *service) { m_trayService = service; }
    void setBarPopupCloser(std::function<void()> closer) { m_barPopupCloser = std::move(closer); }

    Q_INVOKABLE bool presentDesktop(int x, int y, const QString &outputKey);
    Q_INVOKABLE bool presentDock(const QString &desktopFileName, int x, int y,
                                 int width, int height, const QString &outputKey);
    Q_INVOKABLE bool presentTray(const QString &itemKey, int x, int y,
                                 int width, int height, int preferredTop,
                                 const QString &outputKey);
    Q_INVOKABLE QPoint menuPosition(int outputWidth, int outputHeight,
                                    int menuWidth, int menuHeight) const;
    Q_INVOKABLE QPoint submenuPosition(int outputWidth, int outputHeight,
                                        int menuWidth, int menuHeight,
                                        const QRect &parentRectangle,
                                        bool rightToLeft = false) const;
    Q_INVOKABLE void invalidateOutput(const QString &outputKey);
    Q_INVOKABLE bool activate(quint64 generation, const QString &token);
    Q_INVOKABLE void close();
    Q_INVOKABLE void completeClose();
    void invalidateTarget();
    void invalidateProvider();
    void shutdown();

signals:
    void lifecycleChanged();
    void presentationChanged();
    void closeRequested();
    void activationRejected(const QString &token);

private:
    Lifecycle m_lifecycle = Lifecycle::Closed;
    quint64 m_presentationGeneration = 0;
    ContextMenuTarget m_target;
    ContextMenuAnchor m_anchor;
    bool m_targetValid = false;
    std::unique_ptr<ContextMenuModel> m_model;
    ActivationHandler m_activation;
    TargetValidator m_targetValidator;
    ActionAuthorizer m_actionAuthorizer;
    QPointer<QObject> m_trayService;
    DesktopContextMenuProvider *m_desktopProvider = nullptr;
    DockContextMenuProvider *m_dockProvider = nullptr;
    TrayContextMenuAdapter *m_trayProvider = nullptr;
    std::function<void()> m_barPopupCloser;
};

} // namespace Astrea::Shell
