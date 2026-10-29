// Captures every Settings route from the real application, against the real
// session services, for design review. Usage:
//   QT_QPA_PLATFORM=offscreen settings-route-gallery OUTPUT_DIR [WIDTH] [ROUTE...]
#include "app/SettingsApplication.hpp"
#include "app/SettingsFontPolicy.hpp"

#include <QColor>
#include <QDir>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <functional>

namespace {

const QStringList kRoutes = {
    QStringLiteral("system"),        QStringLiteral("internet"),
    QStringLiteral("bluetooth"),     QStringLiteral("devices"),
    QStringLiteral("audio"),         QStringLiteral("display"),
    QStringLiteral("compositor"),    QStringLiteral("performance"),
    QStringLiteral("performance-settings"), QStringLiteral("proton"),
    QStringLiteral("customization"), QStringLiteral("appearance"),
    QStringLiteral("visual-effects"), QStringLiteral("icons"),
    QStringLiteral("wallpaper"),     QStringLiteral("dock"),
    QStringLiteral("animations"),    QStringLiteral("screenshots"),
    QStringLiteral("island"),        QStringLiteral("more-settings"),
    QStringLiteral("compatibility"), QStringLiteral("language"),
    QStringLiteral("apps"),          QStringLiteral("user-profile"),
};

void wait(int milliseconds) {
  QEventLoop loop;
  QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
  loop.exec();
}

// The page's own scroller is the widest visible flickable that can scroll.
QQuickItem *pageScroller(QQuickItem *root) {
  QQuickItem *best = nullptr;
  std::function<void(QQuickItem *)> visit = [&](QQuickItem *item) {
    if (!item->isVisible())
      return;
    if (item->property("contentY").isValid() &&
        item->property("contentHeight").toReal() > item->height() + 1 &&
        (!best || item->width() > best->width()))
      best = item;
    for (QQuickItem *child : item->childItems())
      visit(child);
  };
  visit(root);
  return best;
}

} // namespace

int main(int argc, char *argv[]) {
  QQuickStyle::setStyle(QStringLiteral("Basic"));
  QGuiApplication app(argc, argv);
  applySettingsFontPolicy(app);
  const QStringList arguments = app.arguments();
  if (arguments.size() < 2) {
    qCritical("usage: settings-route-gallery OUTPUT_DIR [WIDTH] [ROUTE...]");
    return 2;
  }
  const QDir output(arguments.at(1));
  QDir().mkpath(output.path());
  const int width = arguments.size() > 2 ? arguments.at(2).toInt() : 1100;
  const QStringList routes = arguments.size() > 3 ? arguments.mid(3) : kRoutes;

  SettingsApplication settings(app);
  QTimer::singleShot(0, [&] {
    auto *window = qobject_cast<QQuickWindow *>(QGuiApplication::topLevelWindows().value(0));
    if (!window) {
      qCritical("no Settings window");
      app.exit(1);
      return;
    }
    window->resize(width, 860);
    // The real window is translucent over the compositor's blur; stand in an opaque backdrop.
    window->setColor(QColor(qEnvironmentVariable("ASTREA_GALLERY_BACKDROP", QStringLiteral("#26262b"))));
    QObject *controller =
        qmlContext(window->contentItem())->contextProperty(QStringLiteral("SettingsController"))
            .value<QObject *>();
    for (const QString &route : routes) {
      bool navigated = false;
      QMetaObject::invokeMethod(controller, "navigateTo", Q_RETURN_ARG(bool, navigated),
                                Q_ARG(QString, route));
      if (!navigated) {
        qWarning("could not navigate to %s", qPrintable(route));
        continue;
      }
      wait(1500);
      window->grabWindow().save(output.filePath(route + QStringLiteral("-0.png")));
      QQuickItem *scroller = pageScroller(window->contentItem());
      for (int page = 1; scroller && page < 8; ++page) {
        const qreal bottom = scroller->property("contentHeight").toReal() - scroller->height();
        const qreal current = scroller->property("contentY").toReal();
        if (current >= bottom - 1)
          break;
        scroller->setProperty("contentY", std::min(bottom, current + scroller->height() * 0.85));
        wait(250);
        window->grabWindow().save(
            output.filePath(route + QStringLiteral("-%1.png").arg(page)));
      }
      qInfo("captured %s", qPrintable(route));
    }
    app.quit();
  });
  return settings.run();
}
