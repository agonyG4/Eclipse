#pragma once

#include "platform/wayland/effects/AstreaAnalyticBackdropCoverage.hpp"
#include "platform/wayland/effects/AstreaBackdropAnalyticCoverage.hpp"
#include "platform/wayland/effects/AstreaBackdropRegion.hpp"

#include <QQuickItem>
#include <QPointer>
#include <QSize>
#include <QVector>

#include <QtQml/QQmlListProperty>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <optional>

class AstreaBackgroundEffectBinding;
class QQuickWindow;

class AstreaBackdropEffectRegions : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(BackdropEffectRegions)
    Q_CLASSINFO("DefaultProperty", "regions")
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QQmlListProperty<AstreaBackdropRegion> regions READ regions)
    Q_PROPERTY(AstreaBackdropAnalyticCoverage *analyticCoverage READ analyticCoverage
                   WRITE setAnalyticCoverage NOTIFY analyticCoverageChanged)

public:
    explicit AstreaBackdropEffectRegions(QQuickItem *parent = nullptr);
    ~AstreaBackdropEffectRegions() override;

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    QQmlListProperty<AstreaBackdropRegion> regions();
    AstreaBackdropAnalyticCoverage *analyticCoverage() const { return m_analyticCoverage; }
    void setAnalyticCoverage(AstreaBackdropAnalyticCoverage *coverage);

    // Kept public for deterministic geometry tests and for callers that need
    // to inspect the exact integer protocol decomposition without a Wayland
    // connection.
    QVector<QRect> resolvedRegion() const;
    std::optional<AstreaAnalyticBackdropCoverage> resolvedAnalyticCoverage() const;
    QVector<QRect> resolvedAnalyticWorkRegion() const;
    static QVector<QRect> analyticWorkRegion(
        const AstreaAnalyticBackdropCoverage &coverage, QSize surfaceSize);
    static constexpr int analyticCoverageGuard = 2;

signals:
    void enabledChanged();
    void resolvedRegionChanged();
    void analyticCoverageChanged();

protected:
    void componentComplete() override;
    void itemChange(ItemChange change, const ItemChangeData &data) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    static void appendRegion(QQmlListProperty<AstreaBackdropRegion> *property,
                             AstreaBackdropRegion *region);
    static qsizetype regionCount(QQmlListProperty<AstreaBackdropRegion> *property);
    static AstreaBackdropRegion *regionAt(QQmlListProperty<AstreaBackdropRegion> *property,
                                           qsizetype index);
    static void clearRegions(QQmlListProperty<AstreaBackdropRegion> *property);

    void connectRegion(AstreaBackdropRegion *region);
    void refreshConnections();
    void scheduleSync();
    void syncForCurrentAnimationFrame();
    void sync();
    void sync(bool requestFrame);

    bool m_enabled = true;
    bool m_componentComplete = false;
    bool m_syncScheduled = false;
    QVector<AstreaBackdropRegion *> m_regions;
    QPointer<AstreaBackdropAnalyticCoverage> m_analyticCoverage;
    QVector<QMetaObject::Connection> m_connections;
    QPointer<QQuickWindow> m_observedWindow;
    std::unique_ptr<AstreaBackgroundEffectBinding> m_binding;
    QVector<QRect> m_lastResolvedRegion;
};
