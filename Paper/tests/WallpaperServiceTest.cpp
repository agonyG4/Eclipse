#include "core/WallpaperPersistence.hpp"
#include "core/WallpaperService.hpp"

#include <QImage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace Paper;

class WallpaperServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void startupWithoutOverrideUsesFactoryDefault();
    void missingFactoryAssetUsesEmergencyFallback();
    void missingConfiguredSourceFallsBackWithoutErasingConfiguration();
    void setAndResetHaveTransactionalSemantics();
    void restartRestoresPersistedConfiguredWallpaper();
    void validSetEmitsOneSuccessfulOperationResult();
    void invalidSetEmitsRejectedResultWithPreviousSnapshot();
    void persistenceFailureEmitsFailureWithoutPublishing();
    void newerSetSupersedesActiveAndPendingOperations();
    void resetCancelsPendingSetAndCompletesItself();
    void rapidSetUsesLatestRequestWinsWithBoundedWork();
    void resetInvalidatesInFlightRequest();
    void sourceDisappearanceAndRecoveryReconcileWithoutLosingConfiguration();
    void sourceAtomicReplacementAndCorruptionReconcile();
    void symlinkRetargetReconcilesConfiguredEntryWithBoundedWatches();
    void catalogImportAndStableSelectionSurviveRestart();
    void catalogOnlyAddPreservesActiveWallpaper();
    void pathConvenienceImportsBeforeSelecting();
    void migratesLegacySymlinkIntoManagedCatalog();
    void missingCatalogSelectionFallsBackWithoutErasingIntent();
    void committedWallpaperChangeEmitsOnce();

private:
    static QString writeImage(const QString &path);
    static std::unique_ptr<XdgWallpaperPersistence> persistence(const QString &path);
};

class FailingWallpaperPersistence final : public WallpaperPersistence
{
public:
    std::optional<WallpaperDescriptor> load(QString *) const override { return std::nullopt; }
    bool save(const WallpaperDescriptor &, QString *errorMessage) override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("test persistence failure");
        }
        return false;
    }
    bool clear(QString *) override { return true; }
    QString location() const override { return QStringLiteral("test://failing"); }
};

QString WallpaperServiceTest::writeImage(const QString &path)
{
    QImage image(20, 20, QImage::Format_ARGB32);
    image.fill(Qt::darkGreen);
    if (!image.save(path)) {
        qFatal("Could not create test image at %s", qPrintable(path));
    }
    return path;
}

std::unique_ptr<XdgWallpaperPersistence> WallpaperServiceTest::persistence(const QString &path)
{
    return std::make_unique<XdgWallpaperPersistence>(
        path, path + QStringLiteral(".legacy-wallpaper"));
}

void WallpaperServiceTest::startupWithoutOverrideUsesFactoryDefault()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             persistence(temp.filePath(QStringLiteral("paper.ini"))));

    service.initialize();

    QVERIFY(!service.snapshot().configured.has_value());
    QCOMPARE(service.snapshot().effective.logicalId(),
             QStringLiteral("astrea://wallpaper/default"));
    QCOMPARE(service.snapshot().state, WallpaperState::Ready);
    QCOMPARE(service.snapshot().generation, quint64(1));
}

void WallpaperServiceTest::missingFactoryAssetUsesEmergencyFallback()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    WallpaperService service(
        WallpaperResolver(temp.filePath(QStringLiteral("missing-factory.png")), emergency),
        persistence(temp.filePath(QStringLiteral("paper.ini"))));

    service.initialize();

    QCOMPARE(service.snapshot().effective.logicalId(),
             QStringLiteral("astrea://wallpaper/emergency"));
    QCOMPARE(service.snapshot().state, WallpaperState::Fallback);
    QCOMPARE(service.snapshot().fallback, WallpaperFallback::EmergencyFallback);
}

