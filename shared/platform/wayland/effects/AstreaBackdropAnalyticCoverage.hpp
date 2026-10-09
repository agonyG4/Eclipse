#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QPointF>
#include <QRectF>

#include <QtQml/qqmlregistration.h>

class AstreaBackdropAnalyticCoverage : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(BackdropAnalyticCoverage)
    Q_PROPERTY(QQuickItem *item READ item WRITE setItem NOTIFY itemChanged)
    Q_PROPERTY(bool roundedRectEnabled READ roundedRectEnabled WRITE setRoundedRectEnabled
                   NOTIFY roundedRectEnabledChanged)
    Q_PROPERTY(QRectF roundedRect READ roundedRect WRITE setRoundedRect NOTIFY roundedRectChanged)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged)
    Q_PROPERTY(bool triangleEnabled READ triangleEnabled WRITE setTriangleEnabled
                   NOTIFY triangleEnabledChanged)
    Q_PROPERTY(QPointF triangleA READ triangleA WRITE setTriangleA NOTIFY triangleAChanged)
    Q_PROPERTY(QPointF triangleB READ triangleB WRITE setTriangleB NOTIFY triangleBChanged)
    Q_PROPERTY(QPointF triangleC READ triangleC WRITE setTriangleC NOTIFY triangleCChanged)

public:
    explicit AstreaBackdropAnalyticCoverage(QObject *parent = nullptr);

    QQuickItem *item() const { return m_item; }
    void setItem(QQuickItem *item);
    bool roundedRectEnabled() const { return m_roundedRectEnabled; }
    void setRoundedRectEnabled(bool enabled);
    QRectF roundedRect() const { return m_roundedRect; }
    void setRoundedRect(const QRectF &rect);
    qreal radius() const { return m_radius; }
    void setRadius(qreal radius);
    bool triangleEnabled() const { return m_triangleEnabled; }
    void setTriangleEnabled(bool enabled);
    QPointF triangleA() const { return m_triangleA; }
    void setTriangleA(const QPointF &point);
    QPointF triangleB() const { return m_triangleB; }
    void setTriangleB(const QPointF &point);
    QPointF triangleC() const { return m_triangleC; }
    void setTriangleC(const QPointF &point);

signals:
    void itemChanged();
    void roundedRectEnabledChanged();
    void roundedRectChanged();
    void radiusChanged();
    void triangleEnabledChanged();
    void triangleAChanged();
    void triangleBChanged();
    void triangleCChanged();

private:
    QPointer<QQuickItem> m_item;
    QRectF m_roundedRect;
    qreal m_radius = 0.0;
    QPointF m_triangleA;
    QPointF m_triangleB;
    QPointF m_triangleC;
    bool m_roundedRectEnabled = false;
    bool m_triangleEnabled = false;
};
