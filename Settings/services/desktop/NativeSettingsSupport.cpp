#include "NativeSettingsSupport.hpp"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtConcurrentRun>

namespace NativeSettings
{
void fail(const QString &message) { throw std::runtime_error(message.toUtf8().constData()); }
QString configPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
           QStringLiteral("/AstreaOS/system/settings.json");
}
QVariantMap readObject(const QString &path)
{
    QFile file(path);
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly))
        fail(path + ": " + file.errorString());
    if (file.size() > 4 * 1024 * 1024)
        fail(path + ": configuration exceeds 4 MiB");
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        fail(path + ": invalid JSON object: " + error.errorString());
    return doc.object().toVariantMap();
}
void patchObject(const QString &path, const std::function<void(QVariantMap &)> &patch)
{
    const auto parent = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(parent))
        fail("Could not create " + parent);
    QLockFile lock(path + ".lock");
    if (!lock.tryLock(5000))
        fail("Configuration is locked: " + path);
    auto doc = readObject(path);
    patch(doc);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        fail(file.errorString());
    if (!QFile::exists(path))
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    const auto bytes = QJsonDocument(QJsonObject::fromVariantMap(doc)).toJson();
    if (file.write(bytes) != bytes.size() || !file.commit())
        fail("Could not save " + path + ": " + file.errorString());
}
CommandResult run(const QString &program, const QStringList &args, int timeout)
{
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("LC_ALL", "C");
    process.setProcessEnvironment(environment);
    process.start(program, args);
    if (!process.waitForStarted(3000))
        fail(program + ": " + process.errorString());
    QElapsedTimer timer;
    timer.start();
    QByteArray out, error;
    while (process.state() != QProcess::NotRunning) {
        process.waitForReadyRead(50);
        out += process.readAllStandardOutput();
        error += process.readAllStandardError();
        if (timer.elapsed() > timeout || out.size() + error.size() > 1024 * 1024) {
            process.kill();
            process.waitForFinished(1000);
            fail(program + ": command timed out or exceeded its output limit");
        }
    }
    out += process.readAllStandardOutput();
    error += process.readAllStandardError();
    if (process.exitStatus() != QProcess::NormalExit)
        fail(program + ": command crashed");
    return {process.exitCode(), QString::fromUtf8(out).trimmed(),
            QString::fromUtf8(error).trimmed()};
}
QString require(const QString &program, const QStringList &args, int timeout)
{
    const auto result = run(program, args, timeout);
    if (result.code != 0)
        fail(program + ": " + (result.error.isEmpty() ? result.out : result.error) +
             QStringLiteral(" (exit %1)").arg(result.code));
    return result.out;
}
} // namespace NativeSettings
SettingsAsyncController::SettingsAsyncController(QObject *parent) : QObject(parent)
{
    m_pool.setMaxThreadCount(1);
    connect(&m_watcher, &QFutureWatcher<Result>::finished, this, [this] {
        const auto result = m_watcher.result();
        m_busy = false;
        if (result.error.isEmpty()) {
            m_state = result.state;
            emit stateChanged();
        }
        report(result.error);
        emit busyChanged();
        if (result.error.isEmpty() && m_mutation)
            emit operationSucceeded();
    });
}
SettingsAsyncController::~SettingsAsyncController() { m_pool.waitForDone(); }
void SettingsAsyncController::report(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}
void SettingsAsyncController::submit(std::function<QVariantMap()> job, bool mutation)
{
    if (m_busy)
        return;
    m_busy = true;
    m_mutation = mutation;
    report({});
    emit busyChanged();
    m_watcher.setFuture(QtConcurrent::run(&m_pool, [job = std::move(job)] {
        try {
            return Result{job(), {}};
        } catch (const std::exception &error) {
            return Result{{}, QString::fromUtf8(error.what())};
        }
    }));
}