void WallpaperServiceTest::missingConfiguredSourceFallsBackWithoutErasingConfiguration()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    auto saved = persistence(temp.filePath(QStringLiteral("paper.ini")));
    const auto missing = WallpaperDescriptor::externalFile(
        temp.filePath(QStringLiteral("gone.png")), WallpaperFit::Cover);
    QString error;
    QVERIFY(saved->save(missing, &error));

    WallpaperService service(WallpaperResolver(factory, emergency),
                             persistence(temp.filePath(QStringLiteral("paper.ini"))));
    service.initialize();

    QVERIFY(service.snapshot().configured.has_value());
    QCOMPARE(service.snapshot().configured->source(), missing.source());
    QCOMPARE(service.snapshot().effective.logicalId(),
             QStringLiteral("astrea://wallpaper/default"));
    QTRY_COMPARE_WITH_TIMEOUT(service.snapshot().state, WallpaperState::Fallback, 1000);
    QCOMPARE(service.snapshot().fallback, WallpaperFallback::SourceMissing);
    QCOMPARE(service.snapshot().generation, quint64(1));
    QVERIFY(!service.snapshot().lastError.isEmpty());
}

void WallpaperServiceTest::setAndResetHaveTransactionalSemantics()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto selectedPath = writeImage(temp.filePath(QStringLiteral("selected.png")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             persistence(temp.filePath(QStringLiteral("paper.ini"))));
    service.initialize();
    const auto initialGeneration = service.snapshot().generation;

    const auto selected = WallpaperDescriptor::externalFile(selectedPath, WallpaperFit::Contain);
    QSignalSpy effectiveSpy(&service, &WallpaperService::effectiveWallpaperChanged);
    service.setWallpaper(selected);
    QTRY_COMPARE_WITH_TIMEOUT(service.snapshot().effective.source(), selected.source(), 1000);
    QVERIFY(service.snapshot().configured.has_value());
    QCOMPARE(service.snapshot().configured->fit(), WallpaperFit::Contain);
    QVERIFY(service.snapshot().generation > initialGeneration);
    QVERIFY(effectiveSpy.count() >= 1);

    const auto beforeFailedSet = service.snapshot();
    const auto missing = WallpaperDescriptor::externalFile(
        temp.filePath(QStringLiteral("missing.png")), WallpaperFit::Cover);
    service.setWallpaper(missing);
    QTRY_VERIFY_WITH_TIMEOUT(!service.snapshot().lastError.isEmpty(), 1000);
    QCOMPARE(service.snapshot().effective.source(), beforeFailedSet.effective.source());
    QCOMPARE(service.snapshot().configured->source(), beforeFailedSet.configured->source());

    service.reset();
    QVERIFY(!service.snapshot().configured.has_value());
    QCOMPARE(service.snapshot().effective.logicalId(),
             QStringLiteral("astrea://wallpaper/default"));
    QVERIFY(service.snapshot().generation > initialGeneration);
}

void WallpaperServiceTest::restartRestoresPersistedConfiguredWallpaper()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto selectedPath = writeImage(temp.filePath(QStringLiteral("selected.png")));
    const auto configPath = temp.filePath(QStringLiteral("paper.ini"));
    const auto selected = WallpaperDescriptor::externalFile(selectedPath, WallpaperFit::Contain);

    {
        WallpaperService first(WallpaperResolver(factory, emergency), persistence(configPath));
        first.initialize();
        first.setWallpaper(selected);
        QTRY_COMPARE_WITH_TIMEOUT(first.snapshot().effective.source(), selectedPath, 1000);
    }

    WallpaperService second(WallpaperResolver(factory, emergency), persistence(configPath));
    second.initialize();
    QTRY_COMPARE_WITH_TIMEOUT(second.snapshot().effective.source(), selectedPath, 1000);
    QVERIFY(second.snapshot().configured.has_value());
    QCOMPARE(second.snapshot().configured->fit(), WallpaperFit::Contain);
    QCOMPARE(second.snapshot().state, WallpaperState::Ready);
}

void WallpaperServiceTest::validSetEmitsOneSuccessfulOperationResult()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto selected = writeImage(temp.filePath(QStringLiteral("selected.png")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             persistence(temp.filePath(QStringLiteral("paper.ini"))));
    service.initialize();
    QSignalSpy finished(&service, &WallpaperService::wallpaperOperationFinished);

    const auto operationId = service.setWallpaper(
        WallpaperDescriptor::externalFile(selected, WallpaperFit::Contain));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1000);

    const auto result = qvariant_cast<WallpaperOperationResult>(finished.at(0).at(1));
    QCOMPARE(result.id, operationId);
    QCOMPARE(result.status, WallpaperOperationStatus::Succeeded);
    QVERIFY(result.succeeded());
    QCOMPARE(result.snapshot.effective.source(), selected);
    QCOMPARE(result.snapshot.state, WallpaperState::Ready);
}

