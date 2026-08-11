#include <QFile>
#include <QDir>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryDir>
#include <QJsonObject>
#include <QJsonDocument>

#include "apps/DesktopEntryCatalog.hpp"

class DesktopEntryCatalogTest final : public QObject {
    Q_OBJECT

private slots:
    void populatesDesktopIdentity();
    void missingApplicationsDirectoryRecovers();
    void deletedApplicationsDirectoryRecreatesWatches();
    void higherPriorityEntryOverridesLowerPriorityEntry();
    void parsesDesktopEntrySemantics();
    void rejectsNonApplicationAndIncompleteEntries();
    void computesNestedDesktopFileIds();
    void discoversEntriesAtMaximumDepth();
    void hiddenHigherPriorityEntryTombstonesLowerPriorityEntry();
    void preservesSpotlightFieldsAndSharedSnapshotIdentity();
    void serializesSpotlightCatalogSnapshot();
    void nestedWatcherTracksCreateChangeAndDelete();
    void nestedDirectoryCreationBecomesIndexed();
    void publishesConsistentSnapshotIndexesPerRevision();
};

static void writeDesktopFile(const QString &path, const QString &name)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("[Desktop Entry]\nType=Application\nName=" + name.toUtf8()
               + "\nIcon=test-icon\nExec=test-app\n");
}

static void writeDesktopText(const QString &path, const QByteArray &text)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QCOMPARE(file.write(text), static_cast<qint64>(text.size()));
}

void DesktopEntryCatalogTest::populatesDesktopIdentity()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString applications = home.path() + QStringLiteral("/.local/share/applications");
    QVERIFY(QDir().mkpath(applications));
    writeDesktopFile(applications + QStringLiteral("/example.desktop"), QStringLiteral("Example"));

    DesktopEntryCatalog catalog;
    catalog.initialize(home.path());

    const auto snapshot = catalog.snapshot();
    const auto record = catalog.findByDesktopFileName(QStringLiteral("example.desktop"));
    QVERIFY(record.has_value());
    QCOMPARE(record->desktopFileName, QStringLiteral("example.desktop"));
    QCOMPARE(record->id, QStringLiteral("example"));
    QCOMPARE(record->sourceFilePath, applications + QStringLiteral("/example.desktop"));
    QCOMPARE(snapshot->byDesktopId.value(QStringLiteral("example")), 0);
}

void DesktopEntryCatalogTest::missingApplicationsDirectoryRecovers()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());

    DesktopEntryCatalog catalog;
    catalog.initialize(home.path());
    QVERIFY(!catalog.findByDesktopFileName(QStringLiteral("created.desktop")).has_value());

    QSignalSpy updatedSpy(&catalog, &DesktopEntryCatalog::indexUpdated);
    const QString applications = home.path() + QStringLiteral("/.local/share/applications");
    QVERIFY(QDir().mkpath(applications));
    QTRY_VERIFY_WITH_TIMEOUT(updatedSpy.count() >= 1, 1500);

    updatedSpy.clear();
    writeDesktopFile(applications + QStringLiteral("/created.desktop"), QStringLiteral("Created"));
    QTRY_VERIFY_WITH_TIMEOUT(catalog.findByDesktopFileName(QStringLiteral("created.desktop")).has_value(), 1500);
    QTRY_COMPARE_WITH_TIMEOUT(updatedSpy.count(), 1, 1500);
}

void DesktopEntryCatalogTest::deletedApplicationsDirectoryRecreatesWatches()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString applications = home.path() + QStringLiteral("/.local/share/applications");
    QVERIFY(QDir().mkpath(applications));
    writeDesktopFile(applications + QStringLiteral("/recreated.desktop"), QStringLiteral("Before"));

    DesktopEntryCatalog catalog;
    catalog.initialize(home.path());
    QVERIFY(catalog.findByDesktopFileName(QStringLiteral("recreated.desktop")).has_value());

    QVERIFY(QDir(applications).removeRecursively());
    QTRY_VERIFY_WITH_TIMEOUT(!catalog.findByDesktopFileName(QStringLiteral("recreated.desktop")).has_value(), 1500);

    QVERIFY(QDir().mkpath(applications));
    writeDesktopFile(applications + QStringLiteral("/recreated.desktop"), QStringLiteral("After"));
    QTRY_VERIFY_WITH_TIMEOUT(catalog.findByDesktopFileName(QStringLiteral("recreated.desktop")).has_value(), 1500);
    const auto record = catalog.findByDesktopFileName(QStringLiteral("recreated.desktop"));
    QVERIFY(record.has_value());
    QCOMPARE(record->name, QStringLiteral("After"));
}

