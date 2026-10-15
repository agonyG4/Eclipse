#include "core/DockController.hpp"
#include "core/DockSurfaceGeometry.hpp"
#include "platform/wayland/DockInputRegionBridge.hpp"
#include "platform/wayland/effects/AstreaBackdropEffectRegions.hpp"
#include "platform/wayland/effects/AstreaBackdropAnalyticCoverage.hpp"
#include "services/DockConfigPersistence.hpp"
#include "icons/AstreaIconProvider.hpp"
#include "theme/ThemeController.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QImage>
#include <QMetaProperty>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QPointingDevice>
#include <QRegularExpression>
#include <QRect>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QTest>
#include <QtMath>

Q_IMPORT_QML_PLUGIN(Astrea_EffectsPlugin)

#include <algorithm>
#include <functional>

namespace {

class FakeContextMenuController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasActivePresentation READ hasActivePresentation NOTIFY presentationChanged)

public:
    bool hasActivePresentation() const { return m_hasActivePresentation; }

    Q_INVOKABLE bool presentDock(const QString &desktopFileName, int x, int y, int width,
                                 int height, const QString &outputKey)
    {
        ++presentCount;
        m_hasActivePresentation = true;
        emit presentationChanged();
        lastDesktopFileName = desktopFileName;
        lastRectangle = QRect(x, y, width, height);
        lastOutputKey = outputKey;
        return true;
    }

    Q_INVOKABLE void clearPresentation()
    {
        if (!m_hasActivePresentation)
            return;
        m_hasActivePresentation = false;
        emit presentationChanged();
    }

    int presentCount = 0;
    QString lastDesktopFileName;
    QRect lastRectangle;
    QString lastOutputKey;

signals:
    void presentationChanged();

private:
    bool m_hasActivePresentation = false;
};

class FakeInputRegionBridge final : public QObject {
    Q_OBJECT

public:
    Q_INVOKABLE void update(const QRectF &chrome, const QVariantList &interactionRects,
                            int itemCount)
    {
        lastChrome = chrome;
        lastInteractionRects = interactionRects;
        lastItemCount = itemCount;
        ++updateCount;
    }

    QRectF lastChrome;
    QVariantList lastInteractionRects;
    int lastItemCount = 0;
    int updateCount = 0;

signals:
    void windowReady();
};

class FakeTooltipSurface final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool tooltipVisible READ tooltipVisible)

public:
    bool tooltipVisible() const { return m_tooltipVisible; }

    Q_INVOKABLE void showTooltip(const QString &title, const QRect &anchorRect,
                                 const QString &position)
    {
        m_tooltipVisible = !title.isEmpty();
        lastTitle = title;
        lastAnchorRect = anchorRect;
        lastPosition = position;
        shownTitles.append(title);
        if (identitySource)
            shownTaskKeys.append(identitySource->property("pointerTargetTaskKey").toString());
        ++showCount;
    }

    Q_INVOKABLE void hideTooltip()
    {
        m_tooltipVisible = false;
        lastTitle.clear();
        ++hideCount;
    }

    QString lastTitle;
    QRect lastAnchorRect;
    QString lastPosition;
    QStringList shownTitles;
    QStringList shownTaskKeys;
    QObject *identitySource = nullptr;
    int showCount = 0;
    int hideCount = 0;

private:
    bool m_tooltipVisible = false;
};

class QmlCountingPersistence final : public DockConfigPersistence {
public:
    QmlCountingPersistence() : DockConfigPersistence(QStringLiteral("/unused/dock.json")) {}

    bool writePins(const QStringList &pins, QString *errorOut = nullptr) override
    {
        ++calls;
        lastPins = pins;
        if (errorOut)
            errorOut->clear();
        return true;
    }

    int calls = 0;
    QStringList lastPins;
};

const QString kDockPanelPath = QString::fromUtf8(
    DOCK_SOURCE_DIR "/qml/components/DockPanel.qml");
const QString kDockMainPath = QString::fromUtf8(DOCK_SOURCE_DIR "/qml/Main.qml");

QQuickItem *delegate(QQuickItem *panel, const QString &identity)
{
    const QString taskKey = identity.startsWith(QStringLiteral("desktop:"))
        ? identity : QStringLiteral("desktop:") + identity;
    QQuickItem *result = nullptr;
    std::function<void(QQuickItem *)> visit = [&result, &identity, &taskKey,
                                               &visit](QQuickItem *item) {
        if (result || !item)
            return;
        if (item->objectName() == identity || item->objectName() == taskKey) {
            result = item;
            return;
        }
        for (QQuickItem *child : item->childItems())
            visit(child);
    };
    visit(panel);
    return result;
}

QQuickItem *iconItem(QQuickItem *delegateItem)
{
    QQuickItem *result = nullptr;
    std::function<void(QQuickItem *)> visit = [&result, &visit](QQuickItem *item) {
        if (result || !item)
            return;
        if (item->property("effectiveSourcePixelSize").isValid()) {
            result = item;
            return;
        }
        for (QQuickItem *child : item->childItems())
            visit(child);
    };
    visit(delegateItem);
    return result;
}

QQuickItem *childWithObjectName(QQuickItem *parent, const QString &objectName)
{
    QQuickItem *result = nullptr;
    std::function<void(QQuickItem *)> visit = [&result, &objectName,
                                               &visit](QQuickItem *item) {
        if (result || !item)
            return;
        if (item->objectName() == objectName) {
            result = item;
            return;
        }
        for (QQuickItem *child : item->childItems())
            visit(child);
    };
    visit(parent);
    return result;
}

QPoint itemCenter(QQuickWindow &window, QQuickItem *item)
{
    return item->mapToItem(window.contentItem(), item->width() / 2.0,
                           item->height() / 2.0)
        .toPoint();
}

QPointF visualCenter(QQuickWindow &window, QQuickItem *item)
{
    return item->mapToItem(window.contentItem(), item->width() / 2.0,
                           item->height() / 2.0);
}

double propertyReal(QQuickItem *item, const char *name)
{
    return item->property(name).toDouble();
}

QRect backdropBounds(const QVector<QRect> &rectangles)
{
    QRect result;
    for (const QRect &rectangle : rectangles)
        result = result.isNull() ? rectangle : result.united(rectangle);
    return result;
}

bool backdropContains(const QVector<QRect> &rectangles, const QPoint &point)
{
    for (const QRect &rectangle : rectangles) {
        if (rectangle.contains(point))
            return true;
    }
    return false;
}

QVector<QRect> integerRectangles(const QVariantList &values)
{
    QVector<QRect> result;
    result.reserve(values.size());
    for (const QVariant &value : values) {
        const QRectF rectangle = value.toRectF();
        result.append(QRect(qRound(rectangle.x()), qRound(rectangle.y()),
                            qRound(rectangle.width()), qRound(rectangle.height())));
    }
    return result;
}

bool pixelCenterInsideCapsule(const QRectF &body, const QPointF &pixelCenter)
{
    const qreal radius = body.height() / 2.0;
    const qreal centerY = body.y() + radius;
    const qreal nearestX = qBound(body.x() + radius, pixelCenter.x(),
                                  body.x() + body.width() - radius);
    const qreal dx = pixelCenter.x() - nearestX;
    const qreal dy = pixelCenter.y() - centerY;
    return dx * dx + dy * dy <= radius * radius + 1e-9;
}

bool pixelCenterInsideTriangle(const QPointF &a, const QPointF &b, const QPointF &c,
                              const QPointF &pixelCenter)
{
    const auto cross = [](const QPointF &left, const QPointF &right,
                          const QPointF &point) {
        return (right.x() - left.x()) * (point.y() - left.y())
            - (right.y() - left.y()) * (point.x() - left.x());
    };
    const qreal ab = cross(a, b, pixelCenter);
    const qreal bc = cross(b, c, pixelCenter);
    const qreal ca = cross(c, a, pixelCenter);
    return (ab >= -1e-9 && bc >= -1e-9 && ca >= -1e-9)
        || (ab <= 1e-9 && bc <= 1e-9 && ca <= 1e-9);
}

QVector<QRect> referenceCapsulePixelCenterRows(const QRectF &body)
{
    QVector<QRect> result;
    const int firstRow = qFloor(body.y());
    const int endRow = qCeil(body.y() + body.height());
    const int firstColumn = qFloor(body.x()) - 1;
    const int endColumn = qCeil(body.x() + body.width());
    for (int y = firstRow; y < endRow; ++y) {
        int runStart = -1;
        for (int x = firstColumn; x <= endColumn; ++x) {
            const bool covered = x < endColumn
                && pixelCenterInsideCapsule(body, QPointF(x + 0.5, y + 0.5));
            if (covered && runStart < 0)
                runStart = x;
            else if (!covered && runStart >= 0) {
                result.append(QRect(runStart, y, x - runStart, 1));
                runStart = -1;
            }
        }
    }
    return result;
}

QRect wholeBandContainedCapsuleRow(const QRectF &body, const int row)
{
    // Diagnostic reference for the old rule: both horizontal edges of the
    // complete one-pixel row band had to stay inside the analytic capsule.
    const qreal radius = body.height() / 2.0;
    const qreal centerY = body.y() + radius;
    const int y = qRound(body.y()) + row;
    const qreal farthestDistance = qMax(qAbs(y - centerY),
                                        qAbs(y + 1.0 - centerY));
    const qreal reach = qSqrt(qMax<qreal>(0.0, radius * radius
                                                - farthestDistance * farthestDistance));
    const int left = qCeil(body.x() + radius - reach);
    const int right = qFloor(body.x() + body.width() - radius + reach);
    return QRect(left, y, qMax(0, right - left), 1);
}

QVector<QRect> referenceTailPixelCenterSlices(const QPointF &a, const QPointF &b,
                                               const QPointF &tip, const QRectF &body,
                                               const QString &edge)
{
    QVector<QRect> result;
    const bool horizontal = qAbs(a.y() - b.y()) < 1e-9;
    const QPointF points[] = {a, b, tip};
    const auto primary = [horizontal](const QPointF &point) {
        return horizontal ? point.y() : point.x();
    };
    const auto secondary = [horizontal](const QPointF &point) {
        return horizontal ? point.x() : point.y();
    };
    const qreal firstValue = qMin(primary(a), qMin(primary(b), primary(tip)));
    const qreal endValue = qMax(primary(a), qMax(primary(b), primary(tip)));
    int first = qFloor(firstValue);
    int end = qCeil(endValue);
    if (edge == QStringLiteral("left"))
        end = qMin(end, qFloor(body.left()) + 1);
    else if (edge == QStringLiteral("right"))
        first = qMax(first, qCeil(body.right()) - 1);
    const int edges[3][2] = {{0, 1}, {1, 2}, {2, 0}};
    for (int slice = first; slice < end; ++slice) {
        const qreal sample = slice + 0.5;
        QVector<qreal> intersections;
        for (const auto &edge : edges) {
            const QPointF &p = points[edge[0]];
            const QPointF &q = points[edge[1]];
            const qreal pPrimary = primary(p);
            const qreal qPrimary = primary(q);
            if ((pPrimary <= sample && sample < qPrimary)
                || (qPrimary <= sample && sample < pPrimary)) {
                const qreal fraction = (sample - pPrimary) / (qPrimary - pPrimary);
                intersections.append(secondary(p)
                                     + fraction * (secondary(q) - secondary(p)));
            }
        }
        if (intersections.size() < 2)
            continue;
        const qreal low = qMin(intersections.at(0), intersections.at(1));
        const qreal high = qMax(intersections.at(0), intersections.at(1));
        const int firstPixel = qCeil(low - 0.5);
        const int lastPixel = qFloor(high - 0.5);
        if (lastPixel >= firstPixel) {
            result.append(horizontal
                ? QRect(firstPixel, slice, lastPixel - firstPixel + 1, 1)
                : QRect(slice, firstPixel, 1, lastPixel - firstPixel + 1));
        }
    }
    return result;
}

DockConfig configFor(const QString &hoverEffect, const QStringList &pins)
{
    DockConfig config = DockConfig::defaults();
    config.hoverEffect = hoverEffect;
    config.pins = pins;
    return config;
}

} // namespace

class DockHoverQmlTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void iconSourceQualityRemainsStableDuringHover();
    void windowDprMatchesScreenDprForDockIcon();
    void surfaceEnvelopeRemainsStableDuringMagnification();
    void backdropRegionTracksMagnifiedChromeInsideStableSurface();
    void dockMaterialFollowsShellThemeState();
    void pointerCenterRemainsStableForPracticalMagnificationTargets();
    void inputMaskTracksCenteredChromeAndMagnifiedIcon();
    void visualHeadroomKeepsIconsInBoundsForEveryIconSize();
    void magnificationChromeFitsSurfaceEnvelopeForRepresentativeConfigurations();
    void hoverModesAndTransitions();
    void reorderPreviewWorksForEveryHoverMode();
    void pointerHandlersActivateAndReorderExactlyOnce();
    void pointerHandlerCancellationRestoresWithoutReorder();
    void pointerHandlerGrabTransitionsFinalizeExactlyOnce();
    void dragGeometryRemainsStableDuringMagnificationCollapse();
    void modelMoveRefreshesHoverGeometryAndIdentity();
    void releasePointerRestoresMagnificationTarget();
    void reorderLiftsOnlyTheDraggedDelegate();
    void runtimeOnlyQmlUsesTaskKeyForIdentityAndContextTarget();
    void magnifiedVisualRegionAcceptsContextMenu();
    void mainSurfaceUsesExplicitOutputGeometryWithoutWindowScreen();
    void tooltipFollowsMagnifiedIconAndHidesForDockInteractions();
    void runningTooltipUsesOnlyTheApplicationDisplayName();
    void visibleTooltipReanchorsAfterOutputGeometryChanges();
    void queuedVisibleTooltipRefreshCannotPresentReplacementTarget();
    void modelChangedTooltipRefreshCannotPresentReplacementTarget();
    void pendingTooltipFollowsReplacementTarget();
    void pendingTooltipDoesNotPresentRemovedTarget();
    void magnifiedInteractionTargetMatchesVisualBounds();
    void magnifiedInteractionRegionsResolveByVisualStacking();
    void exclusiveDragReleaseInEmptyHeadroomStaysOutsideDock();
    void verticalPositionsReusePrimaryAxisGeometry();
    void verticalContentIsCenteredWithinChrome();
    void verticalDragReorderUsesPrimaryAndCrossAxes();
    void floatingAutoHideUsesPhysicalEdgeRevealGeometry();
};

void DockHoverQmlTest::initTestCase()
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
}

void DockHoverQmlTest::iconSourceQualityRemainsStableDuringHover()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString themeRoot = temporary.path() + QStringLiteral("/dock-quality-theme");
    const QString iconDirectory = themeRoot + QStringLiteral("/48x48/apps");
    QVERIFY(QDir().mkpath(iconDirectory));
    QFile index(themeRoot + QStringLiteral("/index.theme"));
    QVERIFY(index.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray indexContent =
        "[Icon Theme]\nName=Dock Quality\nDirectories=48x48/apps\n\n"
        "[48x48/apps]\nSize=48\nType=Fixed\nContext=Applications\n";
    QCOMPARE(index.write(indexContent), indexContent.size());
    QImage fixture(QSize(48, 48), QImage::Format_ARGB32);
    fixture.fill(QColor("#00aa55"));
    QVERIFY(fixture.save(iconDirectory + QStringLiteral("/quality-test-icon.png")));

    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop")};
    DockController controller;
    controller.applyConfig(configFor(QStringLiteral("magnification"), pins));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.addImageProvider(QStringLiteral("astrea-icon"), new AstreaIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(600, 180);
    QTest::qWait(40);

    QQuickItem *source = delegate(panel, QStringLiteral("two.desktop"));
    QQuickItem *icon = iconItem(source);
    QVERIFY(source && icon);
    icon->setProperty("iconPath",
                      QUrl::fromLocalFile(iconDirectory
                                          + QStringLiteral("/quality-test-icon.png"))
                          .toString());
    icon->setProperty("devicePixelRatioOverride", 2.0);
    QTRY_VERIFY_WITH_TIMEOUT(icon->property("ready").toBool(), 1500);
    const QString initialSource = icon->property("resolvedSource").toString();
    const int initialSourcePixels = icon->property("effectiveSourcePixelSize").toInt();
    const int logicalSourceSize = icon->property("effectiveMaximumLogicalSize").toInt();
    QCOMPARE(initialSource,
             QUrl::fromLocalFile(iconDirectory + QStringLiteral("/quality-test-icon.png"))
                 .toString());
    QVERIFY(!initialSource.contains(QStringLiteral("magnification")));
    QCOMPARE(initialSourcePixels, logicalSourceSize * 2);

    const auto verifyStableSource = [&] {
        QCOMPARE(icon->property("resolvedSource").toString(), initialSource);
        QCOMPARE(icon->property("effectiveSourcePixelSize").toInt(), initialSourcePixels);
    };
    const auto movePointerTo = [&](QQuickItem *target) {
        const QPointF point = target->mapToItem(
            panel, target->width() / 2.0, target->height() / 2.0);
        QVERIFY(QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                          Q_ARG(QVariant, QVariant(point.x())),
                                          Q_ARG(QVariant, QVariant(point.y()))));
    };
    const auto movePointerToPoint = [&](const QPointF &point) {
        QVERIFY(QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                          Q_ARG(QVariant, QVariant(point.x())),
                                          Q_ARG(QVariant, QVariant(point.y()))));
    };

    QVERIFY(QMetaObject::invokeMethod(panel, "setPointerInside",
                                      Q_ARG(QVariant, QVariant(false))));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(icon->scale() - 1.0) < 0.01, 1500);
    verifyStableSource();

    QQuickItem *neighbor = delegate(panel, QStringLiteral("one.desktop"));
    QQuickItem *next = delegate(panel, QStringLiteral("three.desktop"));
    QVERIFY(neighbor && next);
    const QPointF neighborCenter = neighbor->mapToItem(
        panel, neighbor->width() / 2.0, neighbor->height() / 2.0);
    const QPointF sourceCenter = source->mapToItem(
        panel, source->width() / 2.0, source->height() / 2.0);
    const QPointF nextCenter = next->mapToItem(
        panel, next->width() / 2.0, next->height() / 2.0);
    movePointerToPoint(QPointF((neighborCenter.x() + sourceCenter.x()) / 2.0,
                               sourceCenter.y()));
    QTRY_VERIFY_WITH_TIMEOUT(icon->scale() > 1.0, 1500);
    const qreal neighborScale = icon->scale();
    verifyStableSource();

    movePointerTo(source);
    QTRY_VERIFY_WITH_TIMEOUT(icon->scale() > neighborScale, 1500);
    verifyStableSource();

    movePointerToPoint(QPointF((sourceCenter.x() + nextCenter.x()) / 2.0,
                               sourceCenter.y()));
    QTRY_VERIFY_WITH_TIMEOUT(icon->scale() > 1.0, 1500);
    verifyStableSource();

    QVERIFY(QMetaObject::invokeMethod(panel, "setPointerInside",
                                      Q_ARG(QVariant, QVariant(false))));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(icon->scale() - 1.0) < 0.01, 1500);
    verifyStableSource();

    delete panel;
}

void DockHoverQmlTest::windowDprMatchesScreenDprForDockIcon()
{
    DockController controller;
    controller.applyConfig(configFor(QStringLiteral("magnification"), {
        QStringLiteral("one.desktop")
    }));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.addImageProvider(QStringLiteral("astrea-icon"), new AstreaIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(240, 180);
    window.show();
    QTest::qWait(40);

    QQuickItem *icon = iconItem(delegate(panel, QStringLiteral("one.desktop")));
    QVERIFY(icon);
    const qreal screenDpr = icon->property("effectiveDevicePixelRatio").toReal();
    const qreal windowDpr = window.devicePixelRatio();
    const qreal effectiveWindowDpr = window.effectiveDevicePixelRatio();
    qInfo() << "Dock icon DPRs: Screen" << screenDpr
            << "window" << windowDpr
            << "effective window" << effectiveWindowDpr;
    QVERIFY(screenDpr > 0.0);
    QVERIFY(effectiveWindowDpr > 0.0);
    QVERIFY(qAbs(screenDpr - effectiveWindowDpr) < 0.001);
    delete panel;
}

void DockHoverQmlTest::surfaceEnvelopeRemainsStableDuringMagnification()
{
    DockController controller;
    controller.applyConfig(configFor(QStringLiteral("magnification"), {
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop"), QStringLiteral("four.desktop"),
        QStringLiteral("five.desktop"), QStringLiteral("six.desktop"),
        QStringLiteral("seven.desktop")
    }));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(900, 220);
    panel->setX((window.width() - panel->width()) / 2.0);
    window.show();
    QTest::qWait(20);

    QQuickItem *chrome = childWithObjectName(panel, QStringLiteral("dockChrome"));
    QQuickItem *center = delegate(panel, QStringLiteral("four.desktop"));
    QVERIFY(chrome && center);

    const qreal restingWidth = panel->property("restingWidth").toReal();
    const qreal initialSurfaceWidth = panel->width();
    const qreal initialSurfaceHeight = panel->height();
    QVERIFY2(initialSurfaceWidth > restingWidth,
             "the Layer Shell envelope must reserve horizontal magnification headroom");
    QVERIFY2(initialSurfaceHeight > panel->property("restingHeight").toReal(),
             "the Layer Shell envelope must reserve vertical magnification headroom");
    QCOMPARE(chrome->width(), restingWidth);
    QCOMPARE(chrome->height(), panel->property("restingHeight").toReal());
    QVERIFY(chrome->width() <= initialSurfaceWidth);
    QVERIFY(chrome->height() == panel->property("restingHeight").toReal());

    const qreal centerX = center->mapToItem(
        panel, center->width() / 2.0, center->height() / 2.0).x();
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(centerX))));
    QTRY_VERIFY_WITH_TIMEOUT(center->property("magnificationScale").toReal() > 1.0, 1500);
    QTRY_VERIFY_WITH_TIMEOUT(panel->property("magnificationExtraPrimary").toReal() > 0.0,
                             1500);
    QTRY_VERIFY_WITH_TIMEOUT(chrome->width() > restingWidth, 1500);
    QTRY_VERIFY_WITH_TIMEOUT(
        qAbs(chrome->width() - restingWidth
             - panel->property("magnificationExtraPrimary").toReal()) < 0.1,
        1500);
    QCOMPARE(chrome->height(), panel->property("restingHeight").toReal());
    QVERIFY2(qAbs(panel->width() - initialSurfaceWidth) < 0.1,
             "hover must not resize the Layer Shell surface width");
    QVERIFY2(qAbs(panel->height() - initialSurfaceHeight) < 0.1,
             "hover must not resize the Layer Shell surface height");
    QVERIFY(chrome->height() <= panel->height());
    QVERIFY(chrome->width() <= panel->width());

    QVERIFY(QMetaObject::invokeMethod(panel, "setPointerInside",
                                      Q_ARG(QVariant, QVariant(false))));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->property("magnificationExtraPrimary").toReal()) < 0.1,
                             1500);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(chrome->width() - restingWidth) < 0.1, 1500);
    QTRY_VERIFY_WITH_TIMEOUT(
        qAbs(chrome->height() - panel->property("restingHeight").toReal()) < 0.1,
        1500);
    QVERIFY2(qAbs(panel->width() - initialSurfaceWidth) < 0.1,
             "hover exit must not resize the Layer Shell surface width");
    QVERIFY2(qAbs(panel->height() - initialSurfaceHeight) < 0.1,
             "hover exit must not resize the Layer Shell surface height");

    delete panel;
}

