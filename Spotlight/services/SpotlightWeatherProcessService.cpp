#include "services/SpotlightWeatherProcessService.hpp"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include <utility>

namespace {

void retireCanceledProcess(QProcess *process)
{
    process->setParent(nullptr);
    QObject::connect(process,
                     QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     process, &QObject::deleteLater);
    QObject::connect(process, &QProcess::errorOccurred, process,
                     [process](QProcess::ProcessError) {
        if (process->state() == QProcess::NotRunning)
            process->deleteLater();
    });
    if (process->state() != QProcess::NotRunning)
        process->kill();
    else
        process->deleteLater();
}

} // namespace

SpotlightWeatherProcessService::SpotlightWeatherProcessService(QString executable,
                                                               QObject *parent)
    : SpotlightWeatherProvider(parent)
    , m_executable(std::move(executable))
{
}

SpotlightWeatherProcessService::~SpotlightWeatherProcessService()
{
    ++m_requestGeneration;
    QPointer<QProcess> process = m_process;
    m_process.clear();
    if (!process)
        return;

    QObject::disconnect(process, nullptr, this, nullptr);
    if (process->state() != QProcess::NotRunning) {
        process->kill();
        if (!process->waitForFinished(1000) && process->state() != QProcess::NotRunning) {
            retireCanceledProcess(process);
            return;
        }
    }
    delete process;
}

void SpotlightWeatherProcessService::refresh()
{
    cancel();
    const quint64 generation = ++m_requestGeneration;

    auto *process = new QProcess(this);
    m_process = process;

    connect(process, &QProcess::finished, this,
            [this, process, generation](int exitCode, QProcess::ExitStatus status) {
        if (generation != m_requestGeneration || process != m_process) {
            process->deleteLater();
            return;
        }

        const QByteArray standardError = process->readAllStandardError();
        const QByteArray standardOutput = process->readAllStandardOutput();
        m_process.clear();
        process->deleteLater();

        if (exitCode != 0 || status != QProcess::NormalExit) {
            const QString stderrText = QString::fromUtf8(standardError).trimmed();
            if (!stderrText.isEmpty())
                qWarning("Weather CLI stderr: %s", qPrintable(stderrText));
            emit failed(SpotlightWeatherProvider::Failure::NoData);
            return;
        }

        const QJsonDocument document = QJsonDocument::fromJson(standardOutput);
        if (!document.isObject()) {
            emit failed(SpotlightWeatherProvider::Failure::InvalidData);
            return;
        }

        const QJsonObject object = document.object();
        emit ready(object.value(QStringLiteral("temp")).toInt(),
                   object.value(QStringLiteral("city")).toString(),
                   object.value(QStringLiteral("condition")).toString());
    });

    connect(process, &QProcess::errorOccurred, this,
            [this, process, generation](QProcess::ProcessError error) {
        Q_UNUSED(error);
        if (generation != m_requestGeneration || process != m_process)
            return;

        m_process.clear();
        process->deleteLater();
        emit failed(SpotlightWeatherProvider::Failure::ConnectionFailed);
    });

    process->setProgram(m_executable);
    process->setArguments({QStringLiteral("summary")});
    process->start();
}

void SpotlightWeatherProcessService::cancel()
{
    ++m_requestGeneration;
    QPointer<QProcess> process = m_process;
    m_process.clear();
    if (!process)
        return;

    QObject::disconnect(process, nullptr, this, nullptr);
    retireCanceledProcess(process);
}
