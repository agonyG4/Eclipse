#include <AstreaSDK/Core.hpp>

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPointer>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlImageProviderBase>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QThread>
#include <QElapsedTimer>
#include <QDebug>

#include <memory>

namespace {

class ConflictingImageProvider final : public QQuickImageProvider {
public:
    ConflictingImageProvider()
        : QQuickImageProvider(QQuickImageProvider::Pixmap)
    {
    }

    QPixmap requestPixmap(const QString &, QSize *, const QSize &) override
    {
        return {};
    }
};

bool writeTheme(const QString &dataHome)
{
    const QString theme = QDir(dataHome).filePath(
        QStringLiteral("icons/AstreaSDKFixture"));
    const QString iconDirectory = QDir(theme).filePath(
        QStringLiteral("32x32/apps"));
    if (!QDir().mkpath(iconDirectory))
        return false;

    QFile index(QDir(theme).filePath(QStringLiteral("index.theme")));
    if (!index.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    const QByteArray content =
        "[Icon Theme]\n"
        "Name=Astrea SDK fixture\n"
        "Directories=32x32/apps\n"
        "Inherits=hicolor\n\n"
        "[32x32/apps]\n"
        "Size=32\nType=Fixed\nContext=Applications\n";
    if (index.write(content) != content.size())
        return false;

    QImage icon(32, 32, QImage::Format_ARGB32_Premultiplied);
    icon.fill(QColor(QStringLiteral("#19c37d")));
    QPainter painter(&icon);
    painter.setPen(QColor(QStringLiteral("#ffffff")));
    painter.drawRect(7, 7, 17, 17);
    painter.end();
    return icon.save(QDir(iconDirectory).filePath(
        QStringLiteral("astrea-sdk-probe.png")));
}

QQuickItem *createIcon(QQmlEngine &engine, const QString &iconName,
                       const QString &appName, const QString &fallbackColor)
{
    QQmlComponent component(&engine);
    component.setData(R"qml(
        import QtQuick
        import Astrea.Shared 1.0
        AstreaAppIcon {
            width: 64
            height: 64
            iconName: ""
            iconRadius: 0
            showFallbackText: false
        }
    )qml", QUrl(QStringLiteral("file:///astrea-sdk-consumer.qml")));
    if (component.isError()) {
        qCritical().noquote() << component.errorString();
        return nullptr;
    }
    QObject *object = component.create();
    auto *item = qobject_cast<QQuickItem *>(object);
    if (!item) {
        delete object;
        qCritical().noquote() << component.errorString();
        return nullptr;
    }
    item->setProperty("iconName", iconName);
    item->setProperty("appName", appName);
    item->setProperty("showFallbackText", !appName.isEmpty());
    item->setProperty("fallbackColor", QColor(fallbackColor));
    item->setProperty("fallbackRadius", 0);
    return item;
}

QImage render(QQmlEngine &engine, const QString &iconName,
              const QString &appName = {},
              const QString &fallbackColor = QStringLiteral("#ff123456"))
{
    QQuickWindow window;
    window.setColor(Qt::transparent);
    window.setGeometry(0, 0, 64, 64);
    std::unique_ptr<QQuickItem> icon(createIcon(engine, iconName, appName,
                                                fallbackColor));
    if (!icon)
        return {};
    icon->setParentItem(window.contentItem());
    window.show();

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5000) {
        QCoreApplication::processEvents();
        if (icon->property("ready").toBool())
            break;
        QThread::msleep(5);
    }
    if (icon->property("ready").toBool()) {
        QCoreApplication::processEvents();
        return window.grabWindow();
    }

    // The unknown-icon case intentionally has no ready image. Allow its
    // QML fallback rectangle to reach the scene graph before capturing.
    QCoreApplication::processEvents();
    QThread::msleep(50);
    return window.grabWindow();
}

bool hasPixel(const QImage &image, const QPoint &point, const QColor &color)
{
    return !image.isNull() && image.rect().contains(point)
        && image.pixelColor(point) == color;
}

bool installedSharedTypesWork(QQmlEngine &engine)
{
    QQmlComponent component(&engine);
    component.setData(R"qml(
        import QtQuick
        import Astrea.Shared 1.0
        Item {
            property color tokenColor: themeTokens.value("accent", "#123456")
            property alias loader: themeLoader
            ThemeTokens { id: themeTokens }
            ThemeComponentLoader {
                id: themeLoader
                fallbackComponent: Component { Rectangle { objectName: "sdk-theme-fallback"; width: 40; height: 30 } }
            }
        }
    )qml", QUrl::fromLocalFile(QStringLiteral("/relocated-sdk-consumer.qml")));
    if (!component.isReady()) {
        qCritical().noquote() << "Installed shared QML types did not load:" << component.errorString();
        return false;
    }
    std::unique_ptr<QObject> root(component.create());
    if (!root) {
        qCritical().noquote() << "Installed shared QML types did not instantiate:" << component.errorString();
        return false;
    }
    QObject *loader = root->property("loader").value<QObject *>();
    QObject *fallback = loader ? loader->property("item").value<QObject *>() : nullptr;
    return fallback && fallback->objectName() == QStringLiteral("sdk-theme-fallback")
        && root->property("tokenColor").value<QColor>() == QColor(QStringLiteral("#123456"));
}

} // namespace