void DockHoverQmlTest::backdropRegionTracksMagnifiedChromeInsideStableSurface()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ThemeController theme(temporary.filePath(QStringLiteral("missing-theme.json")));
    theme.setShellStyle(2);

    DockController controller;
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop"), QStringLiteral("four.desktop"),
        QStringLiteral("five.desktop")};
    controller.applyConfig(configFor(QStringLiteral("magnification"), pins));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &theme);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(900, 220);
    panel->setX((window.width() - panel->width()) / 2.0);
    window.show();
    QTest::qWait(60);

    const auto regions = panel->findChildren<AstreaBackdropEffectRegions *>();
    QCOMPARE(regions.size(), 1);
    auto *descriptors = regions.constFirst();
    const auto before = descriptors->resolvedRegion();
    QVERIFY(!before.isEmpty());
    const QRect beforeBounds = backdropBounds(before);
    const QRectF surfaceRect = panel->mapRectToItem(
        window.contentItem(), QRectF(0, 0, panel->width(), panel->height()));
    const qreal initialSurfaceWidth = panel->width();
    const qreal initialSurfaceHeight = panel->height();

    QQuickItem *center = delegate(panel, QStringLiteral("three.desktop"));
    QVERIFY(center != nullptr);
    const qreal centerX = center->mapToItem(
        panel, center->width() / 2.0, center->height() / 2.0).x();
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(centerX))));
    QTRY_VERIFY_WITH_TIMEOUT(center->property("magnificationScale").toReal() > 1.0, 1500);
    QTRY_VERIFY_WITH_TIMEOUT(panel->property("magnificationExtraPrimary").toReal() > 0.0,
                             1500);
    QTRY_VERIFY_WITH_TIMEOUT(childWithObjectName(panel, QStringLiteral("dockChrome"))->width()
                                 > panel->property("restingWidth").toReal(),
                             1500);
    QTRY_VERIFY_WITH_TIMEOUT(descriptors->resolvedRegion() != before, 1500);

    auto *chrome = childWithObjectName(panel, QStringLiteral("dockChrome"));
    QVERIFY(chrome);
    QTRY_VERIFY_WITH_TIMEOUT(
        qAbs(chrome->width() - panel->property("restingWidth").toReal()
             - panel->property("magnificationExtraPrimary").toReal()) < 0.1,
        1500);
    const auto expanded = descriptors->resolvedRegion();
    const QRect expandedBounds = backdropBounds(expanded);
    const QRectF expandedChromeRect = chrome->mapRectToItem(
        window.contentItem(), QRectF(0, 0, chrome->width(), chrome->height()));
    QVERIFY(expandedBounds.width() > beforeBounds.width());
    QVERIFY2(surfaceRect.contains(expandedChromeRect),
             qPrintable(QStringLiteral("surface=%1,%2 %3x%4 chrome=%5,%6 %7x%8")
                            .arg(surfaceRect.x()).arg(surfaceRect.y()).arg(surfaceRect.width())
                            .arg(surfaceRect.height()).arg(expandedChromeRect.x())
                            .arg(expandedChromeRect.y()).arg(expandedChromeRect.width())
                            .arg(expandedChromeRect.height())));
    QVERIFY(surfaceRect.toAlignedRect().contains(expandedBounds));
    QVERIFY(qAbs(expandedBounds.center().x()
                 - qRound(expandedChromeRect.center().x())) <= 1);
    QVERIFY(qAbs(expandedBounds.center().y()
                 - qRound(expandedChromeRect.center().y())) <= 1);
    QVERIFY(!backdropContains(expanded,
                              QPoint(qRound(expandedChromeRect.left()),
                                     qRound(expandedChromeRect.top()))));
    QVERIFY(qAbs(panel->width() - initialSurfaceWidth) < 0.1);
    QVERIFY(qAbs(panel->height() - initialSurfaceHeight) < 0.1);

    QVERIFY(QMetaObject::invokeMethod(panel, "setPointerInside",
                                      Q_ARG(QVariant, QVariant(false))));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->property("magnificationExtraPrimary").toReal()) < 0.1,
                             1500);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(chrome->width()
                                  - panel->property("restingWidth").toReal()) < 0.1,
                             1500);
    QTRY_VERIFY_WITH_TIMEOUT(descriptors->resolvedRegion() == before, 1500);
    QVERIFY(qAbs(panel->width() - initialSurfaceWidth) < 0.1);
    QVERIFY(qAbs(panel->height() - initialSurfaceHeight) < 0.1);
    delete panel;
}

void DockHoverQmlTest::dockMaterialFollowsShellThemeState()
{
    struct ExpectedMaterial {
        int themeMode;
        int shellStyle;
        QColor background;
        QColor border;
    };
    const auto rgba = [](qreal red, qreal green, qreal blue, qreal alpha) {
        return QColor::fromRgbF(red, green, blue, alpha);
    };
    const QList<ExpectedMaterial> expected{
        {0, 1, rgba(0.10, 0.10, 0.11, 0.96), rgba(1, 1, 1, 0.11)},
        {0, 2, rgba(0, 0, 0, 0.06), rgba(1, 1, 1, 0.14)},
        {1, 1, rgba(0.985, 0.987, 0.994, 0.92), rgba(0, 0, 0, 0.12)},
        {1, 2, rgba(0.96, 0.985, 1, 0.30), rgba(0, 0, 0, 0.10)},
    };

    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ThemeController theme(temporary.filePath(QStringLiteral("missing-theme.json")));
    DockController controller;
    controller.applyConfig(configFor(QStringLiteral("none"), {QStringLiteral("one.desktop")}));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &theme);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));

    for (const ExpectedMaterial &values : expected) {
        theme.setThemeMode(values.themeMode);
        theme.setShellStyle(values.shellStyle);
        QCoreApplication::processEvents();
        auto *panel = qobject_cast<QQuickItem *>(component.create());
        QVERIFY2(panel, qPrintable(component.errorString()));
        auto *chrome = childWithObjectName(panel, QStringLiteral("dockChrome"));
        QVERIFY(chrome != nullptr);
        QCOMPARE(chrome->property("color").value<QColor>(), values.background);
        QCOMPARE(QQmlProperty(chrome, QStringLiteral("border.color")).read().value<QColor>(),
                 values.border);
        if (values.themeMode == 0 && values.shellStyle == 2)
            QVERIFY(chrome->property("color").value<QColor>() != QColor(QStringLiteral("#80343434")));
        delete panel;
    }
}

void DockHoverQmlTest::pointerCenterRemainsStableForPracticalMagnificationTargets()
{
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop"), QStringLiteral("four.desktop"),
        QStringLiteral("five.desktop"), QStringLiteral("six.desktop"),
        QStringLiteral("seven.desktop")};
    for (const int targetIndex : {0, 3, 6}) {
        DockController controller;
        controller.applyConfig(configFor(QStringLiteral("magnification"), pins));

        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
        engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
        QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
        QVERIFY2(component.status() == QQmlComponent::Ready,
                 qPrintable(component.errorString()));
        auto *panel = qobject_cast<QQuickItem *>(component.create());
        QVERIFY2(panel, qPrintable(component.errorString()));
        QQuickWindow window;
        panel->setParentItem(window.contentItem());
        window.resize(900, 220);
        panel->setX((window.width() - panel->width()) / 2.0);
        window.show();
        QTest::qWait(20);

        QQuickItem *target = delegate(panel, pins.at(targetIndex));
        QVERIFY(target);
        const QPointF pointer = target->mapToItem(
            panel, target->width() / 2.0, target->height() / 2.0);
        const qreal expectedCenterRelative = pointer.x() - panel->width() / 2.0;
        QVERIFY(QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                          Q_ARG(QVariant, QVariant(pointer.x())),
                                          Q_ARG(QVariant, QVariant(pointer.y()))));
        QTRY_VERIFY_WITH_TIMEOUT(target->property("magnificationScale").toReal() > 1.0,
                                 1500);
        QTRY_COMPARE_WITH_TIMEOUT(panel->property("pointerTargetDesktopFileName").toString(),
                                  pins.at(targetIndex), 1500);

        const qreal pointerCenterRelative = panel->property("pointerX").toReal();
        QVERIFY2(qAbs(pointerCenterRelative - expectedCenterRelative) < 0.1,
                 qPrintable(QStringLiteral("target=%1 pointer=%2 expected=%3")
                                .arg(pins.at(targetIndex))
                                .arg(pointerCenterRelative)
                                .arg(expectedCenterRelative)));
        QVERIFY2(target->property("magnificationScale").toReal() >= 1.0,
                 "the same resting target must remain a valid magnification target");

        delete panel;
    }
}

void DockHoverQmlTest::inputMaskTracksCenteredChromeAndMagnifiedIcon()
{
    DockController controller;
    controller.applyConfig(configFor(QStringLiteral("magnification"), {
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop"), QStringLiteral("four.desktop"),
        QStringLiteral("five.desktop"), QStringLiteral("six.desktop"),
        QStringLiteral("seven.desktop")
    }));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(900, 220);
    panel->setX((window.width() - panel->width()) / 2.0);
    DockInputRegionBridge bridge;
    panel->setProperty("inputRegionBridge",
                       QVariant::fromValue(static_cast<QObject *>(&bridge)));
    bridge.setWindow(&window);
    window.show();
    QTest::qWait(50);

    QQuickItem *chrome = childWithObjectName(panel, QStringLiteral("dockChrome"));
    QQuickItem *center = delegate(panel, QStringLiteral("four.desktop"));
    QQuickItem *icon = iconItem(center);
    QVERIFY(chrome && center && icon);
    QMetaObject::invokeMethod(panel, "updateInputRegion");
    QCoreApplication::processEvents();

    const QRectF surfaceRect = panel->mapRectToItem(
        window.contentItem(), QRectF(0, 0, panel->width(), panel->height()));
    const QRectF chromeRect = chrome->mapRectToItem(
        window.contentItem(), QRectF(0, 0, chrome->width(), chrome->height()));
    QVERIFY(surfaceRect.width() > chromeRect.width());
    QVERIFY(window.mask().contains(chromeRect.center().toPoint()));
    QVERIFY(!window.mask().contains(QPoint(qRound(surfaceRect.left() + 1.0),
                                           qRound(surfaceRect.center().y()))));

    const qreal centerX = center->mapToItem(
        panel, center->width() / 2.0, center->height() / 2.0).x();
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(centerX))));
    QTRY_VERIFY_WITH_TIMEOUT(icon->scale() > 1.0, 1500);
    QTRY_VERIFY_WITH_TIMEOUT(panel->property("magnificationExtraPrimary").toReal() > 0.0,
                             1500);
    QTRY_VERIFY_WITH_TIMEOUT(chrome->width() > panel->property("restingWidth").toReal(),
                             1500);
    QTRY_VERIFY_WITH_TIMEOUT(window.mask().contains(
                                 icon->mapRectToItem(window.contentItem(),
                                                     QRectF(0, 0, icon->width(), icon->height()))
                                     .topLeft().toPoint()),
                             1500);

    const QRectF magnifiedIconRect = icon->mapRectToItem(
        window.contentItem(), QRectF(0, 0, icon->width(), icon->height()));
    const QRectF expandedChromeRect = chrome->mapRectToItem(
        window.contentItem(), QRectF(0, 0, chrome->width(), chrome->height()));
    QVERIFY(expandedChromeRect.width() > chromeRect.width());
    QVERIFY(qAbs(expandedChromeRect.center().x() - surfaceRect.center().x()) < 0.1);
    QVERIFY(window.mask().contains(expandedChromeRect.center().toPoint()));
    const QPoint expandedLeftEdge(qRound(expandedChromeRect.left() + 1.0),
                                  qRound(expandedChromeRect.center().y()));
    const QPoint expandedRightEdge(qRound(expandedChromeRect.right() - 1.0),
                                   qRound(expandedChromeRect.center().y()));
    QVERIFY(window.mask().contains(expandedLeftEdge));
    QVERIFY(window.mask().contains(expandedRightEdge));
    QVERIFY(magnifiedIconRect.top() < expandedChromeRect.top());
    QVERIFY(!window.mask().contains(QPoint(qRound(surfaceRect.left() + 1.0),
                                           qRound(surfaceRect.top() + 1.0))));

    delete panel;
}

void DockHoverQmlTest::visualHeadroomKeepsIconsInBoundsForEveryIconSize()
{
    for (int iconSize = 32; iconSize <= 64; ++iconSize) {
        DockController controller;
        const QStringList pins{QStringLiteral("one.desktop")};
        DockConfig config = configFor(QStringLiteral("lift"), pins);
        config.iconSize = iconSize;
        controller.applyConfig(config);

        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
        engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
        QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
        QVERIFY2(component.status() == QQmlComponent::Ready,
                 qPrintable(component.errorString()));
        auto *panel = qobject_cast<QQuickItem *>(component.create());
        QVERIFY2(panel, qPrintable(component.errorString()));
        QQuickWindow window;
        panel->setParentItem(window.contentItem());
        QCoreApplication::processEvents();

        QQuickItem *delegateItem = delegate(panel, QStringLiteral("one.desktop"));
        QQuickItem *icon = iconItem(delegateItem);
        QVERIFY(delegateItem && icon);
        const double expectedRestingTop = delegateItem->mapToItem(panel, 0, 0).y()
            + (delegateItem->height() - iconSize) / 2.0;
        const double actualRestingTop = icon->mapToItem(panel, 0, 0).y();
        QVERIFY2(qAbs(actualRestingTop - expectedRestingTop) < 0.5,
                 qPrintable(QStringLiteral("iconSize=%1 resting baseline=%2 expected=%3")
                                .arg(iconSize).arg(actualRestingTop).arg(expectedRestingTop)));
        QQuickItem *chrome = delegate(panel, QStringLiteral("dockChrome"));
        QVERIFY(chrome);
        QCOMPARE(chrome->height(), controller.restingHeight());

        QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                          Q_ARG(QVariant, QVariant(panel->width() / 2.0))));
        QTRY_VERIFY_WITH_TIMEOUT(panel->property("height").toDouble()
                                     >= controller.restingHeight(),
                                 1000);
        QTRY_VERIFY_WITH_TIMEOUT(icon->mapToItem(panel, 0, 0).y() >= -0.01, 1000);
        QCOMPARE(chrome->height(), controller.restingHeight());

        config.hoverEffect = QStringLiteral("magnification");
        controller.applyConfig(config);
        QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                          Q_ARG(QVariant, QVariant(panel->width() / 2.0))));
        QTRY_VERIFY_WITH_TIMEOUT(icon->mapToItem(panel, 0, 0).y() >= -0.01, 1000);
        QCOMPARE(chrome->height(), controller.restingHeight());

        config.hoverEffect = QStringLiteral("none");
        controller.applyConfig(config);
        QVERIFY(QMetaObject::invokeMethod(panel, "beginReorder",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop")))));
        QVERIFY(QMetaObject::invokeMethod(panel, "updateReorder",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop"))),
                                          Q_ARG(QVariant, QVariant(0.0)),
                                          Q_ARG(QVariant, QVariant(0.0)),
                                          Q_ARG(QVariant, QVariant(0.0))));
        QTRY_VERIFY_WITH_TIMEOUT(icon->mapToItem(panel, 0, 0).y() >= -0.01, 1000);
        QCOMPARE(chrome->height(), controller.restingHeight());
        QVERIFY(QMetaObject::invokeMethod(panel, "cancelReorder",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop")))));
        delete panel;
    }
}

void DockHoverQmlTest::magnificationChromeFitsSurfaceEnvelopeForRepresentativeConfigurations()
{
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop"), QStringLiteral("four.desktop"),
        QStringLiteral("five.desktop")};
    for (const QString &position : {QStringLiteral("bottom"), QStringLiteral("left"),
                                    QStringLiteral("right")}) {
        const bool vertical = position != QStringLiteral("bottom");
        for (const int iconSize : {32, 64}) {
            for (const double magnificationScale : {1.25, 2.0}) {
                DockController controller;
                DockConfig config = configFor(QStringLiteral("magnification"), pins);
                config.position = position;
                config.iconSize = iconSize;
                config.magnificationRadius = iconSize == 32 ? 2 : 4;
                config.magnificationScale = magnificationScale;
                controller.applyConfig(config);

                QQmlEngine engine;
                engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
                engine.rootContext()->setContextProperty(QStringLiteral("DockController"),
                                                         &controller);
                QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
                QVERIFY2(component.status() == QQmlComponent::Ready,
                         qPrintable(component.errorString()));
                auto *panel = qobject_cast<QQuickItem *>(component.create());
                QVERIFY2(panel, qPrintable(component.errorString()));
                QQuickWindow window;
                panel->setParentItem(window.contentItem());
                window.resize(vertical ? 180 : 900, vertical ? 900 : 220);
                if (vertical) {
                    panel->setX(20);
                    panel->setY((window.height() - panel->height()) / 2.0);
                } else {
                    panel->setX((window.width() - panel->width()) / 2.0);
                }
                window.show();
                QTest::qWait(30);

                auto *chrome = childWithObjectName(panel, QStringLiteral("dockChrome"));
                auto *center = delegate(panel, QStringLiteral("three.desktop"));
                auto *centerIcon = iconItem(center);
                QVERIFY(chrome && center && centerIcon);
                const qreal initialSurfaceWidth = panel->width();
                const qreal initialSurfaceHeight = panel->height();
                const qreal restingWidth = panel->property("restingWidth").toReal();
                const qreal restingHeight = panel->property("restingHeight").toReal();
                const QPointF centerPoint = center->mapToItem(
                    panel, center->width() / 2.0, center->height() / 2.0);
                if (vertical) {
                    QVERIFY(QMetaObject::invokeMethod(
                        panel, "updatePointerAtPoint",
                        Q_ARG(QVariant, QVariant(centerPoint.x())),
                        Q_ARG(QVariant, QVariant(centerPoint.y()))));
                } else {
                    QVERIFY(QMetaObject::invokeMethod(
                        panel, "updatePointerAtPoint",
                        Q_ARG(QVariant, QVariant(centerPoint.x())),
                        Q_ARG(QVariant, QVariant(centerPoint.y()))));
                }

                QTRY_VERIFY_WITH_TIMEOUT(panel->property("magnificationExtraPrimary").toReal()
                                             > 0.0,
                                         1500);
                const qreal expansion = panel->property("magnificationExtraPrimary").toReal();
                QTRY_VERIFY2_WITH_TIMEOUT(
                    vertical ? chrome->height() > restingHeight
                             : chrome->width() > restingWidth,
                    qPrintable(QStringLiteral("position=%1 iconSize=%2 scale=%3 extra=%4 chrome=%5x%6 resting=%7x%8")
                                   .arg(position).arg(iconSize).arg(magnificationScale)
                                   .arg(expansion).arg(chrome->width()).arg(chrome->height())
                                   .arg(restingWidth).arg(restingHeight)),
                    1500);
                if (vertical) {
                    QCOMPARE(chrome->width(), restingWidth);
                    QVERIFY(chrome->height() > restingHeight);
                } else {
                    QVERIFY(chrome->width() > restingWidth);
                    QCOMPARE(chrome->height(), restingHeight);
                }
                QVERIFY2(expansion <= panel->property("maximumMagnificationExtraPrimary")
                                         .toReal() + 0.1,
                         qPrintable(QStringLiteral("position=%1 iconSize=%2 scale=%3 extra=%4 max=%5")
                                        .arg(position).arg(iconSize).arg(magnificationScale)
                                        .arg(expansion)
                                        .arg(panel->property("maximumMagnificationExtraPrimary")
                                                 .toReal())));
                QVERIFY(qAbs(panel->width() - initialSurfaceWidth) < 0.1);
                QVERIFY(qAbs(panel->height() - initialSurfaceHeight) < 0.1);

                const QRectF surfaceRect = panel->mapRectToItem(
                    window.contentItem(), QRectF(0, 0, panel->width(), panel->height()));
                const QRectF chromeRect = chrome->mapRectToItem(
                    window.contentItem(), QRectF(0, 0, chrome->width(), chrome->height()));
                QVERIFY(chromeRect.left() >= surfaceRect.left() - 0.1);
                QVERIFY(chromeRect.top() >= surfaceRect.top() - 0.1);
                QVERIFY(chromeRect.right() <= surfaceRect.right() + 0.1);
                QVERIFY(chromeRect.bottom() <= surfaceRect.bottom() + 0.1);
                delete panel;
            }
        }
    }
}

void DockHoverQmlTest::pointerHandlersActivateAndReorderExactlyOnce()
{
    DockController controller;
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop")};
    controller.applyConfig(configFor(QStringLiteral("none"), pins));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(400, 120);
    window.show();
    QTest::qWait(20);

    QQuickItem *first = delegate(panel, QStringLiteral("one.desktop"));
    QVERIFY(first);
    QSignalSpy activationSpy(first, SIGNAL(activated(QString)));
    QSignalSpy finishSpy(first, SIGNAL(dragFinished(QString)));
    QSignalSpy cancelSpy(first, SIGNAL(dragCanceled(QString)));
    QSignalSpy reorderSpy(panel, SIGNAL(reorderRequested(QString,int)));
    QVERIFY(activationSpy.isValid() && finishSpy.isValid() && cancelSpy.isValid()
            && reorderSpy.isValid());

    const QPoint start = itemCenter(window, first);
    QTest::mouseMove(&window, start);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, start);
    QTRY_COMPARE_WITH_TIMEOUT(activationSpy.count(), 1, 1000);
    QCOMPARE(finishSpy.count(), 0);
    QCOMPARE(cancelSpy.count(), 0);

    activationSpy.clear();
    const QPoint dragEnd = start + QPoint(140, 0);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&window, start + QPoint(20, 0), 30);
    QTest::mouseMove(&window, dragEnd, 30);
    QCoreApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(first->property("dragging").toBool(), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(panel->property("dragTargetIndex").toInt(), 2, 1000);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, dragEnd);
    QTRY_COMPARE_WITH_TIMEOUT(reorderSpy.count(), 1, 1000);
    QCOMPARE(activationSpy.count(), 0);
    QCOMPARE(finishSpy.count(), 1);
    QCOMPARE(cancelSpy.count(), 0);

    QTest::mouseMove(&window, start);
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, start);
    QTRY_COMPARE_WITH_TIMEOUT(activationSpy.count(), 1, 1000);
    QCOMPARE(finishSpy.count(), 1);
    QCOMPARE(cancelSpy.count(), 0);

    delete panel;
}