void WallpaperServiceTest::invalidSetEmitsRejectedResultWithPreviousSnapshot()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto selected = writeImage(temp.filePath(QStringLiteral("selected.png")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             persistence(temp.filePath(QStringLiteral("paper.ini"))));
    service.initialize();
    service.setWallpaper(WallpaperDescriptor::externalFile(selected, WallpaperFit::Cover));
    QTRY_COMPARE_WITH_TIMEOUT(service.snapshot().effective.source(), selected, 1000);
    const auto before = service.snapshot();
    const auto generation = service.snapshot().generation;
    QSignalSpy finished(&service, &WallpaperService::wallpaperOperationFinished);

    const auto operationId = service.setWallpaper(WallpaperDescriptor::externalFile(
        temp.filePath(QStringLiteral("missing.png")), WallpaperFit::Cover));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1000);

    const auto result = qvariant_cast<WallpaperOperationResult>(finished.at(0).at(1));
    QCOMPARE(result.id, operationId);
    QCOMPARE(result.status, WallpaperOperationStatus::Rejected);
    QCOMPARE(result.errorCode, QStringLiteral("source-missing"));
    QCOMPARE(result.snapshot.effective.source(), before.effective.source());
    QCOMPARE(result.snapshot.configured->source(), before.configured->source());
    QCOMPARE(result.snapshot.generation, generation);
}

void WallpaperServiceTest::persistenceFailureEmitsFailureWithoutPublishing()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto selected = writeImage(temp.filePath(QStringLiteral("selected.png")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             std::make_unique<FailingWallpaperPersistence>());
    service.initialize();
    const auto before = service.snapshot();
    QSignalSpy finished(&service, &WallpaperService::wallpaperOperationFinished);

    const auto operationId = service.setWallpaper(
        WallpaperDescriptor::externalFile(selected, WallpaperFit::Cover));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1000);

    const auto result = qvariant_cast<WallpaperOperationResult>(finished.at(0).at(1));
    QCOMPARE(result.id, operationId);
    QCOMPARE(result.status, WallpaperOperationStatus::PersistenceFailed);
    QCOMPARE(result.errorCode, QStringLiteral("persistence-write-failed"));
    QCOMPARE(result.snapshot.effective, before.effective);
    QCOMPARE(result.snapshot.generation, before.generation);
    QVERIFY(!result.snapshot.configured.has_value());
}

void WallpaperServiceTest::newerSetSupersedesActiveAndPendingOperations()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto selected = writeImage(temp.filePath(QStringLiteral("selected.png")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             persistence(temp.filePath(QStringLiteral("paper.ini"))));
    service.initialize();
    QSignalSpy finished(&service, &WallpaperService::wallpaperOperationFinished);

    const auto first = service.setWallpaper(WallpaperDescriptor::externalFile(
        temp.filePath(QStringLiteral("first-missing.png")), WallpaperFit::Cover));
    const auto second = service.setWallpaper(WallpaperDescriptor::externalFile(
        temp.filePath(QStringLiteral("second-missing.png")), WallpaperFit::Cover));
    const auto third = service.setWallpaper(
        WallpaperDescriptor::externalFile(selected, WallpaperFit::Center));

    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 3, 1000);
    QCOMPARE(service.snapshot().configured->source(), selected);
    QCOMPARE(service.snapshot().effective.source(), selected);
    QCOMPARE(service.snapshot().configured->fit(), WallpaperFit::Center);

    QHash<WallpaperOperationId, WallpaperOperationStatus> statuses;
    for (const auto &entry : finished) {
        const auto result = qvariant_cast<WallpaperOperationResult>(entry.at(1));
        statuses.insert(result.id, result.status);
    }
    QCOMPARE(statuses.value(first), WallpaperOperationStatus::Superseded);
    QCOMPARE(statuses.value(second), WallpaperOperationStatus::Superseded);
    QCOMPARE(statuses.value(third), WallpaperOperationStatus::Succeeded);
}

