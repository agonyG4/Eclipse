#include "theme/ThemePackCatalog.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class ThemeComponentsTest final : public QObject {
    Q_OBJECT
private slots:
    void resolvesOnlyOptedInVersionOnePackageLocalQml();
    void rejectsTraversalAndSymlinkEscape();
    void rejectsUnsupportedApiVersionAndMissingFiles();
};

namespace {
bool writeFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}
}

void ThemeComponentsTest::resolvesOnlyOptedInVersionOnePackageLocalQml()
{
    QTemporaryDir data;
    QVERIFY(data.isValid());
    qputenv("XDG_DATA_HOME", data.path().toUtf8());
    const QString package = data.filePath(QStringLiteral("AstreaOS/themes/panels"));
    QVERIFY(writeFile(package + QStringLiteral("/theme.json"), R"({
      "schema":1,"id":"panels","name":"Panels",
      "components":{"apiVersion":1,"spotlightPanel":"qml/Spotlight.qml"}
    })"));
    QVERIFY(writeFile(package + QStringLiteral("/qml/Spotlight.qml"), "import QtQuick\nItem {}\n"));

    QVERIFY(Astrea::ThemePacks::component(QStringLiteral("panels"), QStringLiteral("spotlightPanel"), QStringLiteral("0")).isEmpty());
    QVERIFY(Astrea::ThemePacks::component(QStringLiteral("panels"), QStringLiteral("spotlightPanel"), QStringLiteral("1")).isLocalFile());
    QVERIFY(Astrea::ThemePacks::component(QStringLiteral("panels"), QStringLiteral("other"), QStringLiteral("1")).isEmpty());
}

void ThemeComponentsTest::rejectsTraversalAndSymlinkEscape()
{
    QTemporaryDir data;
    QTemporaryDir outside;
    QVERIFY(data.isValid());
    QVERIFY(outside.isValid());
    qputenv("XDG_DATA_HOME", data.path().toUtf8());
    const QString package = data.filePath(QStringLiteral("AstreaOS/themes/panels"));
    QVERIFY(writeFile(outside.filePath(QStringLiteral("Escape.qml")), "import QtQuick\nItem {}\n"));
    QVERIFY(QDir().mkpath(package));
    QVERIFY(QFile::link(outside.filePath(QStringLiteral("Escape.qml")), package + QStringLiteral("/escape.qml")));
    QVERIFY(writeFile(package + QStringLiteral("/theme.json"), R"({
      "schema":1,"id":"panels","components":{"apiVersion":1,"spotlightPanel":"../Escape.qml"}
    })"));
    QVERIFY(Astrea::ThemePacks::component(QStringLiteral("panels"), QStringLiteral("spotlightPanel"), QStringLiteral("1")).isEmpty());

    QVERIFY(writeFile(package + QStringLiteral("/theme.json"), R"({
      "schema":1,"id":"panels","components":{"apiVersion":1,"spotlightPanel":"escape.qml"}
    })"));
    QVERIFY(Astrea::ThemePacks::component(QStringLiteral("panels"), QStringLiteral("spotlightPanel"), QStringLiteral("1")).isEmpty());
}

void ThemeComponentsTest::rejectsUnsupportedApiVersionAndMissingFiles()
{
    QTemporaryDir data;
    QVERIFY(data.isValid());
    qputenv("XDG_DATA_HOME", data.path().toUtf8());
    const QString package = data.filePath(QStringLiteral("AstreaOS/themes/panels"));
    QVERIFY(writeFile(package + QStringLiteral("/theme.json"), R"({
      "schema":1,"id":"panels","components":{"apiVersion":2,"spotlightPanel":"Missing.qml"}
    })"));
    QVERIFY(Astrea::ThemePacks::component(QStringLiteral("panels"), QStringLiteral("spotlightPanel"), QStringLiteral("1")).isEmpty());
    QVERIFY(writeFile(package + QStringLiteral("/theme.json"), R"({
      "schema":1,"id":"panels","components":{"apiVersion":1,"spotlightPanel":"Missing.qml"}
    })"));
    QVERIFY(Astrea::ThemePacks::component(QStringLiteral("panels"), QStringLiteral("spotlightPanel"), QStringLiteral("1")).isEmpty());
}

QTEST_GUILESS_MAIN(ThemeComponentsTest)
#include "ThemeComponentsTest.moc"