void DockHoverQmlTest::pointerHandlerCancellationRestoresWithoutReorder()
{
    DockController controller;
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop")};
    controller.applyConfig(configFor(QStringLiteral("none"), pins));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(400, 120);
    window.show();
    QTest::qWait(20);

    QQuickItem *first = delegate(panel, QStringLiteral("one.desktop"));
    QVERIFY(first);
    QSignalSpy activationSpy(first, SIGNAL(activated(QString)));
    QSignalSpy finishSpy(first, SIGNAL(dragFinished(QString)));
    QSignalSpy cancelSpy(first, SIGNAL(dragCanceled(QString)));
    QSignalSpy reorderSpy(panel, SIGNAL(reorderRequested(QString,int)));
    QVERIFY(activationSpy.isValid() && finishSpy.isValid() && cancelSpy.isValid()
            && reorderSpy.isValid());

    const QPoint start = itemCenter(window, first);
    const QPoint dragEnd = start + QPoint(140, 0);
    QTest::mouseMove(&window, start);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&window, start + QPoint(20, 0), 30);
    QTest::mouseMove(&window, dragEnd, 30);
    QCoreApplication::processEvents();
    QTRY_VERIFY_WITH_TIMEOUT(first->property("dragging").toBool(), 1000);
    QVERIFY(QMetaObject::invokeMethod(panel, "cancelReorder",
                                      Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop")))));
    QVERIFY(!first->property("dragging").toBool());
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, dragEnd);
    QCoreApplication::processEvents();
    QCOMPARE(reorderSpy.count(), 0);
    QCOMPARE(activationSpy.count(), 0);

    window.show();
    QTest::qWait(20);
    QTest::mouseMove(&window, start);
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, start);
    QTRY_COMPARE_WITH_TIMEOUT(activationSpy.count(), 1, 1000);
    QCOMPARE(reorderSpy.count(), 0);
    QCOMPARE(cancelSpy.count(), 0);

    delete panel;
}

void DockHoverQmlTest::pointerHandlerGrabTransitionsFinalizeExactlyOnce()
{
    DockController controller;
    controller.applyConfig(configFor(QStringLiteral("none"),
                                     {QStringLiteral("one.desktop")}));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));

    QQuickItem *item = delegate(panel, QStringLiteral("one.desktop"));
    QVERIFY(item);
    QSignalSpy startedSpy(item, SIGNAL(dragStarted(QString)));
    QSignalSpy finishSpy(item, SIGNAL(dragFinished(QString)));
    QSignalSpy cancelSpy(item, SIGNAL(dragCanceled(QString)));
    QSignalSpy reorderSpy(panel, SIGNAL(reorderRequested(QString,int)));
    QVERIFY(startedSpy.isValid() && finishSpy.isValid() && cancelSpy.isValid()
            && reorderSpy.isValid());

    const auto transition = [](QPointingDevice::GrabTransition value) {
        return QVariant::fromValue(static_cast<int>(value));
    };
    QVERIFY(QMetaObject::invokeMethod(item, "handleGrabTransition",
                                      Q_ARG(QVariant, transition(
                                          QPointingDevice::GrabPassive))));
    QCOMPARE(startedSpy.count(), 0);
    QCOMPARE(finishSpy.count(), 0);
    QCOMPARE(cancelSpy.count(), 0);

    QVERIFY(QMetaObject::invokeMethod(item, "handleGrabTransition",
                                      Q_ARG(QVariant, transition(
                                          QPointingDevice::GrabExclusive))));
    QCOMPARE(startedSpy.count(), 1);
    QVERIFY(item->property("dragging").toBool());
    QVERIFY(QMetaObject::invokeMethod(item, "handleGrabTransition",
                                      Q_ARG(QVariant, transition(
                                          QPointingDevice::UngrabPassive))));
    QCOMPARE(finishSpy.count(), 0);
    QVERIFY(QMetaObject::invokeMethod(item, "handleGrabTransition",
                                      Q_ARG(QVariant, transition(
                                          QPointingDevice::UngrabExclusive))));
    QCOMPARE(finishSpy.count(), 1);
    QCOMPARE(cancelSpy.count(), 0);
    QCOMPARE(reorderSpy.count(), 0);
    QVERIFY(!item->property("dragging").toBool());
    QVERIFY(QMetaObject::invokeMethod(item, "handleGrabTransition",
                                      Q_ARG(QVariant, transition(
                                          QPointingDevice::UngrabExclusive))));
    QCOMPARE(finishSpy.count(), 1);

    QVERIFY(QMetaObject::invokeMethod(item, "handleGrabTransition",
                                      Q_ARG(QVariant, transition(
                                          QPointingDevice::GrabExclusive))));
    QCOMPARE(startedSpy.count(), 2);
    QVERIFY(QMetaObject::invokeMethod(item, "handleGrabTransition",
                                      Q_ARG(QVariant, transition(
                                          QPointingDevice::CancelGrabExclusive))));
    QCOMPARE(finishSpy.count(), 1);
    QCOMPARE(cancelSpy.count(), 1);
    QCOMPARE(reorderSpy.count(), 0);
    QVERIFY(!item->property("dragging").toBool());
    QVERIFY(QMetaObject::invokeMethod(item, "handleGrabTransition",
                                      Q_ARG(QVariant, transition(
                                          QPointingDevice::CancelGrabExclusive))));
    QCOMPARE(cancelSpy.count(), 1);

    delete panel;
}

void DockHoverQmlTest::dragGeometryRemainsStableDuringMagnificationCollapse()
{
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop")};
    for (int sourceIndex = 0; sourceIndex < pins.size(); ++sourceIndex) {
        DockController controller;
        controller.applyConfig(configFor(QStringLiteral("magnification"), pins));

        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
        engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
        QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
        QVERIFY2(component.status() == QQmlComponent::Ready,
                 qPrintable(component.errorString()));
        auto *panel = qobject_cast<QQuickItem *>(component.create());
        QVERIFY2(panel, qPrintable(component.errorString()));
        QQuickWindow window;
        panel->setParentItem(window.contentItem());
        panel->setY(20);
        window.resize(600, 180);
        const auto centerPanel = [panel]() {
            if (QQuickWindow *window = panel->window())
                panel->setX((window->width() - panel->width()) / 2.0);
        };
        QObject::connect(panel, &QQuickItem::widthChanged, panel, centerPanel);
        centerPanel();
        window.show();
        QTest::qWait(20);

        QQuickItem *source = delegate(panel, pins.at(sourceIndex));
        QQuickItem *icon = iconItem(source);
        QVERIFY(source && icon);
        const qreal sourceSlotCenter = source->mapToItem(
            panel, source->width() / 2.0, source->height() / 2.0).x();
        QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                          Q_ARG(QVariant, QVariant(sourceSlotCenter))));
        const double restingWidth = panel->property("restingWidth").toDouble();
        const double surfaceWidth = panel->width();
        const double surfaceHeight = panel->height();
        QQuickItem *chrome = childWithObjectName(panel, QStringLiteral("dockChrome"));
        QVERIFY(chrome);
        QVERIFY(surfaceWidth > restingWidth);
        QTRY_VERIFY_WITH_TIMEOUT(panel->property("magnificationExtraPrimary").toReal() > 0.0,
                                 1000);
        QTRY_VERIFY_WITH_TIMEOUT(chrome->width() > restingWidth, 1000);

        QTRY_VERIFY_WITH_TIMEOUT(propertyReal(source, "magnificationScale") > 1.1, 2000);
        if (sourceIndex == 0 || sourceIndex == pins.size() - 1) {
            QTRY_VERIFY_WITH_TIMEOUT(qAbs(propertyReal(source, "visualOffsetX")) > 1.0,
                                     1000);
        }
        QTest::qWait(180);
        const QPoint start = visualCenter(window, icon).toPoint();
        const qreal initialCenterX = visualCenter(window, icon).x();
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(&window, start + QPoint(10, 0), 30);
        QTRY_VERIFY_WITH_TIMEOUT(source->property("dragging").toBool(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->property("magnificationExtraPrimary").toReal())
                                     < 0.1,
                                 1000);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(chrome->width() - restingWidth) < 0.1, 1000);
        const qreal centerAfterGrab = visualCenter(window, icon).x();
        const int targetDuringCollapse = panel->property("dragTargetIndex").toInt();
        QVERIFY2(qAbs(centerAfterGrab - initialCenterX) < 1.0,
                 qPrintable(QStringLiteral("source index=%1 initial=%2 after grab=%3")
                                .arg(sourceIndex).arg(initialCenterX).arg(centerAfterGrab)));

        QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->width() - surfaceWidth) < 0.1, 1000);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->height() - surfaceHeight) < 0.1, 1000);
        const qreal centerAfterCollapse = visualCenter(window, icon).x();
        QCOMPARE(panel->property("dragTargetIndex").toInt(), targetDuringCollapse);
        QVERIFY2(qAbs(centerAfterCollapse - centerAfterGrab) < 0.5,
                 qPrintable(QStringLiteral("source index=%1 grab=%2 after collapse=%3")
                                .arg(sourceIndex).arg(centerAfterGrab).arg(centerAfterCollapse)));

        const int direction = sourceIndex == pins.size() - 1 ? -120 : 120;
        const int expectedTarget = sourceIndex == pins.size() - 1 ? 0 : 2;
        const QPoint dragEnd = start + QPoint(direction, 0);
        QTest::mouseMove(&window, dragEnd, 30);
        QTRY_COMPARE_WITH_TIMEOUT(panel->property("dragTargetIndex").toInt(), expectedTarget,
                                  1000);
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, dragEnd);
        QTRY_VERIFY_WITH_TIMEOUT(!source->property("dragging").toBool(), 1000);
        delete panel;
    }
}

void DockHoverQmlTest::modelMoveRefreshesHoverGeometryAndIdentity()
{
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop")};
    QmlCountingPersistence persistence;
    DockController controller(nullptr, nullptr, &persistence);
    controller.applyConfig(configFor(QStringLiteral("magnification"), pins));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(600, 180);
    const auto centerPanel = [panel]() {
        if (QQuickWindow *window = panel->window())
            panel->setX((window->width() - panel->width()) / 2.0);
    };
    QObject::connect(panel, &QQuickItem::widthChanged, panel, centerPanel);
    centerPanel();
    window.show();
    QTest::qWait(20);

    QQuickItem *first = delegate(panel, QStringLiteral("one.desktop"));
    QVERIFY(first);
    const qreal firstSlotCenter = first->mapToItem(
        panel, first->width() / 2.0, first->height() / 2.0).x();
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(firstSlotCenter))));
    QTRY_COMPARE_WITH_TIMEOUT(panel->property("pointerTargetDesktopFileName").toString(),
                              QStringLiteral("one.desktop"), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(propertyReal(first, "magnificationScale") > 1.1, 2000);

    QSignalSpy rowsMovedSpy(controller.appModel(),
                            SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)));
    QVERIFY(rowsMovedSpy.isValid());
    QVERIFY(controller.movePinned(QStringLiteral("one.desktop"), 2));
    QCOMPARE(persistence.calls, 1);
    QTRY_VERIFY_WITH_TIMEOUT(rowsMovedSpy.count() > 0, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(panel->property("pointerTargetDesktopFileName").toString(),
                              QStringLiteral("two.desktop"), 1000);

    QQuickItem *newFirst = delegate(panel, QStringLiteral("two.desktop"));
    QQuickItem *newLast = delegate(panel, QStringLiteral("one.desktop"));
    QVERIFY(newFirst && newLast);
    QTRY_VERIFY_WITH_TIMEOUT(propertyReal(newFirst, "magnificationScale")
                                 > propertyReal(newLast, "magnificationScale"),
                             1000);
    QVERIFY(!panel->property("delegateKeys").isValid());

    QVERIFY(QMetaObject::invokeMethod(panel, "beginReorder",
                                      Q_ARG(QVariant, QVariant(QStringLiteral("two.desktop")))));
    QCOMPARE(panel->property("draggedDesktopFileName").toString(),
             QStringLiteral("two.desktop"));
    QCOMPARE(panel->property("draggedSourceIndex").toInt(), 0);
    QVERIFY(QMetaObject::invokeMethod(panel, "cancelReorder",
                                      Q_ARG(QVariant, QVariant(QStringLiteral("two.desktop")))));

    delete panel;
}

void DockHoverQmlTest::releasePointerRestoresMagnificationTarget()
{
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop")};
    QmlCountingPersistence persistence;
    DockController controller(nullptr, nullptr, &persistence);
    controller.applyConfig(configFor(QStringLiteral("magnification"), pins));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(600, 180);
    const auto centerPanel = [panel]() {
        if (QQuickWindow *window = panel->window())
            panel->setX((window->width() - panel->width()) / 2.0);
    };
    QObject::connect(panel, &QQuickItem::widthChanged, panel, centerPanel);
    centerPanel();
    window.show();
    QTest::qWait(20);

    QQuickItem *source = delegate(panel, QStringLiteral("one.desktop"));
    QQuickItem *icon = iconItem(source);
    QVERIFY(source && icon);
    const qreal firstSlotCenter = source->mapToItem(
        panel, source->width() / 2.0, source->height() / 2.0).x();
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(firstSlotCenter))));
    QTRY_VERIFY_WITH_TIMEOUT(propertyReal(source, "magnificationScale") > 1.1, 2000);
    const QPoint start = visualCenter(window, icon).toPoint();
    QTest::mouseMove(&window, start);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&window, start + QPoint(20, 0), 30);
    QTRY_VERIFY_WITH_TIMEOUT(source->property("dragging").toBool(), 1000);

    const QPoint releasePoint = start + QPoint(140, 0);
    QTest::mouseMove(&window, releasePoint, 30);
    QTRY_COMPARE_WITH_TIMEOUT(panel->property("dragTargetIndex").toInt(), 2, 1000);
    // Model the exclusive grab suppressing HoverHandler updates until the
    // release transition. The DragHandler must supply the authoritative point.
    QObject *hoverHandler = panel->findChild<QObject *>(QStringLiteral("dockHoverHandler"));
    QVERIFY(hoverHandler);
    hoverHandler->setProperty("enabled", false);
    QVERIFY(QMetaObject::invokeMethod(panel, "setPointerInside",
                                      Q_ARG(QVariant, QVariant(false))));
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, releasePoint);
    QTRY_VERIFY_WITH_TIMEOUT(!source->property("dragging").toBool(), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(panel->property("pointerTargetDesktopFileName").toString(),
                              QStringLiteral("three.desktop"), 1000);
    QVERIFY(panel->property("pointerInside").toBool());

    delete panel;
}

void DockHoverQmlTest::reorderLiftsOnlyTheDraggedDelegate()
{
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop")};
    for (const QString &effect : {QStringLiteral("none"), QStringLiteral("lift"),
                                  QStringLiteral("magnification")}) {
        DockController controller;
        controller.applyConfig(configFor(effect, pins));

        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
        engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
        QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
        QVERIFY2(component.status() == QQmlComponent::Ready,
                 qPrintable(component.errorString()));
        auto *panel = qobject_cast<QQuickItem *>(component.create());
        QVERIFY2(panel, qPrintable(component.errorString()));
        QCoreApplication::processEvents();

        QVERIFY(QMetaObject::invokeMethod(panel, "beginReorder",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop")))));
        QVERIFY(QMetaObject::invokeMethod(panel, "updateReorder",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop"))),
                                          Q_ARG(QVariant, QVariant(80.0)),
                                          Q_ARG(QVariant, QVariant(0.0)),
                                          Q_ARG(QVariant, QVariant(0.0))));
        QQuickItem *dragged = delegate(panel, QStringLiteral("one.desktop"));
        QVERIFY(dragged);
        QVERIFY(dragged->property("dragging").toBool());
        QTRY_COMPARE_WITH_TIMEOUT(propertyReal(dragged, "visualOffsetY"), -8.0, 1000);
        for (const QString &key : {QStringLiteral("two.desktop"), QStringLiteral("three.desktop")}) {
            QQuickItem *neighbor = delegate(panel, key);
            QVERIFY(neighbor);
            QTRY_COMPARE_WITH_TIMEOUT(propertyReal(neighbor, "visualOffsetY"), 0.0, 1000);
        }
        QVERIFY(QMetaObject::invokeMethod(panel, "cancelReorder",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop")))));
        delete panel;
    }
}

void DockHoverQmlTest::magnifiedInteractionTargetMatchesVisualBounds()
{
    DockController controller;
    DockConfig config = configFor(QStringLiteral("magnification"),
                                  {QStringLiteral("one.desktop")});
    config.iconSize = 64;
    config.magnificationScale = 2.0;
    controller.applyConfig(config);
    FakeContextMenuController contextMenuController;
    DockSurfaceGeometry surfaceGeometry;

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    panel->setProperty("contextMenuController", QVariant::fromValue(
                                                    static_cast<QObject *>(&contextMenuController)));
    panel->setProperty("dockSurfaceGeometry", QVariant::fromValue(
                                                  static_cast<QObject *>(&surfaceGeometry)));
    panel->setProperty("outputKey", QStringLiteral("output-1"));
    panel->setProperty("outputWidth", 520);
    panel->setProperty("outputHeight", 220);

    QQuickWindow window;
    window.resize(520, 220);
    panel->setParentItem(window.contentItem());
    panel->setY(20);
    const auto centerPanel = [panel, &window]() {
        panel->setX((window.width() - panel->width()) / 2.0);
    };
    QObject::connect(panel, &QQuickItem::widthChanged, panel, centerPanel);
    centerPanel();
    window.show();
    QTest::qWait(20);

    QQuickItem *delegateItem = delegate(panel, QStringLiteral("one.desktop"));
    QQuickItem *icon = iconItem(delegateItem);
    QQuickItem *interactionTarget = childWithObjectName(panel,
                                                         QStringLiteral("interactionTarget-one.desktop"));
    QVERIFY(delegateItem && icon && interactionTarget);
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(panel->width() / 2.0))));
    QTRY_VERIFY_WITH_TIMEOUT(icon->scale() > 1.99, 1000);

    const QRectF visualRect = icon->mapRectToItem(window.contentItem(),
                                                   QRectF(0, 0, icon->width(), icon->height()));
    const QRectF interactionRect = interactionTarget->mapRectToItem(
        window.contentItem(), QRectF(0, 0, interactionTarget->width(), interactionTarget->height()));
    QVERIFY2(qAbs(visualRect.x() - interactionRect.x()) < 1.0,
             qPrintable(QStringLiteral("visual x=%1 interaction x=%2")
                            .arg(visualRect.x()).arg(interactionRect.x())));
    QVERIFY2(qAbs(visualRect.y() - interactionRect.y()) < 1.0,
             qPrintable(QStringLiteral("visual y=%1 interaction y=%2")
                            .arg(visualRect.y()).arg(interactionRect.y())));
    QVERIFY2(qAbs(visualRect.width() - interactionRect.width()) < 1.0,
             qPrintable(QStringLiteral("visual width=%1 interaction width=%2")
                            .arg(visualRect.width()).arg(interactionRect.width())));
    QVERIFY2(qAbs(visualRect.height() - interactionRect.height()) < 1.0,
             qPrintable(QStringLiteral("visual height=%1 interaction height=%2")
                            .arg(visualRect.height()).arg(interactionRect.height())));

    const QRectF panelRect = panel->mapRectToItem(window.contentItem(),
                                                   QRectF(0, 0, panel->width(), panel->height()));
    QVERIFY(panelRect.top() < visualRect.top());
    const QPoint topPoint(qRound(visualRect.center().x()), qRound(visualRect.top() + 2.0));
    QVERIFY(visualRect.contains(topPoint));
    const QPointF topPanelPoint = panel->mapFromItem(window.contentItem(), topPoint.x(),
                                                      topPoint.y());
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                      Q_ARG(QVariant, QVariant(topPanelPoint.x())),
                                      Q_ARG(QVariant, QVariant(topPanelPoint.y()))));
    QVERIFY(panel->property("pointerInside").toBool());
    QSignalSpy activationSpy(delegateItem, SIGNAL(activated(QString)));
    QVERIFY(activationSpy.isValid());
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, topPoint);
    QTRY_COMPARE_WITH_TIMEOUT(activationSpy.count(), 1, 1000);

    QTest::mouseClick(&window, Qt::RightButton, Qt::NoModifier, topPoint);
    QTRY_COMPARE_WITH_TIMEOUT(contextMenuController.presentCount, 1, 1000);

    const QPoint emptyHeadroom(qRound(visualRect.center().x()),
                               qRound(visualRect.top() - 2.0));
    QVERIFY(panelRect.contains(emptyHeadroom));
    QVERIFY(!interactionRect.contains(emptyHeadroom));
    const QPointF emptyPanelPoint = panel->mapFromItem(window.contentItem(),
                                                        emptyHeadroom.x(), emptyHeadroom.y());
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                      Q_ARG(QVariant, QVariant(emptyPanelPoint.x())),
                                      Q_ARG(QVariant, QVariant(emptyPanelPoint.y()))));
    QVERIFY(!panel->property("pointerInside").toBool());
    QCOMPARE(panel->property("pointerTargetDesktopFileName").toString(), QString());
    activationSpy.clear();
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, emptyHeadroom);
    QTest::qWait(50);
    QCOMPARE(activationSpy.count(), 0);

    delete panel;
}