void DesktopEntryCatalogTest::higherPriorityEntryOverridesLowerPriorityEntry()
{
    QTemporaryDir home;
    QTemporaryDir data;
    QVERIFY(home.isValid());
    QVERIFY(data.isValid());

    const QString homeApplications = home.path() + QStringLiteral("/.local/share/applications");
    const QString dataApplications = data.path() + QStringLiteral("/applications");
    QVERIFY(QDir().mkpath(homeApplications));
    QVERIFY(QDir().mkpath(dataApplications));
    writeDesktopFile(dataApplications + QStringLiteral("/same.desktop"), QStringLiteral("Lower"));
    writeDesktopFile(homeApplications + QStringLiteral("/same.desktop"), QStringLiteral("Higher"));

    const QByteArray previous = qgetenv("XDG_DATA_DIRS");
    qputenv("XDG_DATA_DIRS", data.path().toUtf8());

    DesktopEntryCatalog catalog;
    catalog.initialize(home.path());

    if (previous.isNull())
        qunsetenv("XDG_DATA_DIRS");
    else
        qputenv("XDG_DATA_DIRS", previous);

    const auto record = catalog.findByDesktopFileName(QStringLiteral("same.desktop"));
    QVERIFY(record.has_value());
    QCOMPARE(record->name, QStringLiteral("Higher"));
    QCOMPARE(record->sourceFilePath, homeApplications + QStringLiteral("/same.desktop"));
    QVERIFY(catalog.snapshot()->entries.size() >= 1);
}

void DesktopEntryCatalogTest::parsesDesktopEntrySemantics()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString applications = home.path() + QStringLiteral("/.local/share/applications");
    QVERIFY(QDir().mkpath(applications));
    writeDesktopText(applications + QStringLiteral("/semantics.desktop"),
                     "# ignored comment\n"
                     "[Desktop Entry]\n"
                     "Type=Application\n"
                     "Name=Base\\sName\n"
                     "Name[pt]=Nome\n"
                     "Name[pt_BR]=Nome Brasil\n"
                     "GenericName=Base Generic\n"
                     "GenericName[pt_BR]=Genérico Brasil\n"
                     "Comment=Line\\nNext\\tTab\\rReturn\\\\slash\n"
                     "Comment[pt_BR]=Comentário Brasil\n"
                     "Keywords=one;two\\;with-semicolon;three;\n"
                     "Keywords[pt_BR]=quatro;cinco;\n"
                     "Icon=test\n"
                     "Exec=test %U\n"
                     "TryExec=test\n"
                     "StartupWMClass=TestApp\n"
                     "Terminal=true\n"
                     "OnlyShowIn=Astrea;Hyprland;\n"
                     "NotShowIn=GNOME;\n"
                     "NoDisplay=true\n"
                     "[Desktop Action Open]\n"
                     "Name=Ignored Action\n");

    DesktopEntryCatalog catalog;
    catalog.initialize(home.path());

    const auto record = catalog.findByDesktopId(QStringLiteral("semantics"));
    QVERIFY(record.has_value());
    QCOMPARE(record->name, QStringLiteral("Base Name"));
    QCOMPARE(record->localizedNames.value(QStringLiteral("pt")), QStringLiteral("Nome"));
    QCOMPARE(record->localizedNames.value(QStringLiteral("pt_BR")), QStringLiteral("Nome Brasil"));
    QCOMPARE(record->genericName, QStringLiteral("Base Generic"));
    QCOMPARE(record->localizedGenericNames.value(QStringLiteral("pt_BR")),
             QStringLiteral("Genérico Brasil"));
    QCOMPARE(record->comment, QStringLiteral("Line\nNext\tTab\rReturn\\slash"));
    QCOMPARE(record->localizedComments.value(QStringLiteral("pt_BR")),
             QStringLiteral("Comentário Brasil"));
    QCOMPARE(record->keywords,
             QStringList({QStringLiteral("one"), QStringLiteral("two;with-semicolon"),
                          QStringLiteral("three")}));
    QCOMPARE(record->localizedKeywords.value(QStringLiteral("pt_BR")),
             QStringList({QStringLiteral("quatro"), QStringLiteral("cinco")}));
    QCOMPARE(record->icon, QStringLiteral("test"));
    QCOMPARE(record->exec, QStringLiteral("test %U"));
    QCOMPARE(record->tryExec, QStringLiteral("test"));
    QCOMPARE(record->startupWmClass, QStringLiteral("TestApp"));
    QVERIFY(record->terminal);
    QVERIFY(record->noDisplay);
    QCOMPARE(record->onlyShowIn, QStringList({QStringLiteral("Astrea"), QStringLiteral("Hyprland")}));
    QCOMPARE(record->notShowIn, QStringList({QStringLiteral("GNOME")}));
}

