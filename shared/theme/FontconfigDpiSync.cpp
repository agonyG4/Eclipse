#include "FontconfigDpiSync.hpp"
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QScreen>
#include <QStandardPaths>
#include <QWindow>
#include <cmath>

namespace Astrea {
namespace {
bool sameDensity(const SystemFontConfig::OutputDensity &a, const SystemFontConfig::OutputDensity &b)
{
    return a.name == b.name && a.pixels == b.pixels && a.physicalMm == b.physicalMm && a.scale == b.scale;
}
}

FontconfigDpiSync::FontconfigDpiSync(const QString &path, const QString &drmRoot, QObject *parent)
    : QObject(parent),
      m_path(path.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                                  + QStringLiteral("/fontconfig/fonts.conf")
                            : path),
      m_drmRoot(drmRoot)
{
    // Hotplug and scale changes arrive as bursts of screen and window signals.
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(250);
    connect(&m_debounce, &QTimer::timeout, this, [this] {
        QString error;
        if (!syncNow(&error)) qWarning("Astrea fontconfig dpi update failed: %s", qPrintable(error));
    });
}

void FontconfigDpiSync::start()
{
    auto *app = qGuiApp;
    for (QScreen *screen : app->screens()) watch(screen);
    connect(app, &QGuiApplication::screenAdded, this, [this](QScreen *screen) { watch(screen); schedule(); });
    connect(app, &QGuiApplication::screenRemoved, this, &FontconfigDpiSync::schedule);
    connect(app, &QGuiApplication::primaryScreenChanged, this, &FontconfigDpiSync::schedule);
    // Fractional scale reaches windows, not QScreen.
    app->installEventFilter(this);
    schedule();
}

void FontconfigDpiSync::watch(QScreen *screen)
{
    connect(screen, &QScreen::geometryChanged, this, &FontconfigDpiSync::schedule);
    connect(screen, &QScreen::physicalSizeChanged, this, &FontconfigDpiSync::schedule);
    connect(screen, &QScreen::logicalDotsPerInchChanged, this, &FontconfigDpiSync::schedule);
}

void FontconfigDpiSync::schedule() { m_debounce.start(); }

bool FontconfigDpiSync::eventFilter(QObject *watched, QEvent *event)
{
    if ((event->type() == QEvent::DevicePixelRatioChange || event->type() == QEvent::ScreenChangeInternal)
        && watched->isWindowType())
        schedule();
    return QObject::eventFilter(watched, event);
}

SystemFontConfig::OutputDensity FontconfigDpiSync::densityFor(QScreen *screen) const
{
    SystemFontConfig::OutputDensity density;
    if (!screen) return density;
    density.name = screen->name();
    density.scale = screen->devicePixelRatio();
    for (QWindow *window : QGuiApplication::topLevelWindows()) {
        if (window->isVisible() && window->screen() == screen) {
            density.scale = window->devicePixelRatio();
            break;
        }
    }
    const QSize logical = screen->geometry().size();
    density.pixels = QSize(int(std::lround(logical.width() * density.scale)),
                           int(std::lround(logical.height() * density.scale)));
    const QSizeF mm = screen->physicalSize();
    if (mm.width() >= 1 && mm.height() >= 1)
        density.physicalMm = QSize(int(std::lround(mm.width())), int(std::lround(mm.height())));
    if (density.physicalMm.isEmpty()) density.physicalMm = edidSizeMm(m_drmRoot, density.name);
    return density;
}

QSize FontconfigDpiSync::edidSizeMm(const QString &drmRoot, const QString &output)
{
    if (output.isEmpty()) return {};
    // Connectors are named card<N>-<output>.
    const auto connectors = QDir(drmRoot).entryList({QStringLiteral("card*-") + output}, QDir::Dirs);
    for (const QString &connector : connectors) {
        QFile edid(drmRoot + QLatin1Char('/') + connector + QStringLiteral("/edid"));
        if (!edid.open(QIODevice::ReadOnly)) continue;
        const QSize size = SystemFontConfig::edidPhysicalSizeMm(edid.read(128));
        if (!size.isEmpty()) return size;
    }
    return {};
}

bool FontconfigDpiSync::syncNow(QString *error)
{
    if (error) error->clear();
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return true;
    const auto density = densityFor(screen);
    if (m_hasWritten && sameDensity(density, m_written)) return true;
    if (!SystemFontConfig::applyDpi(m_path, density, error)) return false;
    m_written = density;
    m_hasWritten = true;
    return true;
}
}