void DockHoverQmlTest::exclusiveDragReleaseInEmptyHeadroomStaysOutsideDock()
{
    DockController controller;
    controller.applyConfig(configFor(QStringLiteral("magnification"),
                                     {QStringLiteral("one.desktop"),
                                      QStringLiteral("two.desktop"),
                                      QStringLiteral("three.desktop")}));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(600, 220);
    const auto centerPanel = [panel, &window]() {
        panel->setX((window.width() - panel->width()) / 2.0);
    };
    QObject::connect(panel, &QQuickItem::widthChanged, panel, centerPanel);
    centerPanel();
    window.show();
    QTest::qWait(20);

    QQuickItem *source = delegate(panel, QStringLiteral("one.desktop"));
    QQuickItem *icon = iconItem(source);
    QVERIFY(source && icon);
    QSignalSpy activationSpy(source, SIGNAL(activated(QString)));
    QSignalSpy reorderSpy(panel, SIGNAL(reorderRequested(QString,int)));
    QVERIFY(activationSpy.isValid() && reorderSpy.isValid());

    const QPoint start = visualCenter(window, icon).toPoint();
    QTest::mouseMove(&window, start);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&window, start + QPoint(20, 0), 30);
    QTRY_VERIFY_WITH_TIMEOUT(source->property("dragging").toBool(), 1000);

    const QRectF panelRect = panel->mapRectToItem(window.contentItem(),
                                                   QRectF(0, 0, panel->width(), panel->height()));
    const QPoint emptyHeadroom(qRound(panelRect.left() + 1.0),
                               qRound(panelRect.top() + 1.0));
    const QQuickItem *interactionTarget = childWithObjectName(
        panel, QStringLiteral("interactionTarget-one.desktop"));
    QVERIFY(interactionTarget);
    const QRectF interactionRect = interactionTarget->mapRectToItem(
        window.contentItem(), QRectF(0, 0, interactionTarget->width(),
                                     interactionTarget->height()));
    QVERIFY(panelRect.contains(emptyHeadroom));
    QVERIFY(!interactionRect.contains(emptyHeadroom));

    QTest::mouseMove(&window, emptyHeadroom, 30);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, emptyHeadroom);
    QTRY_VERIFY_WITH_TIMEOUT(!source->property("dragging").toBool(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!panel->property("pointerInside").toBool(), 1000);
    QCOMPARE(panel->property("pointerTargetDesktopFileName").toString(), QString());
    QCOMPARE(activationSpy.count(), 0);
    QCOMPARE(reorderSpy.count(), 0);

    delete panel;
}

void DockHoverQmlTest::magnifiedInteractionRegionsResolveByVisualStacking()
{
    DockController controller;
    DockConfig config = configFor(QStringLiteral("magnification"),
                                  {QStringLiteral("one.desktop"),
                                   QStringLiteral("two.desktop"),
                                   QStringLiteral("three.desktop")});
    config.iconSize = 64;
    config.magnificationScale = 2.0;
    controller.applyConfig(config);

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(600, 220);
    const auto centerPanel = [panel, &window]() {
        panel->setX((window.width() - panel->width()) / 2.0);
    };
    QObject::connect(panel, &QQuickItem::widthChanged, panel, centerPanel);
    centerPanel();
    window.show();
    QTest::qWait(20);

    QQuickItem *center = delegate(panel, QStringLiteral("two.desktop"));
    QQuickItem *right = delegate(panel, QStringLiteral("three.desktop"));
    QVERIFY(center && right);
    const qreal centerX = center->mapToItem(panel, center->width() / 2.0,
                                            center->height() / 2.0).x();
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(centerX))));
    QTRY_VERIFY_WITH_TIMEOUT(propertyReal(center, "magnificationScale")
                                 > 1.99,
                             1000);
    // The normal spacing-preserving layout avoids accidental overlap. Force a
    // transient visual overlap to exercise the z-order arbitration path.
    right->setProperty("dragging", true);
    right->setProperty("visualOffsetX", -30.0);
    right->setProperty("dragging", false);
    QCoreApplication::processEvents();
    const QRectF centerRect = iconItem(center)->mapRectToItem(
        window.contentItem(), QRectF(0, 0, iconItem(center)->width(), iconItem(center)->height()));
    const QRectF rightRect = iconItem(right)->mapRectToItem(
        window.contentItem(), QRectF(0, 0, iconItem(right)->width(), iconItem(right)->height()));
    QQuickItem *centerTarget = childWithObjectName(panel,
                                                    QStringLiteral("interactionTarget-two.desktop"));
    QQuickItem *rightTarget = childWithObjectName(panel,
                                                   QStringLiteral("interactionTarget-three.desktop"));
    QVERIFY(centerTarget && rightTarget);
    const QRectF overlap = centerRect.intersected(rightRect);
    QVERIFY2(overlap.width() > 1.0 && overlap.height() > 1.0,
             qPrintable(QStringLiteral("center=%1,%2 %3x%4 right=%5,%6 %7x%8")
                            .arg(centerRect.x()).arg(centerRect.y()).arg(centerRect.width())
                            .arg(centerRect.height()).arg(rightRect.x()).arg(rightRect.y())
                            .arg(rightRect.width()).arg(rightRect.height())));

    QSignalSpy centerActivation(center, SIGNAL(activated(QString)));
    QSignalSpy rightActivation(right, SIGNAL(activated(QString)));
    QVERIFY(centerActivation.isValid() && rightActivation.isValid());
    const QPoint intendedPoint(qRound(overlap.left() + 2.0), qRound(overlap.center().y()));
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, intendedPoint);
    QTRY_COMPARE_WITH_TIMEOUT(centerActivation.count(), 1, 1000);
    QCOMPARE(rightActivation.count(), 0);

    delete panel;
}

void DockHoverQmlTest::runtimeOnlyQmlUsesTaskKeyForIdentityAndContextTarget()
{
    DockController controller;
    Astrea::Dock::DockApplicationRuntimeProjection projection;
    Astrea::Dock::DockApplicationRuntimeState runtimeState;
    runtimeState.taskKey = QStringLiteral("app:steam_app_1091500");
    runtimeState.appId = QStringLiteral("Steam_App_1091500");
    runtimeState.displayName = QStringLiteral("Cyberpunk 2077");
    runtimeState.running = true;
    runtimeState.active = true;
    runtimeState.windowCount = 1;
    runtimeState.windowIds.append(QStringLiteral("9"));
    runtimeState.focusSerials.append(1);
    projection.states.insert(runtimeState.taskKey, runtimeState);
    projection.encounterOrder.append(runtimeState.taskKey);
    controller.appModel()->applyRuntimeProjection(projection);

    const QString taskKey = QStringLiteral("app:steam_app_1091500");
    QCOMPARE(controller.appModel()->rowForTaskKey(taskKey), 0);

    FakeContextMenuController contextMenuController;
    DockSurfaceGeometry surfaceGeometry;
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    panel->setProperty("contextMenuController", QVariant::fromValue(
                                                    static_cast<QObject *>(&contextMenuController)));
    panel->setProperty("dockSurfaceGeometry", QVariant::fromValue(
                                                  static_cast<QObject *>(&surfaceGeometry)));
    panel->setProperty("outputKey", QStringLiteral("output-1"));
    panel->setProperty("outputWidth", 400);
    panel->setProperty("outputHeight", 120);

    QQuickWindow qmlWindow;
    qmlWindow.resize(400, 120);
    panel->setParentItem(qmlWindow.contentItem());
    qmlWindow.show();
    QTest::qWait(20);

    QQuickItem *delegateItem = delegate(panel, taskKey);
    QVERIFY(delegateItem);
    QCOMPARE(delegateItem->objectName(), taskKey);
    QSignalSpy activationSpy(delegateItem, SIGNAL(activated(QString)));
    QVERIFY(activationSpy.isValid());
    const QPoint center = itemCenter(qmlWindow, delegateItem);
    QTest::mouseClick(&qmlWindow, Qt::LeftButton, Qt::NoModifier, center);
    QTRY_COMPARE_WITH_TIMEOUT(activationSpy.count(), 1, 1000);
    QCOMPARE(activationSpy.at(0).at(0).toString(), taskKey);

    QTest::mouseClick(&qmlWindow, Qt::RightButton, Qt::NoModifier, center);
    QTRY_COMPARE_WITH_TIMEOUT(contextMenuController.presentCount, 1, 1000);
    QCOMPARE(contextMenuController.lastDesktopFileName, taskKey);

    delete panel;
}

void DockHoverQmlTest::magnifiedVisualRegionAcceptsContextMenu()
{
    DockController dockController;
    dockController.applyConfig(configFor(QStringLiteral("magnification"),
                                         {QStringLiteral("one.desktop")}));
    FakeContextMenuController contextMenuController;
    DockSurfaceGeometry surfaceGeometry;

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &dockController);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    panel->setProperty("contextMenuController", QVariant::fromValue(
                                                    static_cast<QObject *>(&contextMenuController)));
    panel->setProperty("dockSurfaceGeometry", QVariant::fromValue(
                                                  static_cast<QObject *>(&surfaceGeometry)));
    panel->setProperty("outputKey", QStringLiteral("output-1"));
    panel->setProperty("outputWidth", 400);
    panel->setProperty("outputHeight", 120);

    QQuickWindow window;
    window.resize(400, 120);
    panel->setParentItem(window.contentItem());
    const auto centerPanel = [panel, &window]() {
        panel->setX((window.width() - panel->width()) / 2.0);
    };
    QObject::connect(panel, &QQuickItem::widthChanged, panel, centerPanel);
    centerPanel();
    window.show();
    QTest::qWait(20);

    QQuickItem *delegateItem = delegate(panel, QStringLiteral("one.desktop"));
    QQuickItem *icon = iconItem(delegateItem);
    QVERIFY(delegateItem && icon);
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(panel->width() / 2.0))));
    const QRectF restingRect = delegateItem->mapRectToItem(window.contentItem(),
                                                            QRectF(0, 0, delegateItem->width(),
                                                                   delegateItem->height()));
    QTRY_VERIFY_WITH_TIMEOUT(
        icon->scale() > 1.0
            && icon->mapRectToItem(window.contentItem(),
                                   QRectF(0, 0, icon->width(), icon->height())).right()
                > restingRect.right(),
        2000);
    const QRectF visualRect = icon->mapRectToItem(window.contentItem(),
                                                   QRectF(0, 0, icon->width(), icon->height()));
    QVERIFY(visualRect.right() > restingRect.right());
    const QPoint outsideRestingInsideVisual(
        qRound(visualRect.right() - 0.5), qRound(visualRect.center().y()));
    QVERIFY(visualRect.contains(outsideRestingInsideVisual));
    QVERIFY2(!restingRect.contains(outsideRestingInsideVisual),
             qPrintable(QStringLiteral("visual=%1,%2 %3x%4 resting=%5,%6 %7x%8 point=%9,%10")
                            .arg(visualRect.x()).arg(visualRect.y())
                            .arg(visualRect.width()).arg(visualRect.height())
                            .arg(restingRect.x()).arg(restingRect.y())
                            .arg(restingRect.width()).arg(restingRect.height())
                            .arg(outsideRestingInsideVisual.x())
                            .arg(outsideRestingInsideVisual.y())));

    QTest::mouseClick(&window, Qt::RightButton, Qt::NoModifier, outsideRestingInsideVisual);
    QTRY_COMPARE_WITH_TIMEOUT(contextMenuController.presentCount, 1, 1000);
    QCOMPARE(contextMenuController.lastDesktopFileName, QStringLiteral("desktop:one.desktop"));
    QCOMPARE(contextMenuController.lastOutputKey, QStringLiteral("output-1"));
    QVERIFY(contextMenuController.lastRectangle.width() > 0);
    QVERIFY(contextMenuController.lastRectangle.height() > 0);

    delete panel;
}

