#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>
#include <QString>

namespace Astrea::System {

class SpatialAudioConfigStore final {
public:
    struct Snapshot {
        bool enabled = false;
        bool valid = true;
        QString error;
    };

    static QString defaultPath()
    {
        QString configHome = qEnvironmentVariable("XDG_CONFIG_HOME");
        if (configHome.isEmpty())
            configHome = QDir::home().filePath(QStringLiteral(".config"));
        return QDir(configHome)
            .filePath(QStringLiteral("AstreaOS/audio/spatial.json"));
    }

    static Snapshot read(const QString &path)
    {
        Snapshot snapshot;
        if (!QFileInfo::exists(path))
            return snapshot;

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            snapshot.valid = false;
            snapshot.error = QStringLiteral("Cannot read Spatial Audio configuration: %1")
                                 .arg(file.errorString());
            return snapshot;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            snapshot.valid = false;
            snapshot.error = QStringLiteral("Invalid Spatial Audio configuration: %1")
                                 .arg(parseError.errorString());
            return snapshot;
        }
        if (!document.isObject()) {
            snapshot.valid = false;
            snapshot.error = QStringLiteral("Spatial Audio configuration must be a JSON object");
            return snapshot;
        }

        const QJsonValue enabledValue = document.object().value(QStringLiteral("enabled"));
        if (enabledValue.isUndefined())
            return snapshot;
        if (!enabledValue.isBool()) {
            snapshot.valid = false;
            snapshot.error = QStringLiteral("Spatial Audio configuration 'enabled' must be boolean");
            return snapshot;
        }

        snapshot.enabled = enabledValue.toBool();
        return snapshot;
    }

    static bool writeEnabled(const QString &path, bool enabled, QString *errorOut = nullptr)
    {
        const QString parentDirectory = QFileInfo(path).absolutePath();
        if (!QDir().mkpath(parentDirectory) && !QFileInfo(parentDirectory).isDir()) {
            setError(errorOut,
                     QStringLiteral("Cannot create Spatial Audio configuration directory: %1")
                         .arg(parentDirectory));
            return false;
        }

        QJsonObject object;
        if (QFileInfo::exists(path)) {
            QFile existingFile(path);
            if (!existingFile.open(QIODevice::ReadOnly)) {
                setError(errorOut,
                         QStringLiteral("Cannot read Spatial Audio configuration before updating: %1")
                             .arg(existingFile.errorString()));
                return false;
            }

            const QByteArray existingContents = existingFile.readAll();
            if (existingFile.error() != QFileDevice::NoError) {
                setError(errorOut,
                         QStringLiteral("Cannot read Spatial Audio configuration before updating: %1")
                             .arg(existingFile.errorString()));
                return false;
            }

            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(existingContents, &parseError);
            if (parseError.error == QJsonParseError::NoError && document.isObject())
                object = document.object();
        }
        object.insert(QStringLiteral("enabled"), enabled);

        QSaveFile file(path);
        file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly)) {
            setError(errorOut,
                     QStringLiteral("Cannot write Spatial Audio configuration: %1")
                         .arg(file.errorString()));
            return false;
        }

        const QByteArray contents = QJsonDocument(object).toJson(QJsonDocument::Indented);
        if (file.write(contents) != static_cast<qint64>(contents.size())) {
            const QString error = file.errorString();
            file.cancelWriting();
            setError(errorOut,
                     QStringLiteral("Cannot write Spatial Audio configuration: %1").arg(error));
            return false;
        }
        if (!file.commit()) {
            setError(errorOut,
                     QStringLiteral("Cannot replace Spatial Audio configuration: %1")
                         .arg(file.errorString()));
            return false;
        }

        if (errorOut)
            errorOut->clear();
        return true;
    }

private:
    static void setError(QString *errorOut, const QString &error)
    {
        if (errorOut)
            *errorOut = error;
    }
};

} // namespace Astrea::System