void WallpaperServiceTest::resetCancelsPendingSetAndCompletesItself()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto selected = writeImage(temp.filePath(QStringLiteral("selected.png")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             persistence(temp.filePath(QStringLiteral("paper.ini"))));
    service.initialize();
    QSignalSpy finished(&service, &WallpaperService::wallpaperOperationFinished);

    const auto setId = service.setWallpaper(
        WallpaperDescriptor::externalFile(selected, WallpaperFit::Cover));
    const auto resetId = service.resetWallpaper();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 1000);

    QHash<WallpaperOperationId, WallpaperOperationStatus> statuses;
    for (const auto &entry : finished) {
        const auto result = qvariant_cast<WallpaperOperationResult>(entry.at(1));
        statuses.insert(result.id, result.status);
    }
    QCOMPARE(statuses.value(setId), WallpaperOperationStatus::CancelledByReset);
    QCOMPARE(statuses.value(resetId), WallpaperOperationStatus::Succeeded);
    QVERIFY(!service.snapshot().configured.has_value());
    QCOMPARE(service.snapshot().effective.logicalId(),
             QStringLiteral("astrea://wallpaper/default"));
}

void WallpaperServiceTest::rapidSetUsesLatestRequestWinsWithBoundedWork()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto a = writeImage(temp.filePath(QStringLiteral("a.png")));
    const auto b = writeImage(temp.filePath(QStringLiteral("b.png")));
    const auto c = writeImage(temp.filePath(QStringLiteral("c.png")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             persistence(temp.filePath(QStringLiteral("paper.ini"))));
    service.initialize();

    service.setWallpaper(WallpaperDescriptor::externalFile(a, WallpaperFit::Cover));
    service.setWallpaper(WallpaperDescriptor::externalFile(b, WallpaperFit::Contain));
    service.setWallpaper(WallpaperDescriptor::externalFile(c, WallpaperFit::Center));

    QVERIFY(service.validationWorkCountForTests() <= 2);
    QTRY_COMPARE_WITH_TIMEOUT(service.snapshot().effective.source(), c, 1000);
    QCOMPARE(service.snapshot().configured->source(), c);
    QCOMPARE(service.snapshot().configured->fit(), WallpaperFit::Center);
    QCOMPARE(service.validationWorkCountForTests(), 0);
}

void WallpaperServiceTest::resetInvalidatesInFlightRequest()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto selected = writeImage(temp.filePath(QStringLiteral("selected.png")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             persistence(temp.filePath(QStringLiteral("paper.ini"))));
    service.initialize();

    service.setWallpaper(WallpaperDescriptor::externalFile(selected, WallpaperFit::Cover));
    service.reset();
    QTRY_VERIFY_WITH_TIMEOUT(!service.snapshot().configured.has_value(), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(service.snapshot().effective.logicalId(),
                              QStringLiteral("astrea://wallpaper/default"), 1000);
    QCOMPARE(service.snapshot().configured.has_value(), false);
}

void WallpaperServiceTest::sourceDisappearanceAndRecoveryReconcileWithoutLosingConfiguration()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto selected = temp.filePath(QStringLiteral("selected.png"));
    writeImage(selected);
    WallpaperService service(WallpaperResolver(factory, emergency),
                             persistence(temp.filePath(QStringLiteral("paper.ini"))));
    service.initialize();
    service.setWallpaper(WallpaperDescriptor::externalFile(selected, WallpaperFit::Cover));
    QTRY_COMPARE_WITH_TIMEOUT(service.snapshot().state, WallpaperState::Ready, 1000);
    const auto generation = service.snapshot().generation;

    QVERIFY(QFile::remove(selected));
    QTRY_COMPARE_WITH_TIMEOUT(service.snapshot().state, WallpaperState::Fallback, 1500);
    QVERIFY(service.snapshot().configured.has_value());
    QCOMPARE(service.snapshot().configured->source(), selected);

    writeImage(selected);
    QTRY_COMPARE_WITH_TIMEOUT(service.snapshot().state, WallpaperState::Ready, 1500);
    QVERIFY(service.snapshot().configured.has_value());
    QCOMPARE(service.snapshot().configured->source(), selected);
    QVERIFY(service.snapshot().generation > generation);
}

