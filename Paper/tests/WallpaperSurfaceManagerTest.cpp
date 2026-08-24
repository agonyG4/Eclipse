#include "platform/wayland/WallpaperSurfaceManager.hpp"
#include "platform/wayland/WallpaperSurfaceBundle.hpp"

#include "core/WallpaperPersistence.hpp"
#include "core/WallpaperService.hpp"

#include <QGuiApplication>
#include <QImage>
#include <QSaveFile>
#include <QQmlComponent>
#include <QQmlApplicationEngine>
#include <QScreen>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <memory>

using namespace Paper;

class FakeWallpaperSurfaceBundle final : public WallpaperSurfaceBundle
{
public:
    FakeWallpaperSurfaceBundle(QScreen *screen, QObject *parent)
        : WallpaperSurfaceBundle(screen, nullptr, nullptr, parent)
    {
    }

    bool initialize(QString *) override
    {
        initialized = true;
        layerConfigured = true;
        return true;
    }

    void map() override { mapped = true; }

    void setEffectiveWallpaper(const WallpaperDescriptor &descriptor) override
    {
        effective = descriptor;
    }

    bool initialized = false;
    bool layerConfigured = false;
    bool mapped = false;
    WallpaperDescriptor effective;
};

class WallpaperSurfaceManagerTest final : public QObject
{
    Q_OBJECT

private slots:
    void createsOneBackgroundBundlePerCurrentScreen();
    void forwardsOnlyEffectiveWallpaperToBundles();
    void reloadsSameSourceForEachNewGeneration();

private:
    static QString writeImage(const QString &path, QColor color = Qt::red);
    static void replaceImage(const QString &path, QColor color);
};

QString WallpaperSurfaceManagerTest::writeImage(const QString &path, const QColor color)
{
    QImage image(20, 20, QImage::Format_ARGB32);
    image.fill(color);
    if (!image.save(path)) {
        qFatal("Could not create test image at %s", qPrintable(path));
    }
    return path;
}

void WallpaperSurfaceManagerTest::replaceImage(const QString &path, const QColor color)
{
    QImage image(20, 20, QImage::Format_ARGB32);
    image.fill(color);
    QSaveFile replacement(path);
    QVERIFY2(replacement.open(QIODevice::WriteOnly), qPrintable(replacement.errorString()));
    QVERIFY(image.save(&replacement, "PNG"));
    QVERIFY2(replacement.commit(), qPrintable(replacement.errorString()));
}

void WallpaperSurfaceManagerTest::createsOneBackgroundBundlePerCurrentScreen()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             std::make_unique<XdgWallpaperPersistence>(
                                 temp.filePath(QStringLiteral("paper.ini"))));
    service.initialize();
    QGuiApplication *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    QVERIFY(application);
    QQmlApplicationEngine engine;
    QList<FakeWallpaperSurfaceBundle *> bundles;
    WallpaperSurfaceManager manager(
        *application, engine, &service, nullptr,
        [&bundles](QScreen *screen, QObject *parent) {
            auto *bundle = new FakeWallpaperSurfaceBundle(screen, parent);
            bundles.append(bundle);
            return bundle;
        });

    QString error;
    QVERIFY2(manager.initialize(&error), qPrintable(error));
    QCOMPARE(manager.bundleCount(), application->screens().size());
    QCOMPARE(bundles.size(), application->screens().size());
    for (const auto *bundle : bundles) {
        QVERIFY(bundle->initialized);
        QVERIFY(bundle->layerConfigured);
        QVERIFY(bundle->mapped);
    }
}

void WallpaperSurfaceManagerTest::forwardsOnlyEffectiveWallpaperToBundles()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto selected = writeImage(temp.filePath(QStringLiteral("selected.png")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             std::make_unique<XdgWallpaperPersistence>(
                                 temp.filePath(QStringLiteral("paper.ini"))));
    service.initialize();
    auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    QVERIFY(application);
    QQmlApplicationEngine engine;
    QList<FakeWallpaperSurfaceBundle *> bundles;
    WallpaperSurfaceManager manager(
        *application, engine, &service, nullptr,
        [&bundles](QScreen *screen, QObject *parent) {
            auto *bundle = new FakeWallpaperSurfaceBundle(screen, parent);
            bundles.append(bundle);
            return bundle;
        });
    QVERIFY(manager.initialize());
    QVERIFY(!bundles.isEmpty());
    QCOMPARE(bundles.constFirst()->effective.logicalId(),
             QStringLiteral("astrea://wallpaper/default"));

    service.setWallpaper(WallpaperDescriptor::externalFile(selected, WallpaperFit::Contain));
    QTRY_COMPARE_WITH_TIMEOUT(bundles.constFirst()->effective.source(), selected, 1000);
    QCOMPARE(bundles.constFirst()->effective.fit(), WallpaperFit::Contain);
}

void WallpaperSurfaceManagerTest::reloadsSameSourceForEachNewGeneration()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto source = writeImage(temp.filePath(QStringLiteral("same-path.png")), Qt::red);

    QQmlApplicationEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Paper/qml/WallpaperSurface.qml")));
    if (component.status() != QQmlComponent::Ready) {
        QFAIL(qPrintable(component.errors().isEmpty()
                              ? QStringLiteral("Wallpaper QML component was not ready")
                              : component.errors().constFirst().toString()));
    }

    const QVariantMap properties{
        {QStringLiteral("wallpaperSource"), QUrl::fromLocalFile(source).toString()},
        {QStringLiteral("wallpaperGeneration"), 1},
        {QStringLiteral("debugEnabled"), true},
    };
    auto *window = qobject_cast<QQuickWindow *>(component.createWithInitialProperties(properties));
    QVERIFY(window);
    QScopedPointer<QQuickWindow> windowGuard(window);

    QTRY_COMPARE_WITH_TIMEOUT(window->property("visibleGeneration").toInt(), 1, 2000);
    const auto initialStarts = window->property("loadStartCount").toInt();
    QVERIFY(initialStarts >= 1);

    replaceImage(source, Qt::blue);
    window->setProperty("wallpaperGeneration", 2);
    QTRY_COMPARE_WITH_TIMEOUT(window->property("visibleGeneration").toInt(), 2, 2000);
    QVERIFY(window->property("loadStartCount").toInt() > initialStarts);
    const auto activeSlot = window->property("activeSlot").toInt();
    const auto activeObjectName = activeSlot == 0 ? QStringLiteral("front")
                                                  : QStringLiteral("back");
    auto *activeImage = window->findChild<QObject *>(activeObjectName);
    QVERIFY(activeImage);
    QVERIFY(activeImage->property("source").toString().contains(
        QStringLiteral("astreaGeneration=2")));

    replaceImage(source, Qt::green);
    window->setProperty("wallpaperGeneration", 3);
    replaceImage(source, Qt::yellow);
    window->setProperty("wallpaperGeneration", 4);
    QTRY_COMPARE_WITH_TIMEOUT(window->property("visibleGeneration").toInt(), 4, 3000);
    QVERIFY(window->property("loadStartCount").toInt() >= initialStarts + 3);
}

QTEST_MAIN(WallpaperSurfaceManagerTest)
#include "WallpaperSurfaceManagerTest.moc"