void DockHoverQmlTest::mainSurfaceUsesExplicitOutputGeometryWithoutWindowScreen()
{
    DockController dockController;
    dockController.applyConfig(configFor(QStringLiteral("none"),
                                         {QStringLiteral("one.desktop")}));
    FakeContextMenuController contextMenuController;
    FakeInputRegionBridge inputRegionBridge;
    DockSurfaceGeometry surfaceGeometry;
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    ThemeController theme(temporary.filePath(QStringLiteral("missing-theme.json")));
    theme.setShellStyle(2);

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &dockController);
    engine.rootContext()->setContextProperty(QStringLiteral("ContextMenuController"),
                                              &contextMenuController);
    engine.rootContext()->setContextProperty(QStringLiteral("DockSurfaceGeometry"),
                                              &surfaceGeometry);
    engine.rootContext()->setContextProperty(QStringLiteral("DockInputRegion"),
                                              &inputRegionBridge);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &theme);

    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockMainPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(component.create());
    QVERIFY2(window, qPrintable(component.errorString()));
    auto *panel = window->findChild<QQuickItem *>(QStringLiteral("dockPanel"));
    QVERIFY(panel);
    auto *tooltipWindow = window->findChild<QQuickWindow *>(
        QStringLiteral("dockTooltipSurface"));
    QVERIFY2(tooltipWindow, "Dock Main must own the native tooltip surface");
    auto *tooltipBubble = tooltipWindow->findChild<QQuickItem *>(
        QStringLiteral("dockTooltipBubble"));
    auto *tooltipShape = tooltipWindow->findChild<QQuickItem *>(
        QStringLiteral("dockTooltipShape"));
    auto *tooltipStroke = tooltipWindow->findChild<QObject *>(
        QStringLiteral("dockTooltipStroke"));
    auto *tooltipText = tooltipWindow->findChild<QQuickItem *>(
        QStringLiteral("dockTooltipText"));
    auto *tooltipFontMetrics = tooltipWindow->findChild<QObject *>(
        QStringLiteral("dockTooltipFontMetrics"));
    auto *tooltipMaterial = tooltipWindow->findChild<QObject *>(
        QStringLiteral("dockTooltipMaterial"));
    QVERIFY(tooltipBubble && tooltipShape && tooltipStroke && tooltipText
            && tooltipFontMetrics && tooltipMaterial);
    QVERIFY(!tooltipWindow->findChild<QQuickItem *>(
        QStringLiteral("dockTooltipBackdropBody")));
    QVERIFY(!tooltipWindow->findChild<QQuickItem *>(QStringLiteral("dockTooltipCard")));
    QVERIFY(!tooltipWindow->findChild<QQuickItem *>(QStringLiteral("dockTooltipPointer")));
    QCOMPARE(tooltipBubble->property("pointerWidth").toInt(), 14);
    QCOMPARE(tooltipBubble->property("pointerDepth").toInt(), 8);
    QCOMPARE(tooltipStroke->property("strokeWidth").toReal(), 1.0);
    QFile bubbleSource(QString::fromUtf8(
        DOCK_SOURCE_DIR "/qml/components/DockTooltipBubble.qml"));
    QVERIFY(bubbleSource.open(QIODevice::ReadOnly));
    const QString bubbleSourceText = QString::fromUtf8(bubbleSource.readAll());
    QCOMPARE(bubbleSourceText.count(QStringLiteral("ShapePath {")), 1);
    QCOMPARE(bubbleSourceText.count(QStringLiteral("MultiEffect {")), 1);
    QVERIFY(bubbleSourceText.contains(QStringLiteral("preferredRendererType: Shape.CurveRenderer")));
    QVERIFY(bubbleSourceText.contains(QStringLiteral("layer.textureSize:")));
    QVERIFY(bubbleSourceText.contains(QStringLiteral("layer.smooth: true")));
    QVERIFY(bubbleSourceText.contains(QStringLiteral("shadowEnabled: true")));
    QVERIFY(window->metaObject()->indexOfProperty("outputWidth") >= 0);
    QVERIFY(window->metaObject()->indexOfProperty("outputHeight") >= 0);

    // This is the same explicit property publication performed by the
    // layer-shell surface. No QML window.screen geometry is involved.
    window->setProperty("outputKey", QStringLiteral("output-1"));
    window->setProperty("outputWidth", 1920);
    window->setProperty("outputHeight", 1080);
    QTRY_COMPARE_WITH_TIMEOUT(panel->property("outputWidth").toInt(), 1920, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(panel->property("outputHeight").toInt(), 1080, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(tooltipWindow->width(), 1920, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(tooltipWindow->height(), 1080, 1000);
    QVERIFY(tooltipWindow->flags().testFlag(Qt::WindowTransparentForInput));
    QVERIFY(tooltipWindow->flags().testFlag(Qt::WindowDoesNotAcceptFocus));
    QVERIFY(!tooltipWindow->isVisible());
    window->show();

    const auto showTooltip = [tooltipWindow](const QString &title, const QRect &anchor,
                                             const QString &position) {
        return QMetaObject::invokeMethod(
            tooltipWindow, "showTooltip", Qt::DirectConnection,
            Q_ARG(QVariant, QVariant(title)), Q_ARG(QVariant, QVariant(anchor)),
            Q_ARG(QVariant, QVariant(position)));
    };
    const auto verifyPath = [tooltipBubble] {
        const QString path = tooltipBubble->property("pathData").toString();
        return path.startsWith(QStringLiteral("M "))
            && path.count(QLatin1Char('M')) == 1 && path.count(QLatin1Char('Z')) == 1
            && path.count(QLatin1Char('A')) >= 3
            && !path.contains(QStringLiteral(" Q "));
    };
    QVERIFY(showTooltip(QStringLiteral("Tooltip App"), QRect(1880, 1000, 48, 48),
                        QStringLiteral("bottom")));
    QTRY_VERIFY_WITH_TIMEOUT(tooltipBubble->isVisible(), 1000);
    QVERIFY(tooltipShape->isVisible());
    QCOMPARE(tooltipBubble->property("bodyHeight").toInt(), 34);
    QCOMPARE(tooltipBubble->property("bodyRadius").toReal(), 17.0);
    const qreal radius = tooltipBubble->property("bodyRadius").toReal();
    const qreal halfPointer = tooltipBubble->property("pointerWidth").toReal() / 2.0;
    const qreal expectedSideTailArcInset =
        radius - qSqrt(radius * radius - halfPointer * halfPointer);
    const qreal sideTailArcInset = tooltipBubble->property("sideTailArcInset").toReal();
    QVERIFY(sideTailArcInset > 0.0);
    QVERIFY(sideTailArcInset < halfPointer);
    QVERIFY(qAbs(sideTailArcInset - expectedSideTailArcInset) < 0.001);
    const QString bottomPath = tooltipBubble->property("pathData").toString();
    QVERIFY(bottomPath.startsWith(QStringLiteral("M ")));
    QCOMPARE(bottomPath.count(QLatin1Char('M')), 1);
    QCOMPARE(bottomPath.count(QLatin1Char('Z')), 1);
    QCOMPARE(tooltipText->property("text").toString(), QStringLiteral("Tooltip App"));
    const QColor frostedFill = tooltipBubble->property("fillColor").value<QColor>();
    QVERIFY(frostedFill.alphaF() >= 0.35 && frostedFill.alphaF() <= 0.50);
    const QRect tooltipRect = tooltipWindow->property("cardRect").toRect();
    QVERIFY(tooltipRect.left() >= 0 && tooltipRect.right() < 1920);
    QVERIFY(tooltipRect.top() >= 0 && tooltipRect.bottom() < 1080);
    QCOMPARE(tooltipText->x(), qreal(tooltipRect.x() + 16));
    QCOMPARE(tooltipText->y(), qreal(tooltipRect.y()));
    QCOMPARE(tooltipText->width(), qreal(tooltipRect.width() - 32));
    QCOMPARE(tooltipText->height(), qreal(tooltipRect.height()));
    QCOMPARE(tooltipText->property("font").value<QFont>().pixelSize(), 13);
    QCOMPARE(tooltipText->property("font").value<QFont>().weight(), QFont::Medium);
    QCOMPARE(tooltipBubble->property("cardWidth").toReal(), qreal(tooltipRect.width()));
    QCOMPARE(tooltipWindow->property("cardPlacement").toMap()
                 .value(QStringLiteral("pointerEdge")).toString(),
             QStringLiteral("bottom"));
    const auto effectRegions = tooltipWindow->findChildren<AstreaBackdropEffectRegions *>();
    QCOMPARE(effectRegions.size(), 1);
    QVERIFY(effectRegions.constFirst()->enabled());
    auto *analyticDescriptor = effectRegions.constFirst()->analyticCoverage();
    QVERIFY(analyticDescriptor);
    QCOMPARE(analyticDescriptor->item(), tooltipBubble);
    QVERIFY(analyticDescriptor->roundedRectEnabled());
    QVERIFY(analyticDescriptor->triangleEnabled());
    const auto backdropDescriptors = tooltipWindow->findChildren<AstreaBackdropRegion *>();
    QCOMPARE(backdropDescriptors.size(), 1);
    AstreaBackdropRegion *bubbleDescriptor = backdropDescriptors.constFirst();
    QCOMPARE(bubbleDescriptor->item(), tooltipBubble);
    QCOMPARE(bubbleDescriptor->radius(), 0.0);
    const QVariantList bodyRectangles =
        tooltipBubble->property("backdropBodyRectangles").toList();
    const QVariantList tailRectangles =
        tooltipBubble->property("backdropTailRectangles").toList();
    const QVariantList bubbleRectangles =
        tooltipBubble->property("backdropRectangles").toList();
    QVERIFY(!bodyRectangles.isEmpty());
    QVERIFY(bodyRectangles.size() <= tooltipBubble->property("bodyHeight").toInt());
    QVERIFY(!tailRectangles.isEmpty());
    QVERIFY(tailRectangles.size() <= tooltipBubble->property("pointerDepth").toInt());
    QCOMPARE(bubbleRectangles.size(), bodyRectangles.size() + tailRectangles.size());
    QCOMPARE(bubbleDescriptor->localRectangles(), bubbleRectangles);
    QTRY_VERIFY_WITH_TIMEOUT(!effectRegions.constFirst()->resolvedRegion().isEmpty(), 1000);

    const auto verifyBackdropEdge = [&](const QString &edge, const QRect &anchor,
                                        const QString &dockPosition) {
        QVERIFY(showTooltip(edge, anchor, dockPosition));
        QTRY_VERIFY_WITH_TIMEOUT(tooltipBubble->isVisible(), 1000);
        QCOMPARE(tooltipWindow->property("cardPlacement").toMap()
                     .value(QStringLiteral("pointerEdge")).toString(), edge);

        const QVariantList slices =
            tooltipBubble->property("backdropTailRectangles").toList();
        QVERIFY(!slices.isEmpty());
        QVERIFY(slices.size() <= tooltipBubble->property("pointerDepth").toInt());
        const auto resolved = effectRegions.constFirst()->resolvedRegion();
        QVERIFY(!resolved.isEmpty());
        QVERIFY(resolved.size() <= 96);
        const QRect currentCardRect = tooltipWindow->property("cardRect").toRect();
        QVERIFY(backdropContains(resolved, currentCardRect.center()));

        const qreal bodyX = tooltipBubble->property("bodyX").toReal();
        const qreal bodyY = tooltipBubble->property("bodyY").toReal();
        const qreal cardWidth = tooltipBubble->property("cardWidth").toReal();
        const qreal bodyHeight = tooltipBubble->property("bodyHeight").toReal();
        const qreal center = tooltipBubble->property("pointerCenter").toReal();
        const qreal depth = tooltipBubble->property("pointerDepth").toReal();
        const QPointF tailBaseA = tooltipBubble->property("tailBaseA").toPointF();
        const QPointF tailBaseB = tooltipBubble->property("tailBaseB").toPointF();
        const QPointF localTailTip = tooltipBubble->property("tailTip").toPointF();
        const qreal renderPadding = tooltipBubble->property("renderPadding").toReal();
        QVERIFY(renderPadding > tooltipStroke->property("strokeWidth").toReal() / 2.0);
        const QRectF bodyRect = tooltipBubble->mapRectToItem(
            tooltipWindow->contentItem(), QRectF(bodyX, bodyY, cardWidth, bodyHeight));
        QVERIFY(qAbs(bodyRect.x() - currentCardRect.x()) < 0.01);
        QVERIFY(qAbs(bodyRect.y() - currentCardRect.y()) < 0.01);
        QVERIFY(qAbs(bodyRect.width() - currentCardRect.width()) < 0.01);
        QVERIFY(qAbs(bodyRect.height() - currentCardRect.height()) < 0.01);

        const qreal pathLeft = edge == QStringLiteral("left") ? bodyX - depth : bodyX;
        const qreal pathRight = edge == QStringLiteral("right")
            ? bodyX + cardWidth + depth : bodyX + cardWidth;
        const qreal pathTop = edge == QStringLiteral("top") ? bodyY - depth : bodyY;
        const qreal pathBottom = edge == QStringLiteral("bottom")
            ? bodyY + bodyHeight + depth : bodyY + bodyHeight;
        QVERIFY(pathLeft > tooltipStroke->property("strokeWidth").toReal() / 2.0);
        QVERIFY(pathTop > tooltipStroke->property("strokeWidth").toReal() / 2.0);
        QVERIFY(tooltipBubble->width() - pathRight
                > tooltipStroke->property("strokeWidth").toReal() / 2.0);
        QVERIFY(tooltipBubble->height() - pathBottom
                > tooltipStroke->property("strokeWidth").toReal() / 2.0);

        const QPointF outputTip = tooltipBubble->mapToItem(
            tooltipWindow->contentItem(), localTailTip);
        qreal expectedGap = 0.0;
        if (edge == QStringLiteral("bottom")) {
            QVERIFY(qAbs(outputTip.x() - (currentCardRect.x() + center)) < 0.01);
            expectedGap = anchor.top() - outputTip.y();
        } else if (edge == QStringLiteral("top")) {
            QVERIFY(qAbs(outputTip.x() - (currentCardRect.x() + center)) < 0.01);
            expectedGap = outputTip.y() - anchor.bottom() - 1;
        } else if (edge == QStringLiteral("left")) {
            QVERIFY(qAbs(outputTip.y() - (currentCardRect.y() + center)) < 0.01);
            expectedGap = outputTip.x() - anchor.right() - 1;
        } else {
            QVERIFY(qAbs(outputTip.y() - (currentCardRect.y() + center)) < 0.01);
            expectedGap = anchor.left() - outputTip.x();
        }
        QCOMPARE(expectedGap, qreal(3.0));

        const auto resolvedAnalytic = effectRegions.constFirst()->resolvedAnalyticCoverage();
        QVERIFY(resolvedAnalytic.has_value());
        QCOMPARE(resolvedAnalytic->roundedRect->rect, bodyRect);
        QCOMPARE(resolvedAnalytic->roundedRect->radius, qreal(17.0));
        QCOMPARE(resolvedAnalytic->triangle->a, tooltipBubble->mapToItem(
                     tooltipWindow->contentItem(), tailBaseA));
        QCOMPARE(resolvedAnalytic->triangle->b, tooltipBubble->mapToItem(
                     tooltipWindow->contentItem(), tailBaseB));
        QCOMPARE(resolvedAnalytic->triangle->c, outputTip);
        const auto analyticWork = effectRegions.constFirst()->resolvedAnalyticWorkRegion();
        QCOMPARE(analyticWork.size(), 1);
        const QRectF triangleBounds(
            qMin(tailBaseA.x(), qMin(tailBaseB.x(), localTailTip.x())),
            qMin(tailBaseA.y(), qMin(tailBaseB.y(), localTailTip.y())),
            qMax(tailBaseA.x(), qMax(tailBaseB.x(), localTailTip.x()))
                - qMin(tailBaseA.x(), qMin(tailBaseB.x(), localTailTip.x())),
            qMax(tailBaseA.y(), qMax(tailBaseB.y(), localTailTip.y()))
                - qMin(tailBaseA.y(), qMin(tailBaseB.y(), localTailTip.y())));
        const QRectF localShapeBounds = QRectF(bodyX, bodyY, cardWidth, bodyHeight)
            .united(triangleBounds);
        const QPointF mappedTopLeft = tooltipBubble->mapToItem(
            tooltipWindow->contentItem(), localShapeBounds.topLeft());
        const QPointF mappedBottomRight = tooltipBubble->mapToItem(
            tooltipWindow->contentItem(), localShapeBounds.bottomRight());
        const int expectedLeft = qMax(0, qFloor(mappedTopLeft.x())
                                         - AstreaBackdropEffectRegions::analyticCoverageGuard);
        const int expectedTop = qMax(0, qFloor(mappedTopLeft.y())
                                        - AstreaBackdropEffectRegions::analyticCoverageGuard);
        const int expectedRight = qMin(tooltipWindow->width(),
                                       qCeil(mappedBottomRight.x())
                                           + AstreaBackdropEffectRegions::analyticCoverageGuard);
        const int expectedBottom = qMin(tooltipWindow->height(),
                                        qCeil(mappedBottomRight.y())
                                            + AstreaBackdropEffectRegions::analyticCoverageGuard);
        QCOMPARE(analyticWork.constFirst(),
                 QRect(expectedLeft, expectedTop, expectedRight - expectedLeft,
                       expectedBottom - expectedTop));
        QVERIFY(analyticWork != resolved);

        const QVariantList currentBodyRectangles =
            tooltipBubble->property("backdropBodyRectangles").toList();
        const QVariantList currentTailRectangles =
            tooltipBubble->property("backdropTailRectangles").toList();
        const QVariantList currentBubbleRectangles =
            tooltipBubble->property("backdropRectangles").toList();
        const QRectF analyticBody(bodyX, bodyY, cardWidth, bodyHeight);
        QCOMPARE(currentBubbleRectangles.size(),
                 currentBodyRectangles.size() + currentTailRectangles.size());
        QCOMPARE(bubbleDescriptor->localRectangles(), currentBubbleRectangles);
        const QVector<QRect> actualBodyRows = integerRectangles(currentBodyRectangles);
        const QVector<QRect> expectedBodyRows =
            referenceCapsulePixelCenterRows(analyticBody);
        QCOMPARE(actualBodyRows, expectedBodyRows);
        for (const QRect &row : actualBodyRows) {
            for (int x = row.x(); x < row.x() + row.width(); ++x) {
                QVERIFY(pixelCenterInsideCapsule(
                    analyticBody, QPointF(x + 0.5, row.y() + 0.5)));
            }
            QVERIFY(!pixelCenterInsideCapsule(
                analyticBody, QPointF(row.x() - 0.5, row.y() + 0.5)));
            QVERIFY(!pixelCenterInsideCapsule(
                analyticBody, QPointF(row.x() + row.width() + 0.5,
                                      row.y() + 0.5)));
        }
        QCOMPARE(integerRectangles(currentTailRectangles),
                 referenceTailPixelCenterSlices(tailBaseA, tailBaseB, localTailTip,
                                                QRectF(bodyX, bodyY, cardWidth, bodyHeight),
                                                edge));
        for (const QVariant &value : currentBodyRectangles) {
            const QRectF rectangle = value.toRectF();
            QVERIFY(qAbs(rectangle.x() - qRound(rectangle.x())) < 0.001);
            QVERIFY(qAbs(rectangle.y() - qRound(rectangle.y())) < 0.001);
            QVERIFY(qAbs(rectangle.width() - qRound(rectangle.width())) < 0.001);
            QVERIFY(qAbs(rectangle.height() - 1.0) < 0.001);
        }
        for (const QVariant &value : currentTailRectangles) {
            const QRectF rectangle = value.toRectF();
            QVERIFY(qAbs(rectangle.x() - qRound(rectangle.x())) < 0.001);
            QVERIFY(qAbs(rectangle.y() - qRound(rectangle.y())) < 0.001);
            QVERIFY(qAbs(rectangle.width() - qRound(rectangle.width())) < 0.001);
            QVERIFY(qAbs(rectangle.height() - qRound(rectangle.height())) < 0.001);
        }
        QVERIFY(currentBubbleRectangles.size() < 96);
        QCOMPARE(resolved.size(),
                 (edge == QStringLiteral("top") || edge == QStringLiteral("bottom"))
                     ? 41 : 42);

        const qreal shapeLeft = qMin(analyticBody.left(),
                                     qMin(tailBaseA.x(), qMin(tailBaseB.x(), localTailTip.x())));
        const qreal shapeTop = qMin(analyticBody.top(),
                                   qMin(tailBaseA.y(), qMin(tailBaseB.y(), localTailTip.y())));
        const qreal shapeRight = qMax(analyticBody.right(),
                                      qMax(tailBaseA.x(), qMax(tailBaseB.x(), localTailTip.x())));
        const qreal shapeBottom = qMax(analyticBody.bottom(),
                                       qMax(tailBaseA.y(), qMax(tailBaseB.y(), localTailTip.y())));
        const int firstX = qFloor(shapeLeft);
        const int firstY = qFloor(shapeTop);
        const int endX = qCeil(shapeRight);
        const int endY = qCeil(shapeBottom);
        for (int y = firstY; y < endY; ++y) {
            for (int x = firstX; x < endX; ++x) {
                const QPointF sample(x + 0.5, y + 0.5);
                const bool expected = pixelCenterInsideCapsule(analyticBody, sample)
                    || pixelCenterInsideTriangle(tailBaseA, tailBaseB, localTailTip, sample);
                const bool actual = std::any_of(
                    currentBubbleRectangles.cbegin(), currentBubbleRectangles.cend(),
                    [x, y](const QVariant &value) { return value.toRectF().contains(x + 0.5, y + 0.5); });
                QVERIFY2(actual == expected,
                         qPrintable(QStringLiteral("Fallback silhouette mismatch at (%1, %2) for %3")
                                        .arg(x).arg(y).arg(edge)));
            }
        }

        const qreal radius = bodyHeight / 2.0;
        const auto inVectorCapsule = [bodyX, bodyY, cardWidth, radius](QPointF point) {
            const qreal nearestX = qBound(bodyX + radius, point.x(),
                                          bodyX + cardWidth - radius);
            const qreal dx = point.x() - nearestX;
            const qreal dy = point.y() - (bodyY + radius);
            return dx * dx + dy * dy < radius * radius;
        };
        const auto surfaceBodyPoint = [&](qreal x, qreal y) {
            return tooltipBubble->mapToItem(tooltipWindow->contentItem(), QPointF(x, y))
                .toPoint();
        };
        const QPoint centerPoint = surfaceBodyPoint(bodyX + cardWidth / 2.0,
                                                     bodyY + bodyHeight / 2.0);
        const QPoint topCenterPoint = surfaceBodyPoint(bodyX + cardWidth / 2.0, bodyY + 1.0);
        const QPoint bottomCenterPoint = surfaceBodyPoint(
            bodyX + cardWidth / 2.0, bodyY + bodyHeight - 2.0);
        QVERIFY(backdropContains(resolved, centerPoint));
        QVERIFY(backdropContains(resolved, topCenterPoint));
        QVERIFY(backdropContains(resolved, bottomCenterPoint));
        const QPointF insideCapsulePoint(bodyX + 2.0, bodyY + radius);
        const QPointF outsideCapsulePoint(bodyX + 1.0, bodyY + 1.0);
        QVERIFY(inVectorCapsule(insideCapsulePoint));
        QVERIFY(!inVectorCapsule(outsideCapsulePoint));
        QVERIFY(backdropContains(resolved, surfaceBodyPoint(insideCapsulePoint.x(),
                                                            insideCapsulePoint.y())));
        QVERIFY(!backdropContains(resolved, surfaceBodyPoint(outsideCapsulePoint.x(),
                                                             outsideCapsulePoint.y())));
        for (const QPointF corner : {
                 QPointF(bodyX + 1.0, bodyY + 1.0),
                 QPointF(bodyX + cardWidth - 2.0, bodyY + 1.0),
                 QPointF(bodyX + 1.0, bodyY + bodyHeight - 2.0),
                 QPointF(bodyX + cardWidth - 2.0, bodyY + bodyHeight - 2.0)}) {
            QVERIFY(!backdropContains(resolved, surfaceBodyPoint(corner.x(), corner.y())));
        }

        QPointF tailPoint;
        QPointF outsidePoint;
        if (edge == QStringLiteral("bottom")) {
            tailPoint = {bodyX + center, bodyY + bodyHeight + depth / 2.0};
            outsidePoint = {bodyX + center - halfPointer + 1.0,
                            bodyY + bodyHeight + depth - 1.5};
        } else if (edge == QStringLiteral("top")) {
            tailPoint = {bodyX + center, bodyY - depth / 2.0};
            outsidePoint = {bodyX + center - halfPointer + 1.0,
                            bodyY - depth + 1.5};
        } else if (edge == QStringLiteral("left")) {
            tailPoint = {bodyX - depth / 2.0, bodyY + center};
            outsidePoint = {bodyX - depth + 1.5,
                            bodyY + center + halfPointer - 1.0};
        } else {
            tailPoint = {bodyX + cardWidth + depth / 2.0, bodyY + center};
            outsidePoint = {bodyX + cardWidth + depth - 1.5,
                            bodyY + center + halfPointer - 1.0};
        }
        const QPoint surfaceTailPoint = tooltipBubble->mapToItem(
            tooltipWindow->contentItem(), tailPoint).toPoint();
        const QPoint surfaceOutsidePoint = tooltipBubble->mapToItem(
            tooltipWindow->contentItem(), outsidePoint).toPoint();
        QVERIFY2(backdropContains(resolved, surfaceTailPoint),
                 qPrintable(QStringLiteral("%1 tail point %2,%3 is not in the backdrop union")
                                .arg(edge).arg(surfaceTailPoint.x()).arg(surfaceTailPoint.y())));
        QVERIFY2(!backdropContains(resolved, surfaceOutsidePoint),
                 qPrintable(QStringLiteral("%1 tail bounding box leaked blur at %2,%3")
                                .arg(edge).arg(surfaceOutsidePoint.x()).arg(surfaceOutsidePoint.y())));
        qInfo().noquote() << "tooltip backdrop rectangles" << edge
                          << "body" << currentBodyRectangles.size()
                          << "tail" << currentTailRectangles.size()
                          << "resolved" << resolved.size();
    };

    // Sober is a compact-label qualification: keep the 72 px pill and verify
    // its curved endcap uses pixel-center coverage rather than whole-band
    // containment, which removed eight pixels from this top scanline.
    const QRect soberAnchor(900, 800, 48, 48);
    QVERIFY(showTooltip(QStringLiteral("Sober"), soberAnchor, QStringLiteral("bottom")));
    QCOMPARE(tooltipWindow->property("title").toString(), QStringLiteral("Sober"));
    QCOMPARE(tooltipText->property("text").toString(), QStringLiteral("Sober"));
    QVERIFY(!tooltipText->property("truncated").toBool());
    const QFontMetricsF soberFontMetrics(tooltipText->property("font").value<QFont>());
    const qreal soberNaturalWidth = soberFontMetrics.horizontalAdvance(QStringLiteral("Sober"));
    QCOMPARE(tooltipWindow->property("naturalCardWidth").toInt(),
             qCeil(soberNaturalWidth) + 32);
    QCOMPARE(tooltipWindow->property("cardWidth").toInt(), 72);
    QCOMPARE(tooltipWindow->property("minimumCardWidth").toInt(), 72);
    QCOMPARE(tooltipBubble->property("bodyHeight").toReal(), 34.0);
    QCOMPARE(tooltipBubble->property("bodyRadius").toReal(), 17.0);
    QCOMPARE(tooltipBubble->property("pointerWidth").toInt(), 14);
    QCOMPARE(tooltipBubble->property("pointerDepth").toInt(), 8);

    const qreal effectiveDpr = tooltipBubble->property("effectiveDevicePixelRatio").toReal();
    const qreal windowDpr = tooltipWindow->devicePixelRatio();
    const qreal rasterScale = tooltipBubble->property("rasterScale").toReal();
    QCOMPARE(tooltipBubble->property("minimumRasterScale").toReal(), 2.0);
    QVERIFY(qAbs(effectiveDpr - windowDpr) < 0.001);
    QCOMPARE(rasterScale, qMax<qreal>(2.0, effectiveDpr));
    QCOMPARE(tooltipBubble->property("rasterTextureWidth").toInt(),
             qCeil(tooltipBubble->width() * rasterScale));
    QCOMPARE(tooltipBubble->property("rasterTextureHeight").toInt(),
             qCeil(tooltipBubble->height() * rasterScale));
    if (qAbs(windowDpr - 1.0) < 0.001) {
        QVERIFY(tooltipBubble->property("rasterTextureWidth").toInt()
                >= qCeil(tooltipBubble->width() * 2.0));
        QVERIFY(tooltipBubble->property("rasterTextureHeight").toInt()
                >= qCeil(tooltipBubble->height() * 2.0));
    }
    qInfo() << "Sober tooltip raster" << "DPR" << effectiveDpr
            << "scale" << rasterScale << "texture"
            << tooltipBubble->property("rasterTextureWidth").toInt() << "x"
            << tooltipBubble->property("rasterTextureHeight").toInt();

    const qreal soberBodyX = tooltipBubble->property("bodyX").toReal();
    const qreal soberBodyY = tooltipBubble->property("bodyY").toReal();
    const qreal soberBodyHeight = tooltipBubble->property("bodyHeight").toReal();
    const QPointF soberLocalTip = tooltipBubble->property("tailTip").toPointF();
    const QPointF soberTip = tooltipBubble->mapToItem(
        tooltipWindow->contentItem(), soberLocalTip);
    QCOMPARE(soberAnchor.top() - soberTip.y(), qreal(3.0));

    const QRectF soberBody(soberBodyX, soberBodyY,
                           tooltipBubble->property("cardWidth").toReal(),
                           soberBodyHeight);
    const QVector<QRect> soberRows = integerRectangles(
        tooltipBubble->property("backdropBodyRectangles").toList());
    QCOMPARE(soberRows, referenceCapsulePixelCenterRows(soberBody));
    QCOMPARE(tooltipBubble->property("bodyRect").toRectF().size(), QSizeF(72.0, 34.0));
    const auto soberCoverage = effectRegions.constFirst()->resolvedAnalyticCoverage();
    QVERIFY(soberCoverage.has_value());
    QCOMPARE(soberCoverage->roundedRect->rect.size(), QSizeF(72.0, 34.0));
    QCOMPARE(soberCoverage->roundedRect->radius, qreal(17.0));
    QCOMPARE(soberCoverage->triangle->c, soberTip);
    const auto soberWorkRegion = effectRegions.constFirst()->resolvedAnalyticWorkRegion();
    QCOMPARE(soberWorkRegion.size(), 1);
    QCOMPARE(soberWorkRegion.constFirst().size(), QSize(76, 46));
    QVERIFY(soberWorkRegion.constFirst().contains(
        tooltipBubble->mapToItem(tooltipWindow->contentItem(),
                                 tooltipBubble->property("bodyRect").toRectF().center())
            .toPoint()));
    QVERIFY(soberRows.size() >= 4);
    const QRect newTopRow = soberRows.constFirst();
    const QRect oldTopRow = wholeBandContainedCapsuleRow(soberBody, 0);
    QCOMPARE(oldTopRow.width(), 38);
    QCOMPARE(newTopRow.width(), 46);
    QVERIFY(newTopRow.width() > oldTopRow.width());
    for (int row = 0; row < 4; ++row) {
        const QRect oldSpan = wholeBandContainedCapsuleRow(soberBody, row);
        const QRect newSpan = soberRows.at(row);
        QCOMPARE(newSpan.y(), qRound(soberBody.y()) + row);
        QVERIFY(newSpan.width() > oldSpan.width());
        qInfo() << "Sober top mask row" << row << "old whole-band span" << oldSpan.x()
                << "width" << oldSpan.width() << "pixel-center span" << newSpan.x()
                << "width" << newSpan.width();
    }

    const auto verifySideSplice = [&](const QString &edge, const qreal expectedBaseX,
                                      const qreal expectedTipX) {
        const QString path = tooltipBubble->property("pathData").toString();
        const QPointF baseA = tooltipBubble->property("tailBaseA").toPointF();
        const QPointF baseB = tooltipBubble->property("tailBaseB").toPointF();
        const QPointF tip = tooltipBubble->property("tailTip").toPointF();
        const auto number = [](const qreal value) {
            return QString::number(qRound64(value * 1000.0) / 1000.0, 'g', 12);
        };
        QString segment;
        if (edge == QStringLiteral("left")) {
            segment = QStringLiteral("A 17 17 0 0 1 %1 %2 L %3 %4 L %5 %6 A 17 17")
                .arg(number(baseB.x()), number(baseB.y()), number(tip.x()), number(tip.y()),
                     number(baseA.x()), number(baseA.y()));
        } else {
            segment = QStringLiteral("A 17 17 0 0 1 %1 %2 L %3 %4 L %5 %6 A 17 17")
                .arg(number(baseA.x()), number(baseA.y()), number(tip.x()), number(tip.y()),
                     number(baseB.x()), number(baseB.y()));
        }
        QVERIFY2(path.contains(segment),
                 qPrintable(QStringLiteral("side tail does not use exported points: %1")
                                .arg(path)));
        QVERIFY(qAbs(baseA.x() - expectedBaseX) < 0.001);
        QVERIFY(qAbs(baseB.x() - expectedBaseX) < 0.001);
        QVERIFY(qAbs(tip.x() - expectedTipX) < 0.001);
        const qreal expectedCenterY = tooltipBubble->property("bodyY").toReal()
            + tooltipBubble->property("pointerCenter").toReal();
        QCOMPARE(baseA.y(), expectedCenterY - 7.0);
        QCOMPARE(baseB.y(), expectedCenterY + 7.0);
        QCOMPARE(tip.y(), expectedCenterY);
    };

    const QFont tooltipFont = tooltipText->property("font").value<QFont>();
    const QFontMetricsF tooltipFontMetricsForTest(tooltipFont);
    QVERIFY(showTooltip(QStringLiteral("A"), QRect(800, 800, 48, 48),
                        QStringLiteral("bottom")));
    QCOMPARE(tooltipWindow->property("cardWidth").toInt(),
             tooltipWindow->property("minimumCardWidth").toInt());
    QCOMPARE(tooltipText->width(), qreal(40.0));

    const qreal zenNaturalWidth = tooltipFontMetricsForTest.horizontalAdvance(
        QStringLiteral("Zen Browser"));
    QVERIFY(showTooltip(QStringLiteral("Zen Browser"), QRect(800, 800, 48, 48),
                        QStringLiteral("bottom")));
    QCOMPARE(tooltipWindow->property("title").toString(), QStringLiteral("Zen Browser"));
    QCOMPARE(tooltipText->property("text").toString(), QStringLiteral("Zen Browser"));
    QVERIFY(!tooltipText->property("truncated").toBool());
    QVERIFY(tooltipWindow->property("minimumCardWidth").toInt() == 72);
    QVERIFY(tooltipWindow->property("cardWidth").toInt()
            > tooltipWindow->property("minimumCardWidth").toInt());
    QVERIFY(qAbs(tooltipWindow->property("naturalTextWidth").toReal()
                 - zenNaturalWidth) < 0.01);
    const int expectedZenCardWidth = qCeil(zenNaturalWidth)
        + 2 * tooltipWindow->property("horizontalTextPadding").toInt();
    QVERIFY(qAbs(tooltipWindow->property("cardWidth").toInt() - expectedZenCardWidth) <= 1);
    QVERIFY(tooltipWindow->property("cardWidth").toInt()
            < tooltipWindow->property("effectiveMaximumCardWidth").toInt());
    qInfo() << "Zen Browser tooltip" << "advance width" << zenNaturalWidth
            << "card width" << tooltipWindow->property("cardWidth").toInt()
            << "effective maximum"
            << tooltipWindow->property("effectiveMaximumCardWidth").toInt();

    QString mediumLongName;
    int mediumLongCardWidth = 0;
    for (int length = 1; length <= 100; ++length) {
        const QString candidate(length, QLatin1Char('W'));
        const int candidateWidth = qCeil(tooltipFontMetricsForTest.horizontalAdvance(candidate))
            + 2 * tooltipWindow->property("horizontalTextPadding").toInt();
        if (candidateWidth > 280
            && candidateWidth < tooltipWindow->property("effectiveMaximumCardWidth").toInt()) {
            mediumLongName = candidate;
            mediumLongCardWidth = candidateWidth;
            break;
        }
    }
    QVERIFY(!mediumLongName.isEmpty());
    QVERIFY(showTooltip(mediumLongName, QRect(800, 800, 48, 48), QStringLiteral("bottom")));
    QCOMPARE(tooltipWindow->property("cardWidth").toInt(), mediumLongCardWidth);
    QVERIFY(tooltipWindow->property("cardWidth").toInt() > 280);
    QVERIFY(!tooltipText->property("truncated").toBool());
    QCOMPARE(tooltipText->property("maximumLineCount").toInt(), 1);
    QCOMPARE(tooltipText->property("lineCount").toInt(), 1);
    const int textFormatIndex = tooltipText->metaObject()->indexOfProperty("textFormat");
    QVERIFY(textFormatIndex >= 0);
    const QMetaProperty textFormatProperty = tooltipText->metaObject()->property(textFormatIndex);
    QCOMPARE(QString::fromLatin1(textFormatProperty.enumerator().valueToKey(
                 tooltipText->property("textFormat").toInt())),
             QStringLiteral("PlainText"));

    const QString veryLongName(1000, QLatin1Char('W'));
    QVERIFY(showTooltip(veryLongName, QRect(800, 800, 48, 48), QStringLiteral("bottom")));
    QCOMPARE(tooltipWindow->property("effectiveMaximumCardWidth").toInt(), 420);
    QCOMPARE(tooltipWindow->property("cardWidth").toInt(), 420);
    QVERIFY(tooltipWindow->property("naturalCardWidth").toInt() > 420);
    QVERIFY(tooltipText->property("truncated").toBool());
    const QRect designMaximumCard = tooltipWindow->property("cardRect").toRect();
    QVERIFY(designMaximumCard.x() >= 8);
    QVERIFY(designMaximumCard.x() + designMaximumCard.width() <= 1920 - 8);

    tooltipWindow->setProperty("outputWidth", 330);
    QTRY_COMPARE_WITH_TIMEOUT(tooltipWindow->width(), 330, 1000);
    const QRect narrowOutputAnchor(140, 800, 48, 48);
    QVERIFY(showTooltip(veryLongName, narrowOutputAnchor, QStringLiteral("bottom")));
    QCOMPARE(tooltipWindow->property("cardWidth").toInt(),
             tooltipWindow->property("effectiveMaximumCardWidth").toInt());
    QCOMPARE(tooltipWindow->property("cardWidth").toInt(), 314);
    QVERIFY(tooltipText->property("truncated").toBool());
    QCOMPARE(tooltipText->property("maximumLineCount").toInt(), 1);
    QCOMPARE(tooltipText->property("lineCount").toInt(), 1);
    const QRect longNameCard = tooltipWindow->property("cardRect").toRect();
    QVERIFY(longNameCard.x() >= 8);
    QVERIFY(longNameCard.y() >= 8);
    QVERIFY(longNameCard.x() + longNameCard.width() <= 330 - 8);
    QVERIFY(longNameCard.y() + longNameCard.height() <= 1080 - 8);
    QCOMPARE(tooltipWindow->property("cardPlacement").toMap()
                 .value(QStringLiteral("pointerEdge")).toString(), QStringLiteral("bottom"));
    const qreal longNameBodyX = tooltipBubble->property("bodyX").toReal();
    const qreal longNameBodyY = tooltipBubble->property("bodyY").toReal();
    const QPointF longNameTip = tooltipBubble->mapToItem(
        tooltipWindow->contentItem(),
        QPointF(longNameBodyX + tooltipBubble->property("pointerCenter").toReal(),
                longNameBodyY + tooltipBubble->property("bodyHeight").toReal()
                    + tooltipBubble->property("pointerDepth").toReal()));
    QCOMPARE(longNameTip.y(), qreal(800 - 3));
    QCOMPARE(longNameTip.x(), QRectF(narrowOutputAnchor).center().x());
    QVERIFY(longNameTip.x() >= 0 && longNameTip.x() < 330);
    const int wrapModeIndex = tooltipText->metaObject()->indexOfProperty("wrapMode");
    QVERIFY(wrapModeIndex >= 0);
    const QMetaProperty wrapModeProperty = tooltipText->metaObject()->property(wrapModeIndex);
    QCOMPARE(QString::fromLatin1(wrapModeProperty.enumerator().valueToKey(
                 tooltipText->property("wrapMode").toInt())),
             QStringLiteral("NoWrap"));
    const int elideIndex = tooltipText->metaObject()->indexOfProperty("elide");
    QVERIFY(elideIndex >= 0);
    const QMetaProperty elideProperty = tooltipText->metaObject()->property(elideIndex);
    QCOMPARE(QString::fromLatin1(elideProperty.enumerator().valueToKey(
                 tooltipText->property("elide").toInt())),
             QStringLiteral("ElideRight"));
    tooltipWindow->setProperty("outputWidth", 1920);
    QTRY_COMPARE_WITH_TIMEOUT(tooltipWindow->width(), 1920, 1000);

    struct TooltipMaterialExpectation {
        int themeMode;
        int shellStyle;
        qreal fillAlpha;
    };
    const QList<TooltipMaterialExpectation> tooltipMaterials{
        {0, 0, 0.06}, {0, 1, 1.0}, {0, 2, 0.46},
        {1, 0, 0.22}, {1, 1, 1.0}, {1, 2, 0.44},
    };
    for (const auto &values : tooltipMaterials) {
        theme.setThemeMode(values.themeMode);
        theme.setShellStyle(values.shellStyle);
        QCoreApplication::processEvents();
        const QColor fill = tooltipBubble->property("fillColor").value<QColor>();
        const QColor tokenFill = tooltipMaterial->property("tooltipSurface").value<QColor>();
        QVERIFY(qAbs(fill.alphaF() - values.fillAlpha) < 0.01);
        QVERIFY(qAbs(tokenFill.alphaF() - values.fillAlpha) < 0.01);
        QCOMPARE(effectRegions.constFirst()->enabled(), values.shellStyle == 2);
    }
    theme.setThemeMode(0);
    theme.setShellStyle(2);
    QCoreApplication::processEvents();

    QVERIFY(showTooltip(QStringLiteral("Top"), QRect(800, 0, 48, 48),
                        QStringLiteral("bottom")));
    QVERIFY(verifyPath());
    QCOMPARE(tooltipWindow->property("cardPlacement").toMap()
                 .value(QStringLiteral("pointerEdge")).toString(), QStringLiteral("top"));
    verifyBackdropEdge(QStringLiteral("top"), QRect(800, 0, 48, 48),
                       QStringLiteral("bottom"));
    QVERIFY(showTooltip(QStringLiteral("Left"), QRect(0, 400, 48, 48),
                        QStringLiteral("left")));
    QVERIFY(verifyPath());
    QCOMPARE(tooltipWindow->property("cardPlacement").toMap()
                 .value(QStringLiteral("pointerEdge")).toString(), QStringLiteral("left"));
    verifySideSplice(QStringLiteral("left"),
                     tooltipBubble->property("bodyX").toReal() + sideTailArcInset,
                     tooltipBubble->property("bodyX").toReal()
                         - tooltipBubble->property("pointerDepth").toReal());
    verifyBackdropEdge(QStringLiteral("left"), QRect(0, 400, 48, 48),
                       QStringLiteral("left"));
    QVERIFY(showTooltip(QStringLiteral("Right"), QRect(1850, 400, 48, 48),
                        QStringLiteral("right")));
    QVERIFY(verifyPath());
    QCOMPARE(tooltipWindow->property("cardPlacement").toMap()
                 .value(QStringLiteral("pointerEdge")).toString(), QStringLiteral("right"));
    const qreal rightBodyX = tooltipBubble->property("bodyX").toReal();
    const qreal rightCardWidth = tooltipBubble->property("cardWidth").toReal();
    verifySideSplice(QStringLiteral("right"),
                     rightBodyX + rightCardWidth - sideTailArcInset,
                     rightBodyX + rightCardWidth
                         + tooltipBubble->property("pointerDepth").toReal());
    verifyBackdropEdge(QStringLiteral("right"), QRect(1850, 400, 48, 48),
                       QStringLiteral("right"));
    verifyBackdropEdge(QStringLiteral("bottom"), QRect(900, 800, 48, 48),
                       QStringLiteral("bottom"));
    QVERIFY(showTooltip(QStringLiteral("Clamped"), QRect(0, 800, 48, 48),
                        QStringLiteral("bottom")));
    QVERIFY(verifyPath());
    QCOMPARE(tooltipWindow->property("cardPlacement").toMap()
                 .value(QStringLiteral("pointerCenter")).toInt(),
             surfaceGeometry.property("tooltipBodyHeight").toInt() / 2
                 + surfaceGeometry.property("tooltipPointerWidth").toInt() / 2);
    QVERIFY(QMetaObject::invokeMethod(tooltipWindow, "hideTooltip", Qt::DirectConnection));
    QTRY_VERIFY_WITH_TIMEOUT(!tooltipBubble->isVisible(), 1000);

    QTRY_VERIFY_WITH_TIMEOUT(delegate(panel, QStringLiteral("one.desktop")) != nullptr, 1000);
    QTest::qWait(20);
    auto *delegateItem = delegate(panel, QStringLiteral("one.desktop"));
    QVERIFY(delegateItem);
    const QPoint clickPoint = delegateItem->mapToItem(window->contentItem(),
                                                       delegateItem->width() / 2.0,
                                                       delegateItem->height() / 2.0)
                                  .toPoint();
    QTest::mouseMove(window, clickPoint);
    QCoreApplication::processEvents();
    QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, clickPoint);
    QTRY_COMPARE_WITH_TIMEOUT(contextMenuController.presentCount, 1, 1000);
    QCOMPARE(contextMenuController.lastOutputKey, QStringLiteral("output-1"));
    QVERIFY(contextMenuController.lastRectangle.center().x() > 800);
    QVERIFY(contextMenuController.lastRectangle.center().x() < 1120);

    delete window;
}

void DockHoverQmlTest::tooltipFollowsMagnifiedIconAndHidesForDockInteractions()
{
    DockController dockController;
    dockController.applyConfig(configFor(QStringLiteral("magnification"), {
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop")
    }));
    FakeContextMenuController contextMenuController;
    FakeTooltipSurface tooltipSurface;
    DockSurfaceGeometry surfaceGeometry;

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.addImageProvider(QStringLiteral("astrea-icon"), new AstreaIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &dockController);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(900, 220);
    panel->setX((window.width() - panel->width()) / 2.0);
    panel->setY((window.height() - panel->height()) / 2.0);
    panel->setProperty("contextMenuController",
                       QVariant::fromValue(static_cast<QObject *>(&contextMenuController)));
    panel->setProperty("dockSurfaceGeometry",
                       QVariant::fromValue(static_cast<QObject *>(&surfaceGeometry)));
    panel->setProperty("dockTooltipSurface",
                       QVariant::fromValue(static_cast<QObject *>(&tooltipSurface)));
    panel->setProperty("outputKey", QStringLiteral("output-1"));
    panel->setProperty("outputWidth", 900);
    panel->setProperty("outputHeight", 600);
    QTest::qWait(40);
    QTRY_VERIFY_WITH_TIMEOUT(delegate(panel, QStringLiteral("one.desktop")) != nullptr, 1000);

    auto *target = delegate(panel, QStringLiteral("one.desktop"));
    QVERIFY(target);
    const QRectF targetRegion = target->property("interactionRegion").toRectF();
    const QPointF pointer(targetRegion.center());
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                      Q_ARG(QVariant, QVariant(pointer.x())),
                                      Q_ARG(QVariant, QVariant(pointer.y()))));
    QTRY_VERIFY_WITH_TIMEOUT(target->property("magnificationScale").toReal() > 1.0, 1500);
    QTRY_VERIFY_WITH_TIMEOUT(tooltipSurface.tooltipVisible(), 1500);

    const QRectF visualRegion = target->property("interactionRegion").toRectF();
    const QPointF windowTopLeft = panel->mapToItem(window.contentItem(), visualRegion.x(), visualRegion.y());
    const QPointF windowBottomRight = panel->mapToItem(
        window.contentItem(), visualRegion.x() + visualRegion.width(),
        visualRegion.y() + visualRegion.height());
    const QRectF windowRegion(windowTopLeft, windowBottomRight);
    const QRect expectedAnchor = surfaceGeometry.outputLocalDelegateRect(
        900, 600, panel->property("surfaceWidth").toInt(),
        panel->property("surfaceHeight").toInt(), dockController.position(),
        dockController.layerShellEdgeMargin(), windowRegion);
    QCOMPARE(tooltipSurface.lastAnchorRect, expectedAnchor);
    QCOMPARE(tooltipSurface.lastTitle, target->property("displayName").toString());
    QCOMPARE(tooltipSurface.lastPosition, dockController.position());

    const int previousShowCount = tooltipSurface.showCount;
    const double initialScale = target->property("magnificationScale").toReal();
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                      Q_ARG(QVariant, QVariant(pointer.x() + 18)),
                                      Q_ARG(QVariant, QVariant(pointer.y()))));
    QCOMPARE(panel->property("pointerTargetTaskKey").toString(),
             QStringLiteral("desktop:one.desktop"));
    QTRY_VERIFY_WITH_TIMEOUT(
        qAbs(target->property("magnificationScale").toReal() - initialScale) > 0.01,
        1000);
    QTRY_VERIFY_WITH_TIMEOUT(tooltipSurface.showCount > previousShowCount, 1000);
    QTest::qWait(160);
    const QRectF updatedRegion = target->property("interactionRegion").toRectF();
    const QPointF updatedTopLeft = panel->mapToItem(
        window.contentItem(), updatedRegion.x(), updatedRegion.y());
    const QPointF updatedBottomRight = panel->mapToItem(
        window.contentItem(), updatedRegion.x() + updatedRegion.width(),
        updatedRegion.y() + updatedRegion.height());
    const QRect expectedUpdatedAnchor = surfaceGeometry.outputLocalDelegateRect(
        900, 600, panel->property("surfaceWidth").toInt(),
        panel->property("surfaceHeight").toInt(), dockController.position(),
        dockController.layerShellEdgeMargin(),
        QRectF(updatedTopLeft, updatedBottomRight));
    QCOMPARE(tooltipSurface.lastAnchorRect, expectedUpdatedAnchor);

    panel->setProperty("draggedDesktopFileName", QStringLiteral("one.desktop"));
    QTRY_VERIFY_WITH_TIMEOUT(!tooltipSurface.tooltipVisible(), 500);
    panel->setProperty("draggedDesktopFileName", QString());
    QTRY_VERIFY_WITH_TIMEOUT(tooltipSurface.tooltipVisible(), 1500);

    QVERIFY(contextMenuController.presentDock(QStringLiteral("one.desktop"), 0, 0,
                                               48, 48, QStringLiteral("output-1")));
    QTRY_VERIFY_WITH_TIMEOUT(!tooltipSurface.tooltipVisible(), 500);
    contextMenuController.clearPresentation();
    QMetaObject::invokeMethod(panel, "setPointerInside",
                              Q_ARG(QVariant, QVariant(false)));
    QTRY_VERIFY_WITH_TIMEOUT(!tooltipSurface.tooltipVisible(), 500);

    delete panel;
}