void WallpaperServiceTest::sourceAtomicReplacementAndCorruptionReconcile()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto selected = temp.filePath(QStringLiteral("selected.png"));
    writeImage(selected);
    WallpaperService service(WallpaperResolver(factory, emergency),
                             persistence(temp.filePath(QStringLiteral("paper.ini"))));
    service.initialize();
    service.setWallpaper(WallpaperDescriptor::externalFile(selected, WallpaperFit::Cover));
    QTRY_COMPARE_WITH_TIMEOUT(service.snapshot().state, WallpaperState::Ready, 1000);

    const auto replacement = writeImage(temp.filePath(QStringLiteral("replacement.png")));
    QVERIFY(QFile::remove(selected));
    QVERIFY(QFile::rename(replacement, selected));
    QTRY_COMPARE_WITH_TIMEOUT(service.snapshot().state, WallpaperState::Ready, 1500);
    QCOMPARE(service.snapshot().configured->source(), selected);

    const auto corrupt = temp.filePath(QStringLiteral("corrupt.png"));
    QFile corruptFile(corrupt);
    QVERIFY(corruptFile.open(QIODevice::WriteOnly));
    QVERIFY(corruptFile.write("not an image") > 0);
    corruptFile.close();
    QVERIFY(QFile::remove(selected));
    QVERIFY(QFile::rename(corrupt, selected));
    QTRY_COMPARE_WITH_TIMEOUT(service.snapshot().state, WallpaperState::Fallback, 1500);
    QCOMPARE(service.snapshot().configured->source(), selected);

    writeImage(selected);
    QTRY_COMPARE_WITH_TIMEOUT(service.snapshot().state, WallpaperState::Ready, 1500);
}

void WallpaperServiceTest::symlinkRetargetReconcilesConfiguredEntryWithBoundedWatches()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto a = writeImage(temp.filePath(QStringLiteral("a.png")));
    const auto b = writeImage(temp.filePath(QStringLiteral("b.png")));
    const auto link = temp.filePath(QStringLiteral("current.png"));
    QVERIFY(QFile::link(a, link));

    WallpaperService service(WallpaperResolver(factory, emergency),
                             persistence(temp.filePath(QStringLiteral("paper.ini"))));
    service.initialize();
    service.setWallpaper(WallpaperDescriptor::externalFile(link, WallpaperFit::Contain));
    QTRY_COMPARE_WITH_TIMEOUT(service.snapshot().state, WallpaperState::Ready, 1000);
    QCOMPARE(service.snapshot().configured->source(), link);
    QCOMPARE(service.snapshot().effective.resolvedSource(), QFileInfo(a).canonicalFilePath());
    QVERIFY(service.sourceWatchPathCountForTests() <= 4);

    QVERIFY(QFile::remove(link));
    QVERIFY(QFile::link(b, link));
    QTRY_COMPARE_WITH_TIMEOUT(service.snapshot().effective.resolvedSource(),
                              QFileInfo(b).canonicalFilePath(),
                              1500);
    QCOMPARE(service.snapshot().configured->source(), link);
    QVERIFY(service.sourceWatchPathCountForTests() <= 4);
}

void WallpaperServiceTest::catalogImportAndStableSelectionSurviveRestart()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto source = writeImage(temp.filePath(QStringLiteral("picked.png")));
    const auto configPath = temp.filePath(QStringLiteral("paper.ini"));
    const auto userDirectory = temp.filePath(QStringLiteral("library"));
    const auto resolver = WallpaperResolver(factory, emergency);

    QString selectedId;
    {
        auto catalog = std::make_shared<WallpaperCatalog>(resolver, userDirectory);
        WallpaperService service(resolver, persistence(configPath), catalog);
        service.initialize();
        QSignalSpy finished(&service, &WallpaperService::wallpaperOperationFinished);

        const auto operationId = service.importWallpaper(source, WallpaperFit::Contain);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1500);
        const auto result = qvariant_cast<WallpaperOperationResult>(finished.at(0).at(1));
        QCOMPARE(result.id, operationId);
        QCOMPARE(result.status, WallpaperOperationStatus::Succeeded);
        QVERIFY(service.snapshot().configured.has_value());
        selectedId = service.snapshot().configured->logicalId();
        QVERIFY(selectedId.startsWith(QStringLiteral("astrea://wallpaper/user/")));
        QCOMPARE(service.snapshot().configured->fit(), WallpaperFit::Contain);
        QVERIFY(QFileInfo::exists(service.snapshot().configured->source()));
        QVERIFY(service.snapshot().configured->source() != source);
    }

    auto catalog = std::make_shared<WallpaperCatalog>(resolver, userDirectory);
    WallpaperService restored(resolver, persistence(configPath), catalog);
    restored.initialize();
    QTRY_COMPARE_WITH_TIMEOUT(restored.snapshot().state, WallpaperState::Ready, 1500);
    QVERIFY(restored.snapshot().configured.has_value());
    QCOMPARE(restored.snapshot().configured->logicalId(), selectedId);
    QCOMPARE(restored.snapshot().configured->fit(), WallpaperFit::Contain);
}