void DesktopEntryCatalogTest::rejectsNonApplicationAndIncompleteEntries()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString applications = home.path() + QStringLiteral("/.local/share/applications");
    QVERIFY(QDir().mkpath(applications));
    writeDesktopText(applications + QStringLiteral("/link.desktop"),
                     "[Desktop Entry]\nType=Link\nName=Link\n");
    writeDesktopText(applications + QStringLiteral("/directory.desktop"),
                     "[Desktop Entry]\nType=Directory\nName=Directory\n");
    writeDesktopText(applications + QStringLiteral("/unknown.desktop"),
                     "[Desktop Entry]\nType=SomethingUnknown\nName=Unknown\n");
    writeDesktopText(applications + QStringLiteral("/missing-type.desktop"),
                     "[Desktop Entry]\nName=Missing Type\n");
    writeDesktopText(applications + QStringLiteral("/missing-name.desktop"),
                     "[Desktop Entry]\nType=Application\nExec=missing-name\n");

    DesktopEntryCatalog catalog;
    catalog.initialize(home.path());

    for (const QString &id : {QStringLiteral("link"), QStringLiteral("directory"),
                              QStringLiteral("unknown"), QStringLiteral("missing-type"),
                              QStringLiteral("missing-name")}) {
        QVERIFY2(!catalog.findByDesktopId(id).has_value(), qPrintable(id));
    }
}

void DesktopEntryCatalogTest::computesNestedDesktopFileIds()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString nested = home.path()
        + QStringLiteral("/.local/share/applications/vendor/tools");
    QVERIFY(QDir().mkpath(nested));
    const QString sourcePath = nested + QStringLiteral("/example.desktop");
    writeDesktopFile(sourcePath, QStringLiteral("Nested Example"));

    DesktopEntryCatalog catalog;
    catalog.initialize(home.path());

    const auto record = catalog.findByDesktopId(QStringLiteral("vendor-tools-example"));
    QVERIFY(record.has_value());
    QCOMPARE(record->desktopFileName, QStringLiteral("vendor-tools-example.desktop"));
    QCOMPARE(record->id, QStringLiteral("vendor-tools-example"));
    QCOMPARE(record->sourceFilePath, sourcePath);
    QVERIFY(!catalog.findByDesktopFileName(QStringLiteral("example.desktop")).has_value());
}

void DesktopEntryCatalogTest::discoversEntriesAtMaximumDepth()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    QString nested = home.path() + QStringLiteral("/.local/share/applications");
    const QStringList segments{QStringLiteral("one"), QStringLiteral("two"),
                               QStringLiteral("three"), QStringLiteral("four"),
                               QStringLiteral("five")};
    for (const QString &segment : segments)
        nested += QStringLiteral("/") + segment;
    QVERIFY(QDir().mkpath(nested));
    const QString sourcePath = nested + QStringLiteral("/depth.desktop");
    writeDesktopFile(sourcePath, QStringLiteral("Depth Five"));

    DesktopEntryCatalog catalog;
    catalog.initialize(home.path());

    const auto record = catalog.findByDesktopId(QStringLiteral("one-two-three-four-five-depth"));
    QVERIFY(record.has_value());
    QCOMPARE(record->sourceFilePath, sourcePath);
}

void DesktopEntryCatalogTest::hiddenHigherPriorityEntryTombstonesLowerPriorityEntry()
{
    QTemporaryDir home;
    QTemporaryDir data;
    QVERIFY(home.isValid());
    QVERIFY(data.isValid());
    const QString homeApplications = home.path() + QStringLiteral("/.local/share/applications");
    const QString dataApplications = data.path() + QStringLiteral("/applications");
    QVERIFY(QDir().mkpath(homeApplications));
    QVERIFY(QDir().mkpath(dataApplications));
    writeDesktopFile(dataApplications + QStringLiteral("/same.desktop"), QStringLiteral("Lower"));
    writeDesktopText(homeApplications + QStringLiteral("/same.desktop"),
                     "[Desktop Entry]\nType=Application\nName=Hidden\nHidden=true\n");

    const QByteArray previous = qgetenv("XDG_DATA_DIRS");
    qputenv("XDG_DATA_DIRS", data.path().toUtf8());
    DesktopEntryCatalog catalog;
    catalog.initialize(home.path());
    if (previous.isNull())
        qunsetenv("XDG_DATA_DIRS");
    else
        qputenv("XDG_DATA_DIRS", previous);

    QVERIFY(!catalog.findByDesktopId(QStringLiteral("same")).has_value());
    QVERIFY(!catalog.findByDesktopFileName(QStringLiteral("same.desktop")).has_value());
}

