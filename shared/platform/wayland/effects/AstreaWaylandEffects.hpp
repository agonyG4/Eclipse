#pragma once

#include <QHash>
#include <QObject>
#include <QRect>
#include <QString>
#include <QVector>

#include <cstdint>
#include <memory>

class QWindow;
struct wl_compositor;
struct wl_display;
struct wl_surface;
struct ext_background_effect_manager_v1;
struct ext_background_effect_surface_v1;

class AstreaWaylandEffects final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

public:
    static AstreaWaylandEffects *instance();
    explicit AstreaWaylandEffects(QObject *parent = nullptr);
    ~AstreaWaylandEffects() override;

    bool initialize();
    bool available() const { return m_available; }
    QString errorString() const { return m_error; }

    bool setBlurRegion(QWindow *window, const QVector<QRect> &rectangles);
    void clear(QWindow *window);

signals:
    void availableChanged();

private:
    static void handleCapabilities(void *data, ext_background_effect_manager_v1 *manager,
                                   uint32_t flags);
    void setError(const QString &error);

    wl_display *m_display = nullptr;
    wl_compositor *m_compositor = nullptr;
    ext_background_effect_manager_v1 *m_manager = nullptr;
    QHash<wl_surface *, ext_background_effect_surface_v1 *> m_surfaces;
    QString m_error;
    uint32_t m_capabilities = 0;
    bool m_initialized = false;
    bool m_available = false;
};