void WallpaperServiceTest::catalogOnlyAddPreservesActiveWallpaper()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto active = writeImage(temp.filePath(QStringLiteral("active.png")));
    const auto added = writeImage(temp.filePath(QStringLiteral("added.png")));
    const auto resolver = WallpaperResolver(factory, emergency);
    auto catalog = std::make_shared<WallpaperCatalog>(resolver, temp.filePath(QStringLiteral("library")));
    WallpaperService service(resolver,
                             persistence(temp.filePath(QStringLiteral("paper.ini"))),
                             catalog);
    service.initialize();

    QSignalSpy imported(&service, &WallpaperService::wallpaperOperationFinished);
    const auto importId = service.importWallpaper(active,
                                                  WallpaperFit::Contain,
                                                  QStringLiteral("Active"));
    QTRY_COMPARE_WITH_TIMEOUT(imported.count(), 1, 1500);
    QCOMPARE(qvariant_cast<WallpaperOperationResult>(imported.at(0).at(1)).id, importId);
    const auto beforeConfigured = service.snapshot().configured->logicalId();
    const auto beforeEffective = service.snapshot().effective.logicalId();
    const auto beforeGeneration = service.snapshot().generation;

    imported.clear();
    const auto addId = service.addWallpaper(added, QStringLiteral("Library B"));
    QCOMPARE(imported.count(), 1);
    const auto addResult = qvariant_cast<WallpaperOperationResult>(imported.at(0).at(1));
    QCOMPARE(addResult.id, addId);
    QCOMPARE(addResult.status, WallpaperOperationStatus::Succeeded);
    QCOMPARE(service.snapshot().configured->logicalId(), beforeConfigured);
    QCOMPARE(service.snapshot().effective.logicalId(), beforeEffective);
    QCOMPARE(service.snapshot().generation, beforeGeneration);

    QString addedId;
    for (const auto &entry : service.listWallpapers()) {
        if (entry.displayName() == QStringLiteral("Library B")) {
            addedId = entry.logicalId();
            break;
        }
    }
    QVERIFY(!addedId.isEmpty());
    imported.clear();
    const auto selectId = service.selectWallpaper(addedId, WallpaperFit::Center);
    QTRY_COMPARE_WITH_TIMEOUT(imported.count(), 1, 1500);
    QCOMPARE(qvariant_cast<WallpaperOperationResult>(imported.at(0).at(1)).id, selectId);
    QCOMPARE(service.snapshot().effective.logicalId(), addedId);
}

void WallpaperServiceTest::pathConvenienceImportsBeforeSelecting()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto source = writeImage(temp.filePath(QStringLiteral("path-entry.png")));
    const auto resolver = WallpaperResolver(factory, emergency);
    auto catalog = std::make_shared<WallpaperCatalog>(
        resolver,
        temp.filePath(QStringLiteral("library")),
        temp.filePath(QStringLiteral("system")));
    WallpaperService service(resolver,
                             persistence(temp.filePath(QStringLiteral("paper.ini"))),
                             catalog);
    service.initialize();

    QSignalSpy finished(&service, &WallpaperService::wallpaperOperationFinished);
    const auto operationId = service.setWallpaperSource(source, QStringLiteral("contain"));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1500);
    const auto result = qvariant_cast<WallpaperOperationResult>(finished.at(0).at(1));

    QCOMPARE(result.id, operationId);
    QCOMPARE(result.status, WallpaperOperationStatus::Succeeded);
    QVERIFY(service.snapshot().configured.has_value());
    QVERIFY(service.snapshot().configured->logicalId().startsWith(
        QStringLiteral("astrea://wallpaper/user/")));
    QVERIFY(service.snapshot().configured->source() != source);
    QCOMPARE(service.snapshot().configured->fit(), WallpaperFit::Contain);
}