void DesktopEntryCatalogTest::preservesSpotlightFieldsAndSharedSnapshotIdentity()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString applications = home.path() + QStringLiteral("/.local/share/applications");
    QVERIFY(QDir().mkpath(applications));

    QFile file(applications + QStringLiteral("/rich.desktop"));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("[Desktop Entry]\nType=Application\nName=Rich App\n"
               "Name[en_US]=Rich App US\nGenericName=Launcher\n"
               "GenericName[en_US]=Application Launcher\nComment=Find things\n"
               "Comment[en_US]=Find applications\nIcon=rich-icon\nExec=rich-app %U\n"
               "TryExec=rich-app\nStartupWMClass=RichApp\nTerminal=true\n"
               "Keywords=one;two;\nCategories=Utility;Development;\n"
               "OnlyShowIn=Hyprland;\nNotShowIn=GNOME;\n");
    file.close();

    DesktopEntryCatalog catalog;
    catalog.initialize(home.path());

    const auto firstSnapshot = catalog.snapshot();
    const auto secondSnapshot = catalog.snapshot();
    QVERIFY(firstSnapshot == secondSnapshot);
    const auto record = catalog.findByDesktopFileName(QStringLiteral("rich.desktop"));
    QVERIFY(record.has_value());
    QCOMPARE(record->genericName, QStringLiteral("Launcher"));
    QCOMPARE(record->comment, QStringLiteral("Find things"));
    QCOMPARE(record->localizedNames.value(QStringLiteral("en_US")), QStringLiteral("Rich App US"));
    QCOMPARE(record->localizedGenericNames.value(QStringLiteral("en_US")),
             QStringLiteral("Application Launcher"));
    QCOMPARE(record->localizedComments.value(QStringLiteral("en_US")),
             QStringLiteral("Find applications"));
    QCOMPARE(record->keywords, QStringList({QStringLiteral("one"), QStringLiteral("two")}));
    QCOMPARE(record->categories,
             QStringList({QStringLiteral("Utility"), QStringLiteral("Development")}));
    QCOMPARE(record->onlyShowIn, QStringList({QStringLiteral("Hyprland")}));
    QCOMPARE(record->notShowIn, QStringList({QStringLiteral("GNOME")}));
    QVERIFY(record->terminal);
}

void DesktopEntryCatalogTest::serializesSpotlightCatalogSnapshot()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString applications = home.path() + QStringLiteral("/.local/share/applications");
    QVERIFY(QDir().mkpath(applications));
    writeDesktopFile(applications + QStringLiteral("/serialized.desktop"), QStringLiteral("Serialized App"));

    DesktopEntryCatalog catalog;
    catalog.initialize(home.path());
    const auto json = catalog.snapshotJson();
    QJsonObject object;
    for (const auto &value : json) {
        const auto candidate = value.toObject();
        if (candidate.value(QStringLiteral("id")).toString() == QStringLiteral("serialized")) {
            object = candidate;
            break;
        }
    }
    QVERIFY(!object.isEmpty());
    QCOMPARE(object.value(QStringLiteral("id")).toString(), QStringLiteral("serialized"));
    QCOMPARE(object.value(QStringLiteral("name")).toString(), QStringLiteral("Serialized App"));
    QCOMPARE(object.value(QStringLiteral("desktop_file_name")).toString(),
             QStringLiteral("serialized.desktop"));
    QCOMPARE(object.value(QStringLiteral("keywords")).toArray().size(), 0);
    QVERIFY(!catalog.watchedDirectories().isEmpty());
}

