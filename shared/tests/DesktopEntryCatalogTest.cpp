#include <QFile>
#include <QDir>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryDir>
#include <QJsonObject>

#include "apps/DesktopEntryCatalog.hpp"

class DesktopEntryCatalogTest final : public QObject {
    Q_OBJECT

private slots:
    void populatesDesktopIdentity();
    void missingApplicationsDirectoryRecovers();
    void deletedApplicationsDirectoryRecreatesWatches();
    void higherPriorityEntryOverridesLowerPriorityEntry();
    void preservesSpotlightFieldsAndSharedSnapshotIdentity();
    void serializesSpotlightCatalogSnapshot();
};

static void writeDesktopFile(const QString &path, const QString &name)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("[Desktop Entry]\nType=Application\nName=" + name.toUtf8()
               + "\nIcon=test-icon\nExec=test-app\n");
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
    QTest::qWait(400);
    QCOMPARE(updatedSpy.count(), 1);
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

QTEST_MAIN(DesktopEntryCatalogTest)
#include "DesktopEntryCatalogTest.moc"
