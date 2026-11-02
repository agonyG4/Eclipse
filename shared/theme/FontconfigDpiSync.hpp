#pragma once
#include "SystemFontConfig.hpp"
#include <QObject>
#include <QString>
#include <QTimer>

class QScreen;

namespace Astrea {
// Keeps the fontconfig dpi in step with the primary output across hotplug and scale changes.
class FontconfigDpiSync final : public QObject {
    Q_OBJECT
public:
    // An empty path targets the user's fonts.conf; drmRoot is where connector EDIDs live.
    explicit FontconfigDpiSync(const QString &path = {}, const QString &drmRoot = QStringLiteral("/sys/class/drm"),
                               QObject *parent = nullptr);
    void start();
    // Resolution, size and the scale windows actually render at on screen.
    SystemFontConfig::OutputDensity densityFor(QScreen *screen) const;
    // Physical size from the connector's EDID, for outputs whose wl_output geometry reports none.
    static QSize edidSizeMm(const QString &drmRoot, const QString &output);
    bool syncNow(QString *error = nullptr);
    QString path() const { return m_path; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void watch(QScreen *screen);
    void schedule();

    QString m_path;
    QString m_drmRoot;
    QTimer m_debounce;
    SystemFontConfig::OutputDensity m_written;
    bool m_hasWritten = false;
};
}