int main(int argc, char **argv)
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    const QString dataHome = qEnvironmentVariable("ASTREA_SDK_TEST_DATA_HOME");
    if (dataHome.isEmpty() || !QDir().mkpath(dataHome) || !writeTheme(dataHome)) {
        qCritical("Could not create the deterministic icon theme fixture");
        return 1;
    }
    qputenv("XDG_DATA_HOME", dataHome.toUtf8());
    qputenv("ASTREA_ICON_THEME", "AstreaSDKFixture");

    QGuiApplication application(argc, argv);
    QPointer<QObject> conflictingProviderObject;
    {
        QQmlEngine conflictingEngine;
        auto *conflictingProvider = new ConflictingImageProvider;
        conflictingProviderObject = dynamic_cast<QObject *>(conflictingProvider);
        conflictingEngine.addImageProvider(QStringLiteral("astrea-icon"),
                                           conflictingProvider);
        QString conflictError;
        if (AstreaSDK::Core::initializeQmlEngine(conflictingEngine, &conflictError)
            || conflictingEngine.imageProvider(QStringLiteral("astrea-icon"))
                != conflictingProvider
            || conflictingEngine.rootContext()->contextProperty(
                   QStringLiteral("AstreaIconProvider")).isValid()) {
            qCritical("SDK initialization did not reject the conflicting provider cleanly");
            return 1;
        }
    }
    if (!conflictingProviderObject.isNull()) {
        qCritical("The conflicting engine failed to destroy its provider");
        return 1;
    }

    QPointer<QObject> providerObject;
    {
        QQmlEngine engine;
        QString error;
        if (!AstreaSDK::Core::initializeQmlEngine(engine, &error)) {
            qCritical().noquote() << "SDK initialization failed:" << error;
            return 1;
        }
        if (!installedSharedTypesWork(engine)) {
            qCritical("The relocated SDK did not expose ThemeTokens and ThemeComponentLoader");
            return 1;
        }

        QQmlImageProviderBase *providerBase = engine.imageProvider(
            QStringLiteral("astrea-icon"));
        auto *provider = dynamic_cast<QQuickImageProvider *>(providerBase);
        if (!provider) {
            qCritical("SDK did not register the astrea-icon image provider");
            return 1;
        }
        providerObject = dynamic_cast<QObject *>(providerBase);
        if (!providerObject) {
            qCritical("Astrea image provider is not QObject-managed");
            return 1;
        }
        if (!AstreaSDK::Core::initializeQmlEngine(engine, &error)
            || engine.imageProvider(QStringLiteral("astrea-icon")) != providerBase) {
            qCritical().noquote() << "SDK engine initialization was not idempotent:" << error;
            return 1;
        }

        QSize missingSize;
        const QPixmap missing = provider->requestPixmap(
            QStringLiteral("astrea-sdk-guaranteed-missing-probe"),
            &missingSize, QSize(32, 32));
        if (!missing.isNull()) {
            qCritical("Unknown icon unexpectedly resolved through the SDK provider");
            return 1;
        }

        const QImage icon = render(engine, QStringLiteral("astrea-sdk-probe"));
        if (icon.isNull() || !hasPixel(icon, QPoint(32, 32), QColor("#19c37d"))) {
            qCritical("The installed QML module did not render the themed icon provider");
            return 1;
        }

        const QImage fallback = render(
            engine, QStringLiteral("astrea-sdk-guaranteed-missing-probe"),
            QStringLiteral("SDK Fallback"));
        if (!hasPixel(fallback, QPoint(2, 2), QColor("#123456"))) {
            qCritical("AstreaAppIcon did not render its unknown-icon QML fallback");
            return 1;
        }
    }

    if (!providerObject.isNull()) {
        qCritical("The engine did not destroy its owned icon provider");
        return 1;
    }
    return 0;
}