void DockHoverQmlTest::runningTooltipUsesOnlyTheApplicationDisplayName()
{
    DockController dockController;
    dockController.applyConfig(configFor(QStringLiteral("none"), {
        QStringLiteral("zen-browser.desktop")
    }));
    Astrea::Dock::DockApplicationRuntimeState runtimeState;
    runtimeState.taskKey = QStringLiteral("desktop:zen-browser.desktop");
    runtimeState.desktopFileName = QStringLiteral("zen-browser.desktop");
    runtimeState.displayName = QStringLiteral("Zen Browser");
    runtimeState.running = true;
    runtimeState.windowCount = 4;
    Astrea::Dock::DockApplicationRuntimeProjection projection;
    projection.states.insert(runtimeState.taskKey, runtimeState);
    projection.encounterOrder.append(runtimeState.taskKey);
    dockController.appModel()->applyRuntimeProjection(projection);

    FakeTooltipSurface tooltipSurface;
    DockSurfaceGeometry surfaceGeometry;
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.addImageProvider(QStringLiteral("astrea-icon"), new AstreaIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"),
                                             &dockController);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(900, 220);
    panel->setX((window.width() - panel->width()) / 2.0);
    panel->setY((window.height() - panel->height()) / 2.0);
    panel->setProperty("dockSurfaceGeometry",
                       QVariant::fromValue(static_cast<QObject *>(&surfaceGeometry)));
    panel->setProperty("dockTooltipSurface",
                       QVariant::fromValue(static_cast<QObject *>(&tooltipSurface)));
    panel->setProperty("outputKey", QStringLiteral("output-1"));
    panel->setProperty("outputWidth", 900);
    panel->setProperty("outputHeight", 600);
    tooltipSurface.identitySource = panel;
    window.show();

    QQuickItem *target = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(
        (target = delegate(panel, QStringLiteral("zen-browser.desktop"))) != nullptr, 1000);
    QVERIFY(target->property("runtimeKnown").toBool());
    QVERIFY(target->property("running").toBool());
    QCOMPARE(target->property("windowCount").toInt(), 4);
    QCOMPARE(target->property("displayName").toString(), QStringLiteral("Zen Browser"));
    const QPointF pointer = target->property("interactionRegion").toRectF().center();
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                      Q_ARG(QVariant, QVariant(pointer.x())),
                                      Q_ARG(QVariant, QVariant(pointer.y()))));
    QTRY_VERIFY_WITH_TIMEOUT(tooltipSurface.tooltipVisible(), 1500);
    QCOMPARE(tooltipSurface.lastTitle, QStringLiteral("Zen Browser"));
    QCOMPARE(tooltipSurface.shownTitles.constLast(), QStringLiteral("Zen Browser"));
    QCOMPARE(target->property("windowCount").toInt(), 4);

    delete panel;
}

