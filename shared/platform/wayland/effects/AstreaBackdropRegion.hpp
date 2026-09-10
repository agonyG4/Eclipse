#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickItem>

#include <QtQml/qqmlregistration.h>

class AstreaBackdropRegion : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(BackdropRegion)
    Q_PROPERTY(QQuickItem *item READ item WRITE setItem NOTIFY itemChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged)

public:
    explicit AstreaBackdropRegion(QObject *parent = nullptr);

    QQuickItem *item() const { return m_item; }
    void setItem(QQuickItem *item);
    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    qreal radius() const { return m_radius; }
    void setRadius(qreal radius);

signals:
    void itemChanged();
    void enabledChanged();
    void radiusChanged();

private:
    QPointer<QQuickItem> m_item;
    bool m_enabled = true;
    qreal m_radius = 0.0;
};
