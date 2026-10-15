#include <QTest>

#include "apps/DesktopEntryCatalog.hpp"
#include "core/runtime/DockAppMatcher.hpp"

using namespace Astrea::Dock;

namespace {

std::shared_ptr<DesktopEntrySnapshot> makeCatalog()
{
    auto snapshot = std::make_shared<DesktopEntrySnapshot>();
    const auto add = [snapshot](const QString &fileName, const QString &id,
                                const QString &wmClass, bool hidden, bool noDisplay) {
        DesktopEntryRecord entry;
        entry.desktopFileName = fileName;
        entry.id = id;
        entry.name = id;
        entry.icon = id + QStringLiteral("-icon");
        entry.startupWmClass = wmClass;
        entry.hidden = hidden;
        entry.noDisplay = noDisplay;
        snapshot->byDesktopFileName.insert(fileName, snapshot->entries.size());
        snapshot->byDesktopId.insert(id, snapshot->entries.size());
        if (!wmClass.isEmpty())
            snapshot->byStartupWmClass.insert(wmClass, snapshot->entries.size());
        snapshot->entries.append(entry);
    };

    add(QStringLiteral("org.example.App.desktop"), QStringLiteral("org.example.App"), {}, false, false);
    add(QStringLiteral("visible.desktop"), QStringLiteral("Visible"), QStringLiteral("ExampleClass"), false, false);
    add(QStringLiteral("hidden.desktop"), QStringLiteral("Hidden"), QStringLiteral("ExampleClass"), true, false);
    add(QStringLiteral("nodisplay.desktop"), QStringLiteral("NoDisplay"), QStringLiteral("NoDisplayClass"), false, true);
    add(QStringLiteral("a.desktop"), QStringLiteral("Tie"), QStringLiteral("TieClass"), false, false);
    add(QStringLiteral("z.desktop"), QStringLiteral("Tie2"), QStringLiteral("TieClass"), false, false);
    return snapshot;
}

DockAppMatch match(const std::shared_ptr<const DesktopEntrySnapshot> &catalog,
                   const QString &appId, const QString &title = {})
{
    DockAppMatcher matcher(catalog);
    DockRuntimeWindow window;
    window.appId = appId;
    window.title = title;
    window.pid = 1234;
    return matcher.match(window);
}

} // namespace

class DockAppMatcherTest final : public QObject {
    Q_OBJECT

private slots:
    void exactDesktopFileName();
    void exactDesktopId();
    void caseInsensitiveDesktopId();
    void startupWmClassMatchesCaseInsensitively();
    void normalizedReverseDnsIdMatches();
    void visibleCandidateWinsAndTiesUseFileName();
    void titleIsNeverUsed();
    void unresolvedResultIsExplicit();
    void stress100CatalogRevisionRematchCycles();
};

void DockAppMatcherTest::exactDesktopFileName()
{
    const auto result = match(makeCatalog(), QStringLiteral("org.example.App.desktop"));
    QCOMPARE(result.desktopFileName, QStringLiteral("org.example.App.desktop"));
    QCOMPARE(result.confidence, DockAppMatchConfidence::ExactDesktopFileName);
}

void DockAppMatcherTest::exactDesktopId()
{
    const auto result = match(makeCatalog(), QStringLiteral("org.example.App"));
    QCOMPARE(result.desktopFileName, QStringLiteral("org.example.App.desktop"));
    QCOMPARE(result.confidence, DockAppMatchConfidence::ExactDesktopId);
}

void DockAppMatcherTest::caseInsensitiveDesktopId()
{
    const auto result = match(makeCatalog(), QStringLiteral("ORG.EXAMPLE.APP"));
    QCOMPARE(result.desktopFileName, QStringLiteral("org.example.App.desktop"));
    QCOMPARE(result.confidence, DockAppMatchConfidence::CaseInsensitiveDesktopId);
}

void DockAppMatcherTest::startupWmClassMatchesCaseInsensitively()
{
    const auto result = match(makeCatalog(), QStringLiteral("exampleclass"));
    QCOMPARE(result.desktopFileName, QStringLiteral("visible.desktop"));
    QCOMPARE(result.confidence, DockAppMatchConfidence::CaseInsensitiveStartupWmClass);
}

void DockAppMatcherTest::normalizedReverseDnsIdMatches()
{
    auto catalog = makeCatalog();
    DesktopEntryRecord entry;
    entry.desktopFileName = QStringLiteral("org.example_normalized.desktop");
    entry.id = QStringLiteral("org.example-normalized");
    entry.name = QStringLiteral("Normalized");
    catalog->byDesktopFileName.insert(entry.desktopFileName, catalog->entries.size());
    catalog->byDesktopId.insert(entry.id, catalog->entries.size());
    catalog->entries.append(entry);

    const auto result = match(catalog, QStringLiteral("org.example_normalized"));
    QCOMPARE(result.desktopFileName, QStringLiteral("org.example_normalized.desktop"));
    QCOMPARE(result.confidence, DockAppMatchConfidence::NormalizedReverseDnsDesktopId);
}

void DockAppMatcherTest::visibleCandidateWinsAndTiesUseFileName()
{
    auto result = match(makeCatalog(), QStringLiteral("ExampleClass"));
    QCOMPARE(result.desktopFileName, QStringLiteral("visible.desktop"));

    auto catalog = makeCatalog();
    const auto add = [catalog](const QString &fileName) {
        DesktopEntryRecord entry;
        entry.desktopFileName = fileName;
        entry.id = fileName;
        entry.startupWmClass = QStringLiteral("same");
        catalog->byDesktopFileName.insert(fileName, catalog->entries.size());
        catalog->byDesktopId.insert(entry.id, catalog->entries.size());
        catalog->byStartupWmClass.insert(entry.startupWmClass, catalog->entries.size());
        catalog->entries.append(entry);
    };
    add(QStringLiteral("b.desktop"));
    add(QStringLiteral("a.desktop"));
    result = match(catalog, QStringLiteral("same"));
    QCOMPARE(result.desktopFileName, QStringLiteral("a.desktop"));
}

void DockAppMatcherTest::titleIsNeverUsed()
{
    const auto result = match(makeCatalog(), QStringLiteral("unknown"), QStringLiteral("Visible"));
    QVERIFY(result.desktopFileName.isEmpty());
    QCOMPARE(result.confidence, DockAppMatchConfidence::Unresolved);
}

void DockAppMatcherTest::unresolvedResultIsExplicit()
{
    const auto result = match(makeCatalog(), QStringLiteral("not-installed"));
    QVERIFY(result.desktopFileName.isEmpty());
    QVERIFY(result.displayName.isEmpty());
    QCOMPARE(result.confidence, DockAppMatchConfidence::Unresolved);
}

void DockAppMatcherTest::stress100CatalogRevisionRematchCycles()
{
    DockAppMatcher matcher;
    for (quint64 revision = 1; revision <= 100; ++revision) {
        auto snapshot = makeCatalog();
        snapshot->revision = revision;
        matcher.setSnapshot(snapshot);
        DockRuntimeWindow window;
        window.appId = QStringLiteral("org.example.App");
        const DockAppMatch result = matcher.match(window);
        QCOMPARE(result.desktopFileName, QStringLiteral("org.example.App.desktop"));
        window.appId = QStringLiteral("unknown");
        QCOMPARE(matcher.match(window).confidence, DockAppMatchConfidence::Unresolved);
    }
}

QTEST_MAIN(DockAppMatcherTest)
#include "DockAppMatcherTest.moc"