void DockHoverQmlTest::visibleTooltipReanchorsAfterOutputGeometryChanges()
{
    for (const QString &position : {QStringLiteral("bottom"), QStringLiteral("left")}) {
        DockController dockController;
        DockConfig config = configFor(QStringLiteral("none"), {
            QStringLiteral("one.desktop")
        });
        config.position = position;
        dockController.applyConfig(config);
        FakeTooltipSurface tooltipSurface;
        DockSurfaceGeometry surfaceGeometry;

        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
        engine.addImageProvider(QStringLiteral("astrea-icon"), new AstreaIconProvider);
        engine.rootContext()->setContextProperty(QStringLiteral("DockController"),
                                                 &dockController);
        QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
        QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
        auto *panel = qobject_cast<QQuickItem *>(component.create());
        QVERIFY2(panel, qPrintable(component.errorString()));
        QQuickWindow window;
        panel->setParentItem(window.contentItem());
        window.resize(900, 600);
        panel->setX((window.width() - panel->width()) / 2.0);
        panel->setY((window.height() - panel->height()) / 2.0);
        panel->setProperty("dockSurfaceGeometry",
                           QVariant::fromValue(static_cast<QObject *>(&surfaceGeometry)));
        panel->setProperty("dockTooltipSurface",
                           QVariant::fromValue(static_cast<QObject *>(&tooltipSurface)));
        panel->setProperty("outputWidth", 900);
        panel->setProperty("outputHeight", 600);
        tooltipSurface.identitySource = panel;
        window.show();
        QTest::qWait(40);

        QQuickItem *target = delegate(panel, QStringLiteral("one.desktop"));
        QVERIFY(target);
        const QPointF pointer = target->property("interactionRegion").toRectF().center();
        QVERIFY(QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                          Q_ARG(QVariant, QVariant(pointer.x())),
                                          Q_ARG(QVariant, QVariant(pointer.y()))));
        QTRY_COMPARE_WITH_TIMEOUT(panel->property("pointerTargetTaskKey").toString(),
                                  QStringLiteral("desktop:one.desktop"), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(tooltipSurface.tooltipVisible(), 1500);

        const QRect previousAnchor = tooltipSurface.lastAnchorRect;
        const QString previousTitle = tooltipSurface.lastTitle;
        const QString previousTaskKey = tooltipSurface.shownTaskKeys.constLast();
        const int previousShowCount = tooltipSurface.showCount;

        panel->setProperty("outputWidth", 1200);
        panel->setProperty("outputHeight", 760);
        QTRY_COMPARE_WITH_TIMEOUT(tooltipSurface.showCount, previousShowCount + 1, 300);
        QVERIFY(tooltipSurface.tooltipVisible());
        QCOMPARE(panel->property("pointerTargetTaskKey").toString(), previousTaskKey);
        QCOMPARE(tooltipSurface.lastTitle, previousTitle);
        QCOMPARE(tooltipSurface.shownTaskKeys.constLast(), previousTaskKey);

        const QRectF visualRegion = target->property("interactionRegion").toRectF();
        const QPointF topLeft = panel->mapToItem(window.contentItem(), visualRegion.topLeft());
        const QPointF bottomRight = panel->mapToItem(window.contentItem(), visualRegion.bottomRight());
        const QRect expectedAnchor = surfaceGeometry.outputLocalDelegateRect(
            1200, 760, panel->property("surfaceWidth").toInt(),
            panel->property("surfaceHeight").toInt(), dockController.position(),
            dockController.layerShellEdgeMargin(), QRectF(topLeft, bottomRight));
        QCOMPARE(tooltipSurface.lastAnchorRect, expectedAnchor);
        QVERIFY(tooltipSurface.lastAnchorRect != previousAnchor);

        delete panel;
    }
}

void DockHoverQmlTest::queuedVisibleTooltipRefreshCannotPresentReplacementTarget()
{
    DockController dockController;
    dockController.applyConfig(configFor(QStringLiteral("none"), {
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop")
    }));
    FakeTooltipSurface tooltipSurface;
    DockSurfaceGeometry surfaceGeometry;

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.addImageProvider(QStringLiteral("astrea-icon"), new AstreaIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &dockController);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(900, 220);
    panel->setX((window.width() - panel->width()) / 2.0);
    panel->setY((window.height() - panel->height()) / 2.0);
    panel->setProperty("dockSurfaceGeometry",
                       QVariant::fromValue(static_cast<QObject *>(&surfaceGeometry)));
    panel->setProperty("dockTooltipSurface",
                       QVariant::fromValue(static_cast<QObject *>(&tooltipSurface)));
    panel->setProperty("outputWidth", 900);
    panel->setProperty("outputHeight", 600);
    tooltipSurface.identitySource = panel;
    window.show();
    QTest::qWait(40);

    QObject *delay = panel->findChild<QObject *>(QStringLiteral("dockTooltipDelay"));
    QVERIFY(delay);
    constexpr int delayIntervalMs = 320;
    QVERIFY(delay->setProperty("interval", delayIntervalMs));
    QQuickItem *targetA = delegate(panel, QStringLiteral("one.desktop"));
    QQuickItem *targetB = delegate(panel, QStringLiteral("two.desktop"));
    QVERIFY(targetA && targetB);
    const auto movePointerTo = [panel](QQuickItem *target) {
        const QPointF point = target->property("interactionRegion").toRectF().center();
        return QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                         Q_ARG(QVariant, QVariant(point.x())),
                                         Q_ARG(QVariant, QVariant(point.y())));
    };

    QVERIFY(movePointerTo(targetA));
    QTRY_COMPARE_WITH_TIMEOUT(panel->property("pointerTargetTaskKey").toString(),
                              QStringLiteral("desktop:one.desktop"), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(tooltipSurface.tooltipVisible(), 1000);
    QCOMPARE(tooltipSurface.showCount, 1);
    QCOMPARE(tooltipSurface.shownTaskKeys.constLast(), QStringLiteral("desktop:one.desktop"));

    const int showCountBeforeRefresh = tooltipSurface.showCount;
    panel->setProperty("outputWidth", 1200); // queues a visible refresh with Qt.callLater
    QVERIFY(movePointerTo(targetB)); // keep this before event processing to preserve the race
    QCOMPARE(panel->property("pointerTargetTaskKey").toString(),
             QStringLiteral("desktop:two.desktop"));

    QCoreApplication::processEvents();
    QVERIFY(!tooltipSurface.tooltipVisible());
    QCOMPARE(tooltipSurface.showCount, showCountBeforeRefresh);
    QVERIFY(!tooltipSurface.shownTaskKeys.contains(QStringLiteral("desktop:two.desktop")));

    QTest::qWait(80);
    QVERIFY(!tooltipSurface.tooltipVisible());
    QCOMPARE(tooltipSurface.showCount, showCountBeforeRefresh);
    QVERIFY(!tooltipSurface.shownTaskKeys.contains(QStringLiteral("desktop:two.desktop")));

    QTRY_COMPARE_WITH_TIMEOUT(tooltipSurface.showCount, showCountBeforeRefresh + 1, 1000);
    QVERIFY(tooltipSurface.tooltipVisible());
    QCOMPARE(tooltipSurface.shownTaskKeys.constLast(), QStringLiteral("desktop:two.desktop"));
    QCOMPARE(tooltipSurface.lastTitle, targetB->property("displayName").toString());

    delete panel;
}

void DockHoverQmlTest::modelChangedTooltipRefreshCannotPresentReplacementTarget()
{
    DockController dockController;
    dockController.applyConfig(configFor(QStringLiteral("none"), {
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop")
    }));
    FakeTooltipSurface tooltipSurface;
    DockSurfaceGeometry surfaceGeometry;

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.addImageProvider(QStringLiteral("astrea-icon"), new AstreaIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &dockController);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(900, 220);
    panel->setX((window.width() - panel->width()) / 2.0);
    panel->setY((window.height() - panel->height()) / 2.0);
    panel->setProperty("dockSurfaceGeometry",
                       QVariant::fromValue(static_cast<QObject *>(&surfaceGeometry)));
    panel->setProperty("dockTooltipSurface",
                       QVariant::fromValue(static_cast<QObject *>(&tooltipSurface)));
    panel->setProperty("outputWidth", 900);
    panel->setProperty("outputHeight", 600);
    tooltipSurface.identitySource = panel;
    window.show();
    QTest::qWait(40);

    QObject *delay = panel->findChild<QObject *>(QStringLiteral("dockTooltipDelay"));
    QVERIFY(delay);
    constexpr int delayIntervalMs = 320;
    QVERIFY(delay->setProperty("interval", delayIntervalMs));
    QQuickItem *targetA = delegate(panel, QStringLiteral("one.desktop"));
    QQuickItem *targetB = delegate(panel, QStringLiteral("two.desktop"));
    QVERIFY(targetA && targetB);
    const auto movePointerTo = [panel](QQuickItem *target) {
        const QPointF point = target->property("interactionRegion").toRectF().center();
        return QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                         Q_ARG(QVariant, QVariant(point.x())),
                                         Q_ARG(QVariant, QVariant(point.y())));
    };

    QVERIFY(movePointerTo(targetA));
    QTRY_COMPARE_WITH_TIMEOUT(panel->property("pointerTargetTaskKey").toString(),
                              QStringLiteral("desktop:one.desktop"), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(tooltipSurface.tooltipVisible(), 1000);
    QCOMPARE(tooltipSurface.showCount, 1);
    QCOMPARE(tooltipSurface.shownTaskKeys.constLast(), QStringLiteral("desktop:one.desktop"));

    const int showCountBeforeRefresh = tooltipSurface.showCount;
    // Emit directly to qualify DockPanel's lifecycle boundary, independent of model mutation behavior.
    emit dockController.modelChanged();
    QVERIFY(movePointerTo(targetB)); // do not run the event loop between refresh and replacement
    QCOMPARE(panel->property("pointerTargetTaskKey").toString(),
             QStringLiteral("desktop:two.desktop"));

    QCoreApplication::processEvents();
    QVERIFY(!tooltipSurface.tooltipVisible());
    QCOMPARE(tooltipSurface.showCount, showCountBeforeRefresh);
    QVERIFY(!tooltipSurface.shownTaskKeys.contains(QStringLiteral("desktop:two.desktop")));

    QTest::qWait(80);
    QVERIFY(!tooltipSurface.tooltipVisible());
    QCOMPARE(tooltipSurface.showCount, showCountBeforeRefresh);
    QVERIFY(!tooltipSurface.shownTaskKeys.contains(QStringLiteral("desktop:two.desktop")));

    QTRY_COMPARE_WITH_TIMEOUT(tooltipSurface.showCount, showCountBeforeRefresh + 1, 1000);
    QVERIFY(tooltipSurface.tooltipVisible());
    QCOMPARE(tooltipSurface.shownTaskKeys.constLast(), QStringLiteral("desktop:two.desktop"));
    QCOMPARE(tooltipSurface.lastTitle, targetB->property("displayName").toString());

    delete panel;
}

void DockHoverQmlTest::pendingTooltipFollowsReplacementTarget()
{
    DockController dockController;
    dockController.applyConfig(configFor(QStringLiteral("none"), {
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop")
    }));
    FakeTooltipSurface tooltipSurface;
    DockSurfaceGeometry surfaceGeometry;

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.addImageProvider(QStringLiteral("astrea-icon"), new AstreaIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &dockController);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(900, 220);
    panel->setX((window.width() - panel->width()) / 2.0);
    panel->setY((window.height() - panel->height()) / 2.0);
    panel->setProperty("dockSurfaceGeometry",
                       QVariant::fromValue(static_cast<QObject *>(&surfaceGeometry)));
    panel->setProperty("dockTooltipSurface",
                       QVariant::fromValue(static_cast<QObject *>(&tooltipSurface)));
    panel->setProperty("outputWidth", 900);
    panel->setProperty("outputHeight", 600);
    tooltipSurface.identitySource = panel;
    window.show();
    QTest::qWait(40);

    QObject *delay = panel->findChild<QObject *>(QStringLiteral("dockTooltipDelay"));
    QVERIFY(delay);
    constexpr int delayIntervalMs = 240;
    QVERIFY(delay->setProperty("interval", delayIntervalMs));
    QQuickItem *targetA = delegate(panel, QStringLiteral("one.desktop"));
    QQuickItem *targetB = delegate(panel, QStringLiteral("two.desktop"));
    QVERIFY(targetA && targetB);
    const auto movePointerTo = [panel](QQuickItem *target) {
        const QPointF point = target->property("interactionRegion").toRectF().center();
        return QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                         Q_ARG(QVariant, QVariant(point.x())),
                                         Q_ARG(QVariant, QVariant(point.y())));
    };

    QVERIFY(movePointerTo(targetA));
    QTRY_COMPARE_WITH_TIMEOUT(panel->property("pointerTargetTaskKey").toString(),
                              QStringLiteral("desktop:one.desktop"), 1000);
    QTest::qWait(120);
    QVERIFY(movePointerTo(targetB));
    QTRY_COMPARE_WITH_TIMEOUT(panel->property("pointerTargetTaskKey").toString(),
                              QStringLiteral("desktop:two.desktop"), 1000);
    QTest::qWait(125);

    QVERIFY2(tooltipSurface.shownTitles.isEmpty(),
             "target A must not be presented when its original delay expires");
    QVERIFY(!tooltipSurface.shownTaskKeys.contains(QStringLiteral("desktop:one.desktop")));
    QVERIFY(!tooltipSurface.tooltipVisible());

    QTRY_COMPARE_WITH_TIMEOUT(tooltipSurface.shownTitles.size(), 1, 1000);
    QCOMPARE(tooltipSurface.lastTitle, targetB->property("displayName").toString());
    QCOMPARE(tooltipSurface.shownTitles.constLast(), targetB->property("displayName").toString());
    QCOMPARE(tooltipSurface.shownTaskKeys.constLast(), QStringLiteral("desktop:two.desktop"));
    QVERIFY(!tooltipSurface.shownTaskKeys.contains(QStringLiteral("desktop:one.desktop")));

    delete panel;
}

void DockHoverQmlTest::pendingTooltipDoesNotPresentRemovedTarget()
{
    DockController dockController;
    dockController.applyConfig(configFor(QStringLiteral("none"), {
        QStringLiteral("one.desktop")
    }));
    FakeTooltipSurface tooltipSurface;
    DockSurfaceGeometry surfaceGeometry;

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.addImageProvider(QStringLiteral("astrea-icon"), new AstreaIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &dockController);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    window.resize(900, 220);
    panel->setX((window.width() - panel->width()) / 2.0);
    panel->setY((window.height() - panel->height()) / 2.0);
    panel->setProperty("dockSurfaceGeometry",
                       QVariant::fromValue(static_cast<QObject *>(&surfaceGeometry)));
    panel->setProperty("dockTooltipSurface",
                       QVariant::fromValue(static_cast<QObject *>(&tooltipSurface)));
    panel->setProperty("outputWidth", 900);
    panel->setProperty("outputHeight", 600);
    tooltipSurface.identitySource = panel;
    window.show();
    QTest::qWait(40);

    QObject *delay = panel->findChild<QObject *>(QStringLiteral("dockTooltipDelay"));
    QVERIFY(delay);
    constexpr int delayIntervalMs = 240;
    QVERIFY(delay->setProperty("interval", delayIntervalMs));
    QQuickItem *target = delegate(panel, QStringLiteral("one.desktop"));
    QVERIFY(target);
    const QPointF pointer = target->property("interactionRegion").toRectF().center();
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                      Q_ARG(QVariant, QVariant(pointer.x())),
                                      Q_ARG(QVariant, QVariant(pointer.y()))));
    QTRY_COMPARE_WITH_TIMEOUT(panel->property("pointerTargetTaskKey").toString(),
                              QStringLiteral("desktop:one.desktop"), 1000);
    QTest::qWait(100);

    dockController.applyConfig(configFor(QStringLiteral("none"), {}));
    QTRY_COMPARE_WITH_TIMEOUT(dockController.appModel()->rowCount(), 0, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(panel->property("pointerTargetTaskKey").toString(),
                              QString(), 1000);
    QTest::qWait(160);

    QVERIFY2(tooltipSurface.shownTitles.isEmpty(),
             "a task removed during the hover delay must never be presented");
    QVERIFY(!tooltipSurface.shownTaskKeys.contains(QStringLiteral("desktop:one.desktop")));
    QVERIFY(!tooltipSurface.tooltipVisible());
    QVERIFY(tooltipSurface.lastTitle.isEmpty());
    QVERIFY(tooltipSurface.lastAnchorRect.isNull());

    delete panel;
}

void DockHoverQmlTest::hoverModesAndTransitions()
{
    DockController controller;
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop"), QStringLiteral("four.desktop"),
        QStringLiteral("five.desktop"), QStringLiteral("six.desktop"),
        QStringLiteral("seven.desktop")};
    controller.applyConfig(configFor(QStringLiteral("magnification"), pins));

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
    QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem *>(component.create());
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickWindow window;
    panel->setParentItem(window.contentItem());
    QCoreApplication::processEvents();
    QVERIFY(panel->width() > 0);

    QQuickItem *chrome = childWithObjectName(panel, QStringLiteral("dockChrome"));
    QVERIFY(chrome);

    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(panel->width() / 2.0))));

    QQuickItem *farLeft = delegate(panel, QStringLiteral("one.desktop"));
    QQuickItem *nearLeft = delegate(panel, QStringLiteral("three.desktop"));
    QQuickItem *center = delegate(panel, QStringLiteral("four.desktop"));
    QQuickItem *nearRight = delegate(panel, QStringLiteral("five.desktop"));
    QQuickItem *farRight = delegate(panel, QStringLiteral("seven.desktop"));
    QVERIFY(farLeft && nearLeft && center && nearRight && farRight);
    QTRY_VERIFY_WITH_TIMEOUT(propertyReal(center, "magnificationScale")
                                 > propertyReal(nearLeft, "magnificationScale"),
                             1000);
    QTRY_VERIFY_WITH_TIMEOUT(panel->property("magnificationExtraPrimary").toReal() > 0.0,
                             1000);
    QTRY_VERIFY_WITH_TIMEOUT(chrome->width() > panel->property("restingWidth").toReal(),
                             1000);
    QCOMPARE(propertyReal(nearLeft, "magnificationScale"),
             propertyReal(nearRight, "magnificationScale"));
    QCOMPARE(propertyReal(farLeft, "magnificationScale"),
             propertyReal(farRight, "magnificationScale"));
    QCOMPARE(propertyReal(farLeft, "magnificationScale"), 1.0);
    QVERIFY(propertyReal(nearLeft, "visualOffsetX") < 0.0);
    QVERIFY(propertyReal(nearRight, "visualOffsetX") > 0.0);

    const double rowWidth = pins.size() * controller.delegateWidth()
        + (pins.size() - 1) * controller.itemSpacing();
    const double boundaryX = panel->width() / 2.0
        - rowWidth / 2.0 + controller.delegateWidth() / 2.0
        + 3.5 * controller.itemSpacing() + 3.5 * controller.delegateWidth();
    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(boundaryX))));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(propertyReal(center, "magnificationScale")
                                      - propertyReal(nearRight, "magnificationScale"))
                                 < 0.005,
                             1000);

    QVERIFY(QMetaObject::invokeMethod(panel, "updatePointer",
                                      Q_ARG(QVariant, QVariant(panel->width() / 2.0))));
    QTRY_VERIFY_WITH_TIMEOUT(propertyReal(center, "magnificationScale")
                                 > propertyReal(nearLeft, "magnificationScale"),
                             1000);

    const double restingWidth = panel->property("restingWidth").toDouble();
    const double restingHeight = panel->property("restingHeight").toDouble();
    const double surfaceWidth = panel->width();
    const double surfaceHeight = panel->height();
    QVERIFY(surfaceWidth > restingWidth);
    QVERIFY(surfaceHeight > restingHeight);
    controller.applyConfig(configFor(QStringLiteral("lift"), pins));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->width() - surfaceWidth) < 0.1, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->height() - surfaceHeight) < 0.1, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->property("magnificationExtraPrimary").toReal()) < 0.1,
                             1000);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(chrome->width() - restingWidth) < 0.1, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(propertyReal(center, "magnificationScale"), 1.1, 1000);
    QCOMPARE(propertyReal(farLeft, "magnificationScale"), 1.0);
    QCOMPARE(propertyReal(farRight, "magnificationScale"), 1.0);
    QCOMPARE(propertyReal(center, "visualOffsetY"), -5.0);
    QCOMPARE(propertyReal(farLeft, "visualOffsetY"), 0.0);
    QCOMPARE(propertyReal(farRight, "visualOffsetY"), 0.0);
    QCOMPARE(propertyReal(farLeft, "visualOffsetX"), 0.0);
    QCOMPARE(propertyReal(farRight, "visualOffsetX"), 0.0);

    controller.applyConfig(configFor(QStringLiteral("none"), pins));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->width() - surfaceWidth) < 0.1, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->height() - surfaceHeight) < 0.1, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->property("magnificationExtraPrimary").toReal()) < 0.1,
                             1000);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(chrome->width() - restingWidth) < 0.1, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(propertyReal(center, "magnificationScale"), 1.0, 1000);
    QCOMPARE(propertyReal(center, "visualOffsetY"), 0.0);
    QCOMPARE(propertyReal(center, "visualOffsetX"), 0.0);
    QCOMPARE(propertyReal(farLeft, "magnificationScale"), 1.0);
    QCOMPARE(propertyReal(farRight, "magnificationScale"), 1.0);

    QVERIFY(QMetaObject::invokeMethod(panel, "setPointerInside",
                                      Q_ARG(QVariant, QVariant(false))));
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->property("magnificationExtraPrimary").toReal()) < 0.1,
                             1000);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(chrome->width() - restingWidth) < 0.1, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->width() - surfaceWidth) < 0.1, 1000);
    QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->height() - surfaceHeight) < 0.1, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(propertyReal(center, "magnificationScale"), 1.0, 1000);
    QCOMPARE(propertyReal(center, "visualOffsetX"), 0.0);
    QCOMPARE(propertyReal(center, "visualOffsetY"), 0.0);

    delete panel;
}

