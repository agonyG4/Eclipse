#pragma once

#include <QObject>
#include <QJsonObject>
#include <QString>
#include <QPointer>
#include <QVariantList>

class SettingsWallpaperController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString configuredSource READ configuredSource NOTIFY snapshotChanged)
    Q_PROPERTY(QString configuredId READ configuredId NOTIFY snapshotChanged)
    Q_PROPERTY(QString configuredFit READ configuredFit NOTIFY snapshotChanged)
    Q_PROPERTY(QString effectiveSource READ effectiveSource NOTIFY snapshotChanged)
    Q_PROPERTY(QString effectiveId READ effectiveId NOTIFY snapshotChanged)
    Q_PROPERTY(QString effectiveFit READ effectiveFit NOTIFY snapshotChanged)
    Q_PROPERTY(QString stateName READ stateName NOTIFY snapshotChanged)
    Q_PROPERTY(QString fallbackReason READ fallbackReason NOTIFY snapshotChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)
    Q_PROPERTY(QString errorCode READ errorCode NOTIFY errorChanged)
    Q_PROPERTY(qulonglong generation READ generation NOTIFY snapshotChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString pendingAction READ pendingAction NOTIFY busyChanged)
    Q_PROPERTY(QVariantList wallpapers READ wallpapers NOTIFY snapshotChanged)

public:
    explicit SettingsWallpaperController(QString endpoint = {}, QObject *parent = nullptr);

    QString endpoint() const { return m_endpoint; }
    QString configuredSource() const { return m_configuredSource; }
    QString configuredId() const { return m_configuredId; }
    QString configuredFit() const { return m_configuredFit; }
    QString effectiveSource() const { return m_effectiveSource; }
    QString effectiveId() const { return m_effectiveId; }
    QString effectiveFit() const { return m_effectiveFit; }
    QString stateName() const { return m_stateName; }
    QString fallbackReason() const { return m_fallbackReason; }
    QString errorMessage() const { return m_errorMessage; }
    QString errorCode() const { return m_errorCode; }
    quint64 generation() const { return m_generation; }
    bool busy() const { return m_busy; }
    QString pendingAction() const { return m_pendingAction; }
    QVariantList wallpapers() const { return m_wallpapers; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void refreshLibrary();
    Q_INVOKABLE void setSource(const QString &source,
                               const QString &fit = QStringLiteral("cover"));
    Q_INVOKABLE void selectWallpaper(const QString &logicalId,
                                     const QString &fit = QStringLiteral("cover"));
    Q_INVOKABLE void importWallpaper(const QString &path,
                                     const QString &fit = QStringLiteral("cover"));
    Q_INVOKABLE void reset();
    Q_INVOKABLE void loadDefault();

signals:
    void snapshotChanged();
    void errorChanged();
    void busyChanged();

private:
    static QString defaultEndpoint();
    void startRequest(const QString &action, const QJsonObject &argument);
    void handleConnected(quint64 requestId, const QJsonObject &argument);
    void handleReadyRead(quint64 requestId);
    void finishResponse(quint64 requestId, const QByteArray &payload);
    void finishError(quint64 requestId, const QString &code, const QString &message);
    bool applyResponse(const QByteArray &payload);
    void setBusy(bool busy, const QString &action = {});
    static bool isSupportedFit(const QString &fit);

    QString m_endpoint;
    QString m_configuredSource;
    QString m_configuredId;
    QString m_configuredFit;
    QString m_effectiveSource;
    QString m_effectiveId;
    QString m_effectiveFit;
    QString m_stateName;
    QString m_fallbackReason;
    QString m_errorCode;
    QString m_errorMessage;
    quint64 m_generation = 0;
    QPointer<class QLocalSocket> m_socket;
    class QTimer *m_timeout = nullptr;
    QByteArray m_readBuffer;
    quint64 m_requestId = 0;
    QString m_pendingAction;
    QVariantList m_wallpapers;
    bool m_busy = false;
};