void DesktopEntryCatalogTest::nestedWatcherTracksCreateChangeAndDelete()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString nested = home.path()
        + QStringLiteral("/.local/share/applications/vendor/tools");
    QVERIFY(QDir().mkpath(nested));
    const QString path = nested + QStringLiteral("/watched.desktop");

    DesktopEntryCatalog catalog;
    catalog.initialize(home.path());
    QSignalSpy updatedSpy(&catalog, &DesktopEntryCatalog::indexUpdated);

    updatedSpy.clear();
    writeDesktopFile(path, QStringLiteral("Before"));
    QTRY_VERIFY_WITH_TIMEOUT(catalog.findByDesktopId(QStringLiteral("vendor-tools-watched"))
                                 .has_value(),
                             1500);
    QTRY_COMPARE_WITH_TIMEOUT(updatedSpy.count(), 1, 1500);
    const auto before = catalog.findByDesktopId(QStringLiteral("vendor-tools-watched"));
    QVERIFY(before.has_value());
    QCOMPARE(before->name, QStringLiteral("Before"));

    updatedSpy.clear();
    writeDesktopFile(path, QStringLiteral("Changed"));
    QTRY_VERIFY_WITH_TIMEOUT(
        [&catalog] {
            const auto current = catalog.findByDesktopId(QStringLiteral("vendor-tools-watched"));
            return current.has_value() && current->name == QStringLiteral("Changed");
        }(),
        1500);
    QTRY_COMPARE_WITH_TIMEOUT(updatedSpy.count(), 1, 1500);

    updatedSpy.clear();
    QVERIFY(QFile::remove(path));
    QTRY_VERIFY_WITH_TIMEOUT(!catalog.findByDesktopId(QStringLiteral("vendor-tools-watched"))
                                  .has_value(),
                             1500);
    QTRY_COMPARE_WITH_TIMEOUT(updatedSpy.count(), 1, 1500);
}

void DesktopEntryCatalogTest::nestedDirectoryCreationBecomesIndexed()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString applications = home.path() + QStringLiteral("/.local/share/applications");
    QVERIFY(QDir().mkpath(applications));

    DesktopEntryCatalog catalog;
    catalog.initialize(home.path());
    QSignalSpy updatedSpy(&catalog, &DesktopEntryCatalog::indexUpdated);

    const QString nested = applications + QStringLiteral("/new/vendor");
    QVERIFY(QDir().mkpath(nested));
    const QString path = nested + QStringLiteral("/created.desktop");
    writeDesktopFile(path, QStringLiteral("Created Nested"));

    QTRY_VERIFY_WITH_TIMEOUT(catalog.findByDesktopId(QStringLiteral("new-vendor-created"))
                                 .has_value(),
                             1500);
    QVERIFY(updatedSpy.count() >= 1);
}

void DesktopEntryCatalogTest::publishesConsistentSnapshotIndexesPerRevision()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString applications = home.path() + QStringLiteral("/.local/share/applications");
    QVERIFY(QDir().mkpath(applications));
    const QString path = applications + QStringLiteral("/indexed.desktop");
    writeDesktopFile(path, QStringLiteral("Before"));

    DesktopEntryCatalog catalog;
    catalog.initialize(home.path());
    const auto initial = catalog.snapshot();
    const auto stable = catalog.snapshot();
    QVERIFY(initial == stable);
    const quint64 initialRevision = initial->revision;

    for (auto it = initial->byDesktopId.constBegin(); it != initial->byDesktopId.constEnd(); ++it) {
        QVERIFY(it.value() >= 0 && it.value() < initial->entries.size());
        QCOMPARE(initial->entries.at(it.value()).id, it.key());
    }
    for (auto it = initial->byDesktopFileName.constBegin();
         it != initial->byDesktopFileName.constEnd(); ++it) {
        QVERIFY(it.value() >= 0 && it.value() < initial->entries.size());
        QCOMPARE(initial->entries.at(it.value()).desktopFileName, it.key());
    }
    for (auto it = initial->byStartupWmClass.constBegin();
         it != initial->byStartupWmClass.constEnd(); ++it) {
        QVERIFY(it.value() >= 0 && it.value() < initial->entries.size());
        QCOMPARE(initial->entries.at(it.value()).startupWmClass, it.key());
    }

    QSignalSpy updatedSpy(&catalog, &DesktopEntryCatalog::indexUpdated);
    updatedSpy.clear();
    writeDesktopFile(path, QStringLiteral("After"));
    QTRY_VERIFY_WITH_TIMEOUT(
        [&catalog] {
            const auto current = catalog.findByDesktopId(QStringLiteral("indexed"));
            return current.has_value() && current->name == QStringLiteral("After");
        }(),
        1500);
    const auto replacement = catalog.snapshot();
    QVERIFY(replacement != initial);
    QCOMPARE(replacement->revision, initialRevision + 1);
    QCOMPARE(updatedSpy.count(), 1);
    for (auto it = replacement->byDesktopId.constBegin();
         it != replacement->byDesktopId.constEnd(); ++it) {
        QVERIFY(it.value() >= 0 && it.value() < replacement->entries.size());
        QCOMPARE(replacement->entries.at(it.value()).id, it.key());
    }
}

QTEST_MAIN(DesktopEntryCatalogTest)
#include "DesktopEntryCatalogTest.moc"