void DockHoverQmlTest::reorderPreviewWorksForEveryHoverMode()
{
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop")};
    for (const QString &effect : {QStringLiteral("none"), QStringLiteral("lift"),
                                  QStringLiteral("magnification")}) {
        DockController controller;
        controller.applyConfig(configFor(effect, pins));

        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
        engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
        QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
        QVERIFY2(component.status() == QQmlComponent::Ready,
                 qPrintable(component.errorString()));
        auto *panel = qobject_cast<QQuickItem *>(component.create());
        QVERIFY2(panel, qPrintable(component.errorString()));
        QQuickWindow window;
        panel->setParentItem(window.contentItem());
        QCoreApplication::processEvents();

        QSignalSpy reorderSpy(panel, SIGNAL(reorderRequested(QString,int)));
        QVERIFY(QMetaObject::invokeMethod(panel, "beginReorder",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop")))));
        QVERIFY(QMetaObject::invokeMethod(panel, "updateReorder",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop"))),
                                          Q_ARG(QVariant, QVariant(140.0)),
                                          Q_ARG(QVariant, QVariant(0.0)),
                                          Q_ARG(QVariant, QVariant(0.0))));
        QQuickItem *dragged = delegate(panel, QStringLiteral("one.desktop"));
        QQuickItem *neighbor = delegate(panel, QStringLiteral("two.desktop"));
        QVERIFY(dragged && neighbor);
        QVERIFY(dragged->property("dragging").toBool());
        QSignalSpy activationSpy(dragged, SIGNAL(activated(QString)));
        QVERIFY(activationSpy.isValid());
        QTRY_VERIFY_WITH_TIMEOUT(propertyReal(neighbor, "visualOffsetX") < 0.0, 1000);
        if (effect == QStringLiteral("magnification"))
            QCOMPARE(propertyReal(dragged, "magnificationScale"), 1.0);

        QVERIFY(QMetaObject::invokeMethod(panel, "finishReorder",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("one.desktop")))));
        QCOMPARE(reorderSpy.count(), 1);
        QCOMPARE(reorderSpy.at(0).at(0).toString(), QStringLiteral("one.desktop"));
        QCOMPARE(reorderSpy.at(0).at(1).toInt(), 2);
        QCOMPARE(activationSpy.count(), 0);
        delete panel;
    }
}

void DockHoverQmlTest::verticalPositionsReusePrimaryAxisGeometry()
{
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop")};
    for (const QString &position : {QStringLiteral("left"), QStringLiteral("right")}) {
        DockController controller;
        DockConfig config = configFor(QStringLiteral("magnification"), pins);
        config.position = position;
        config.indicatorStyle = QStringLiteral("line");
        config.indicatorSize = 5;
        controller.applyConfig(config);

        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
        engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
        QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
        QVERIFY2(component.status() == QQmlComponent::Ready,
                 qPrintable(component.errorString()));
        auto *panel = qobject_cast<QQuickItem *>(component.create());
        QVERIFY2(panel, qPrintable(component.errorString()));
        QQuickWindow window;
        panel->setParentItem(window.contentItem());
        window.resize(180, 900);
        QTest::qWait(40);

        QVERIFY(panel->property("vertical").toBool());
        QCOMPARE(panel->property("restingWidth").toInt(), controller.restingHeight());
        QVERIFY(panel->height() > panel->property("restingHeight").toDouble());
        QQuickItem *first = delegate(panel, pins.at(0));
        QQuickItem *middle = delegate(panel, pins.at(1));
        QQuickItem *last = delegate(panel, pins.at(2));
        QVERIFY(first && middle && last);
        QQuickItem *middleIcon = iconItem(middle);
        QQuickItem *indicator = childWithObjectName(middle, QStringLiteral("runningIndicator"));
        QVERIFY(middleIcon && indicator);

        const qreal initialSurfaceWidth = panel->width();
        const qreal initialSurfaceHeight = panel->height();
        QQuickItem *chrome = childWithObjectName(panel, QStringLiteral("dockChrome"));
        QVERIFY(chrome);
        const qreal restingWidth = panel->property("restingWidth").toReal();
        const qreal restingHeight = panel->property("restingHeight").toReal();
        QCOMPARE(chrome->width(), restingWidth);
        QCOMPARE(chrome->height(), restingHeight);
        const QPointF middlePoint = middle->mapToItem(
            panel, middle->width() / 2.0, middle->height() / 2.0);
        QVERIFY(QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                          Q_ARG(QVariant, QVariant(middlePoint.x())),
                                          Q_ARG(QVariant, QVariant(middlePoint.y()))));
        QTRY_VERIFY_WITH_TIMEOUT(middleIcon->scale() > 1.0, 1500);
        QTRY_VERIFY_WITH_TIMEOUT(panel->property("magnificationExtraPrimary").toReal() > 0.0,
                                 1500);
        QTRY_VERIFY_WITH_TIMEOUT(chrome->height() > restingHeight, 1500);
        QVERIFY2(qAbs(panel->width() - initialSurfaceWidth) < 0.1,
                 "vertical hover must not resize the fixed surface width");
        QVERIFY2(qAbs(panel->height() - initialSurfaceHeight) < 0.1,
                 "vertical hover must not resize the fixed surface height");
        QCOMPARE(chrome->width(), restingWidth);
        QVERIFY(chrome->height() > restingHeight);

        const QRectF iconRect = middleIcon->mapRectToItem(
            panel, QRectF(0, 0, middleIcon->width(), middleIcon->height()));
        const QRectF indicatorRect = indicator->mapRectToItem(
            panel, QRectF(0, 0, indicator->width(), indicator->height()));
        if (position == QStringLiteral("left"))
            QVERIFY(indicatorRect.right() <= iconRect.left() + 0.1);
        else
            QVERIFY(indicatorRect.left() >= iconRect.right() - 0.1);

        const QPointF firstPoint = first->mapToItem(
            panel, first->width() / 2.0, first->height() / 2.0);
        QVERIFY(QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                          Q_ARG(QVariant, QVariant(firstPoint.x())),
                                          Q_ARG(QVariant, QVariant(firstPoint.y()))));
        QTRY_VERIFY_WITH_TIMEOUT(iconItem(first)->scale() > 1.0, 1500);
        QTRY_VERIFY_WITH_TIMEOUT(iconItem(first)->scale() >= iconItem(middle)->scale(), 1500);

        const QPointF lastPoint = last->mapToItem(
            panel, last->width() / 2.0, last->height() / 2.0);
        QVERIFY(QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                          Q_ARG(QVariant, QVariant(lastPoint.x())),
                                          Q_ARG(QVariant, QVariant(lastPoint.y()))));
        QTRY_VERIFY_WITH_TIMEOUT(iconItem(last)->scale() > 1.0, 1500);
        QTRY_VERIFY_WITH_TIMEOUT(iconItem(last)->scale() >= iconItem(middle)->scale(), 1500);

        QVERIFY(QMetaObject::invokeMethod(panel, "setPointerInside",
                                          Q_ARG(QVariant, QVariant(false))));
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(panel->property("magnificationExtraPrimary").toReal())
                                     < 0.1,
                                 1500);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(chrome->height() - restingHeight) < 0.1, 1500);
        QCOMPARE(chrome->width(), restingWidth);
        QVERIFY2(qAbs(panel->width() - initialSurfaceWidth) < 0.1,
                 "vertical hover exit must not resize the fixed surface width");
        QVERIFY2(qAbs(panel->height() - initialSurfaceHeight) < 0.1,
                 "vertical hover exit must not resize the fixed surface height");

        delete panel;
    }
}

void DockHoverQmlTest::verticalContentIsCenteredWithinChrome()
{
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop")};
    qreal leftEdgeDistance = -1;
    for (const QString &position : {QStringLiteral("left"), QStringLiteral("right")}) {
        DockController controller;
        DockConfig config = configFor(QStringLiteral("none"), pins);
        config.position = position;
        controller.applyConfig(config);

        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
        engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
        QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
        QVERIFY2(component.status() == QQmlComponent::Ready,
                 qPrintable(component.errorString()));
        auto *panel = qobject_cast<QQuickItem *>(component.create());
        QVERIFY2(panel, qPrintable(component.errorString()));
        QQuickWindow window;
        panel->setParentItem(window.contentItem());
        QCoreApplication::processEvents();

        QQuickItem *chrome = childWithObjectName(panel, QStringLiteral("dockChrome"));
        QVERIFY(chrome);
        const QRectF chromeRect = chrome->mapRectToItem(
            panel, QRectF(0, 0, chrome->width(), chrome->height()));
        for (const QString &key : pins) {
            QQuickItem *icon = iconItem(delegate(panel, key));
            QVERIFY(icon);
            const QRectF iconRect = icon->mapRectToItem(
                panel, QRectF(0, 0, icon->width(), icon->height()));
            QVERIFY2(qAbs(iconRect.center().x() - chromeRect.center().x()) < 0.1,
                     qPrintable(QStringLiteral("%1 icon center=%2 chrome center=%3")
                                    .arg(position).arg(iconRect.center().x())
                                    .arg(chromeRect.center().x())));
        }

        const QQuickItem *middleIcon = iconItem(delegate(panel, pins.at(1)));
        QVERIFY(middleIcon);
        const QRectF middleRect = middleIcon->mapRectToItem(
            panel, QRectF(0, 0, middleIcon->width(), middleIcon->height()));
        const qreal edgeDistance = position == QStringLiteral("left")
            ? middleRect.center().x() - chromeRect.left()
            : chromeRect.right() - middleRect.center().x();
        if (position == QStringLiteral("left"))
            leftEdgeDistance = edgeDistance;
        else
            QVERIFY2(qAbs(edgeDistance - leftEdgeDistance) < 0.1,
                     qPrintable(QStringLiteral("left distance=%1 right distance=%2")
                                    .arg(leftEdgeDistance).arg(edgeDistance)));
        delete panel;
    }
}

void DockHoverQmlTest::verticalDragReorderUsesPrimaryAndCrossAxes()
{
    const QStringList pins{
        QStringLiteral("one.desktop"), QStringLiteral("two.desktop"),
        QStringLiteral("three.desktop")};
    for (const QString &position : {QStringLiteral("left"), QStringLiteral("right")}) {
        for (const int sourceIndex : {0, 2}) {
            DockController controller;
            DockConfig config = configFor(QStringLiteral("magnification"), pins);
            config.position = position;
            controller.applyConfig(config);

            QQmlEngine engine;
            engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
            engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
            QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
            QVERIFY2(component.status() == QQmlComponent::Ready,
                     qPrintable(component.errorString()));
            auto *panel = qobject_cast<QQuickItem *>(component.create());
            QVERIFY2(panel, qPrintable(component.errorString()));
            FakeInputRegionBridge inputBridge;
            panel->setProperty("inputRegionBridge", QVariant::fromValue(
                                                         static_cast<QObject *>(&inputBridge)));
            QQuickWindow window;
            panel->setParentItem(window.contentItem());
            QCoreApplication::processEvents();

            const QString sourceKey = pins.at(sourceIndex);
            QQuickItem *source = delegate(panel, sourceKey);
            QVERIFY(source);
            QQuickItem *sourceIcon = iconItem(source);
            QVERIFY(sourceIcon);
            const QPointF pointer = source->mapToItem(
                panel, source->width() / 2.0, source->height() / 2.0);
            QVERIFY(QMetaObject::invokeMethod(panel, "updatePointerAtPoint",
                                              Q_ARG(QVariant, QVariant(pointer.x())),
                                              Q_ARG(QVariant, QVariant(pointer.y()))));
            QTRY_VERIFY_WITH_TIMEOUT(sourceIcon->scale() > 1.0, 1000);
            QTest::qWait(160);
            QCoreApplication::processEvents();

            const qreal initialWidth = panel->width();
            const qreal initialHeight = panel->height();
            const QPointF sourceCenter = source->mapToItem(
                panel, source->width() / 2.0, source->height() / 2.0);
            QVERIFY(QMetaObject::invokeMethod(panel, "beginReorder",
                                              Q_ARG(QVariant, QVariant(sourceKey))));
            const qreal expectedOrigin = sourceCenter.y() - panel->height() / 2.0;
            QVERIFY2(qAbs(panel->property("dragOriginCenterRelativeX").toReal()
                          - expectedOrigin) < 0.1,
                     qPrintable(QStringLiteral("%1 source=%2 origin=%3 expected=%4")
                                    .arg(position).arg(sourceKey)
                                    .arg(panel->property("dragOriginCenterRelativeX").toReal())
                                    .arg(expectedOrigin)));
            QTRY_COMPARE_WITH_TIMEOUT(propertyReal(source, "magnificationScale"), 1.0, 1000);
            QCOMPARE(panel->property("dragOriginCenterRelativeX").toReal(), expectedOrigin);

            const qreal direction = sourceIndex == 0 ? 1.0 : -1.0;
            const qreal delta = direction * panel->property("slotPitch").toReal() * 2.0;
            QVERIFY(QMetaObject::invokeMethod(panel, "updateReorder",
                                              Q_ARG(QVariant, QVariant(sourceKey)),
                                              Q_ARG(QVariant, QVariant(delta)),
                                              Q_ARG(QVariant, QVariant(sourceCenter.x())),
                                              Q_ARG(QVariant, QVariant(sourceCenter.y()))));
            QCoreApplication::processEvents();
            const int expectedTarget = sourceIndex == 0 ? 2 : 0;
            QCOMPARE(panel->property("dragTargetIndex").toInt(), expectedTarget);
            QCOMPARE(panel->property("dragOriginCenterRelativeX").toReal(), expectedOrigin);
            QCOMPARE(panel->property("magnificationScales").toList().size(), pins.size());
            QCOMPARE(propertyReal(source, "magnificationScale"), 1.0);

            const qreal expectedCrossLift = position == QStringLiteral("left") ? 8.0 : -8.0;
            QCOMPARE(propertyReal(source, "visualOffsetX"), expectedCrossLift);
            const qreal expectedNeighborOffset = sourceIndex == 0
                ? -panel->property("slotPitch").toReal()
                : panel->property("slotPitch").toReal();
            for (const QString &key : pins) {
                if (key == sourceKey)
                    continue;
                QTRY_COMPARE_WITH_TIMEOUT(propertyReal(delegate(panel, key), "visualOffsetY"),
                                          expectedNeighborOffset, 1000);
            }

            QVERIFY(QMetaObject::invokeMethod(panel, "updateInputRegion"));
            QVERIFY(inputBridge.updateCount > 0);
            QCOMPARE(inputBridge.lastItemCount, pins.size());
            QVERIFY(inputBridge.lastChrome.left() >= -0.1);
            QVERIFY(inputBridge.lastChrome.top() >= -0.1);
            QVERIFY(inputBridge.lastChrome.right() < panel->width() + 0.1);
            QVERIFY(inputBridge.lastChrome.bottom() < panel->height() + 0.1);
            for (const QVariant &value : inputBridge.lastInteractionRects) {
                const QRectF rect = value.toRectF();
                QVERIFY(rect.left() >= -0.1);
                QVERIFY(rect.top() >= -0.1);
                QVERIFY(rect.right() < panel->width() + 0.1);
                QVERIFY(rect.bottom() < panel->height() + 0.1);
            }
            const qreal widthWhileDragging = panel->width();
            const qreal heightWhileDragging = panel->height();
            QCOMPARE(widthWhileDragging, initialWidth);
            QCOMPARE(heightWhileDragging, initialHeight);

            QSignalSpy activationSpy(source, SIGNAL(activated(QString)));
            QSignalSpy reorderSpy(panel, SIGNAL(reorderRequested(QString,int)));
            QVERIFY(activationSpy.isValid() && reorderSpy.isValid());
            QVERIFY(QMetaObject::invokeMethod(panel, "finishReorder",
                                              Q_ARG(QVariant, QVariant(sourceKey))));
            QCOMPARE(reorderSpy.count(), 1);
            QCOMPARE(reorderSpy.at(0).at(0).toString(), sourceKey);
            QCOMPARE(reorderSpy.at(0).at(1).toInt(), expectedTarget);
            QCOMPARE(activationSpy.count(), 0);
            QVERIFY(QMetaObject::invokeMethod(panel, "finishReorder",
                                              Q_ARG(QVariant, QVariant(sourceKey))));
            QCOMPARE(reorderSpy.count(), 1);
            delete panel;
        }

        DockController controller;
        controller.applyConfig(configFor(QStringLiteral("magnification"), pins));
        DockConfig verticalConfig = configFor(QStringLiteral("magnification"), pins);
        verticalConfig.position = position;
        controller.applyConfig(verticalConfig);
        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
        engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
        QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
        QVERIFY2(component.status() == QQmlComponent::Ready,
                 qPrintable(component.errorString()));
        auto *panel = qobject_cast<QQuickItem *>(component.create());
        QVERIFY2(panel, qPrintable(component.errorString()));
        QQuickWindow window;
        panel->setParentItem(window.contentItem());
        QCoreApplication::processEvents();
        QQuickItem *source = delegate(panel, pins.at(1));
        QVERIFY(source);
        QSignalSpy reorderSpy(panel, SIGNAL(reorderRequested(QString,int)));
        QVERIFY(QMetaObject::invokeMethod(panel, "beginReorder",
                                          Q_ARG(QVariant, QVariant(pins.at(1)))));
        QVERIFY(QMetaObject::invokeMethod(panel, "updateReorder",
                                          Q_ARG(QVariant, QVariant(pins.at(1))),
                                          Q_ARG(QVariant, QVariant(panel->property("slotPitch").toReal())),
                                          Q_ARG(QVariant, QVariant(0.0)),
                                          Q_ARG(QVariant, QVariant(0.0))));
        QVERIFY(QMetaObject::invokeMethod(panel, "cancelReorder",
                                          Q_ARG(QVariant, QVariant(pins.at(1)))));
        QCOMPARE(reorderSpy.count(), 0);
        delete panel;
    }
}

void DockHoverQmlTest::floatingAutoHideUsesPhysicalEdgeRevealGeometry()
{
    const QStringList pins{QStringLiteral("one.desktop")};
    for (const QString &position : {QStringLiteral("bottom"), QStringLiteral("left"),
                                    QStringLiteral("right")}) {
        DockController controller;
        DockConfig config = configFor(QStringLiteral("none"), pins);
        config.position = position;
        config.edgeMargin = 12;
        config.floating = true;
        controller.applyConfig(config);

        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(DOCK_BUILD_DIR));
        engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &controller);
        QQmlComponent component(&engine, QUrl::fromLocalFile(kDockPanelPath));
        QVERIFY2(component.status() == QQmlComponent::Ready,
                 qPrintable(component.errorString()));
        auto *panel = qobject_cast<QQuickItem *>(component.create());
        QVERIFY2(panel, qPrintable(component.errorString()));
        QQuickWindow window;
        panel->setParentItem(window.contentItem());
        QCoreApplication::processEvents();

        QQuickItem *chrome = childWithObjectName(panel, QStringLiteral("dockChrome"));
        QQuickItem *reveal = childWithObjectName(panel, QStringLiteral("edgeRevealTarget"));
        QVERIFY(chrome && reveal);
        QCOMPARE(controller.layerShellEdgeMargin(), 12);
        QCOMPARE(controller.chromeEdgeInset(), 0);
        QVERIFY(!controller.physicalEdgeReveal());
        const qreal normalCrossExtent = position == QStringLiteral("bottom")
            ? panel->height() : panel->width();
        QCOMPARE(controller.exclusiveZone(), controller.restingHeight());

        config.autoHide = QStringLiteral("always");
        controller.applyConfig(config);
        QCoreApplication::processEvents();
        QCOMPARE(controller.layerShellEdgeMargin(), 0);
        QCOMPARE(controller.chromeEdgeInset(), 12);
        QVERIFY(controller.physicalEdgeReveal());
        QCOMPARE(controller.exclusiveZone(), 0);
        const qreal hiddenCrossExtent = position == QStringLiteral("bottom")
            ? panel->height() : panel->width();
        QCOMPARE(hiddenCrossExtent, normalCrossExtent + 12);

        if (position == QStringLiteral("bottom")) {
            QCOMPARE(reveal->y() + reveal->height(), panel->height());
            QCOMPARE(panel->height() - (chrome->y() + chrome->height()), 12.0);
        } else if (position == QStringLiteral("left")) {
            QCOMPARE(reveal->x(), 0.0);
            QCOMPARE(chrome->x(), 12.0);
        } else {
            QCOMPARE(reveal->x() + reveal->width(), panel->width());
            QCOMPARE(panel->width() - (chrome->x() + chrome->width()), 12.0);
        }

        controller.setPointerInside(true);
        QCoreApplication::processEvents();
        QVERIFY(controller.revealed());
        QCOMPARE(controller.exclusiveZone(), 0);
        delete panel;
    }
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication application(argc, argv);
    DockHoverQmlTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "DockHoverQmlTest.moc"
