#include <QTest>

#include "app/CommandLine.hpp"

class DockCommandLineTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesDaemonAndCommands();
    void defaultsToDaemon();
};

void DockCommandLineTest::parsesDaemonAndCommands()
{
    const CommandLineRequest request = CommandLine::parse({QStringLiteral("--show")});
    QCOMPARE(request.mode, CommandLineRequest::Mode::Show);
    QCOMPARE(CommandLine::parse({QStringLiteral("--hide")}).mode, CommandLineRequest::Mode::Hide);
    QCOMPARE(CommandLine::parse({QStringLiteral("--reload")}).mode, CommandLineRequest::Mode::Reload);
    QCOMPARE(CommandLine::parse({QStringLiteral("--quit")}).mode, CommandLineRequest::Mode::Quit);
    QCOMPARE(CommandLine::parse({QStringLiteral("--status")}).mode, CommandLineRequest::Mode::Status);
}

void DockCommandLineTest::defaultsToDaemon()
{
    const CommandLineRequest request = CommandLine::parse({QStringLiteral("--daemon")});
    QCOMPARE(request.mode, CommandLineRequest::Mode::Daemon);
    QVERIFY(request.daemonMode);
}

QTEST_MAIN(DockCommandLineTest)
#include "CommandLineTest.moc"