void WallpaperServiceTest::migratesLegacySymlinkIntoManagedCatalog()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto source = writeImage(temp.filePath(QStringLiteral("legacy-source.png")));
    const auto legacy = temp.filePath(QStringLiteral("legacy/wallpaper.jpg"));
    QVERIFY(QDir().mkpath(QFileInfo(legacy).absolutePath()));
    QVERIFY(QFile::link(source, legacy));
    const auto configPath = temp.filePath(QStringLiteral("paper.ini"));
    auto catalog = std::make_shared<WallpaperCatalog>(
        WallpaperResolver(factory, emergency), temp.filePath(QStringLiteral("library")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             std::make_unique<XdgWallpaperPersistence>(configPath, legacy),
                             catalog);
    service.initialize();
    QTRY_COMPARE_WITH_TIMEOUT(service.snapshot().state, WallpaperState::Ready, 1500);
    QVERIFY(service.snapshot().configured.has_value());
    QVERIFY(service.snapshot().configured->logicalId()
            .startsWith(QStringLiteral("astrea://wallpaper/user/")));
    QVERIFY(service.snapshot().configured->source() != QFileInfo(source).canonicalFilePath());
    QVERIFY(QFileInfo::exists(service.snapshot().configured->source()));

    const auto persisted = persistence(configPath)->loadSelection();
    QVERIFY(persisted.has_value());
    QCOMPARE(persisted->wallpaperId, service.snapshot().configured->logicalId());
}

void WallpaperServiceTest::missingCatalogSelectionFallsBackWithoutErasingIntent()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto configPath = temp.filePath(QStringLiteral("paper.ini"));
    auto saved = persistence(configPath);
    QString error;
    QVERIFY(saved->saveSelection(
        WallpaperSelection{QStringLiteral("astrea://wallpaper/user/missing"), WallpaperFit::Center},
        &error));

    auto catalog = std::make_shared<WallpaperCatalog>(
        WallpaperResolver(factory, emergency), temp.filePath(QStringLiteral("empty-library")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             persistence(configPath),
                             catalog);
    service.initialize();
    QTRY_COMPARE_WITH_TIMEOUT(service.snapshot().state, WallpaperState::Fallback, 1500);
    QVERIFY(service.snapshot().configured.has_value());
    QCOMPARE(service.snapshot().configured->logicalId(),
             QStringLiteral("astrea://wallpaper/user/missing"));
    QCOMPARE(service.snapshot().configured->fit(), WallpaperFit::Center);
    QCOMPARE(service.snapshot().effective.logicalId(),
             QStringLiteral("astrea://wallpaper/default"));
    QVERIFY(!service.snapshot().lastError.isEmpty());
}

void WallpaperServiceTest::committedWallpaperChangeEmitsOnce()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto configPath = temp.filePath(QStringLiteral("paper.ini"));
    auto catalog = std::make_shared<WallpaperCatalog>(WallpaperResolver(factory, emergency),
                                                       temp.filePath(QStringLiteral("library")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             persistence(configPath),
                             catalog);
    service.initialize();
    QSignalSpy changed(&service, &WallpaperService::wallpaperChanged);
    QSignalSpy finished(&service, &WallpaperService::wallpaperOperationFinished);

    const auto operationId = service.selectWallpaper(QStringLiteral("astrea://wallpaper/default"),
                                                      WallpaperFit::Center);
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 1500);
    const auto result = qvariant_cast<WallpaperOperationResult>(finished.at(0).at(1));
    QCOMPARE(result.id, operationId);
    QCOMPARE(result.status, WallpaperOperationStatus::Succeeded);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(qvariant_cast<qulonglong>(changed.at(0).at(2)), service.snapshot().generation);
    QCOMPARE(changed.at(0).at(3).toString(), QStringLiteral("runtime"));
}

QTEST_MAIN(WallpaperServiceTest)
#include "WallpaperServiceTest.moc"
