#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QObject>
#include <QTimer>

class ScreenshotConfiguration final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY configurationChanged)
    Q_PROPERTY(bool borderEnabled READ borderEnabled WRITE setBorderEnabled NOTIFY configurationChanged)
    Q_PROPERTY(int borderWidth READ borderWidth WRITE setBorderWidth NOTIFY configurationChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY configurationChanged)
    Q_PROPERTY(bool shadowEnabled READ shadowEnabled WRITE setShadowEnabled NOTIFY configurationChanged)
    Q_PROPERTY(qreal shadowStrength READ shadowStrength WRITE setShadowStrength NOTIFY configurationChanged)
    Q_PROPERTY(int thumbnailWidth READ thumbnailWidth WRITE setThumbnailWidth NOTIFY configurationChanged)
    Q_PROPERTY(int thumbnailHeight READ thumbnailHeight WRITE setThumbnailHeight NOTIFY configurationChanged)
    Q_PROPERTY(qreal previewMaximumScale READ previewMaximumScale WRITE setPreviewMaximumScale NOTIFY configurationChanged)
    Q_PROPERTY(QString position READ position WRITE setPosition NOTIFY configurationChanged)
    Q_PROPERTY(int edgeMargin READ edgeMargin WRITE setEdgeMargin NOTIFY configurationChanged)
    Q_PROPERTY(bool animationsEnabled READ animationsEnabled WRITE setAnimationsEnabled NOTIFY configurationChanged)
    Q_PROPERTY(QString entryAnimation READ entryAnimation WRITE setEntryAnimation NOTIFY configurationChanged)
    Q_PROPERTY(QString exitAnimation READ exitAnimation WRITE setExitAnimation NOTIFY configurationChanged)
    Q_PROPERTY(int entryDurationMs READ entryDurationMs WRITE setEntryDurationMs NOTIFY configurationChanged)
    Q_PROPERTY(int exitDurationMs READ exitDurationMs WRITE setExitDurationMs NOTIFY configurationChanged)
    Q_PROPERTY(QString entryEasing READ entryEasing WRITE setEntryEasing NOTIFY configurationChanged)
    Q_PROPERTY(QString exitEasing READ exitEasing WRITE setExitEasing NOTIFY configurationChanged)
    Q_PROPERTY(bool showThumbnail READ showThumbnail WRITE setShowThumbnail NOTIFY configurationChanged)
    Q_PROPERTY(bool autoCopy READ autoCopy WRITE setAutoCopy NOTIFY configurationChanged)
    Q_PROPERTY(bool dragEnabled READ dragEnabled WRITE setDragEnabled NOTIFY configurationChanged)
    Q_PROPERTY(int displayDurationMs READ displayDurationMs WRITE setDisplayDurationMs NOTIFY configurationChanged)
    Q_PROPERTY(QString clickAction READ clickAction WRITE setClickAction NOTIFY configurationChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY statusChanged)
    Q_PROPERTY(QString filePath READ filePath CONSTANT)

public:
    enum class Position { BottomRight, BottomLeft, TopRight, TopLeft };
    enum class AnimationStyle { Fade, ScaleFade, SlideFade };
    enum class Easing { Linear, InQuad, OutCubic, InOutCubic };
    enum class ClickAction { Preview, Copy, Dismiss };

    struct Values {
        int cornerRadius = 10;
        bool borderEnabled = true;
        int borderWidth = 1;
        QColor borderColor;
        bool shadowEnabled = true;
        qreal shadowStrength = 0.55;
        int thumbnailWidth = 240;
        int thumbnailHeight = 160;
        qreal previewMaximumScale = 0.78;
        Position position = Position::BottomRight;
        int edgeMargin = 20;
        bool animationsEnabled = true;
        AnimationStyle entryAnimation = AnimationStyle::ScaleFade;
        AnimationStyle exitAnimation = AnimationStyle::SlideFade;
        int entryDurationMs = 160;
        int exitDurationMs = 220;
        Easing entryEasing = Easing::OutCubic;
        Easing exitEasing = Easing::OutCubic;
        bool showThumbnail = true;
        bool autoCopy = true;
        bool dragEnabled = true;
        int displayDurationMs = 5000;
        ClickAction clickAction = ClickAction::Preview;
    };

    explicit ScreenshotConfiguration(QString filePath = {}, QObject *parent = nullptr);

    int cornerRadius() const { return m_values.cornerRadius; }
    bool borderEnabled() const { return m_values.borderEnabled; }
    int borderWidth() const { return m_values.borderWidth; }
    QColor borderColor() const { return m_values.borderColor; }
    bool shadowEnabled() const { return m_values.shadowEnabled; }
    qreal shadowStrength() const { return m_values.shadowStrength; }
    int thumbnailWidth() const { return m_values.thumbnailWidth; }
    int thumbnailHeight() const { return m_values.thumbnailHeight; }
    qreal previewMaximumScale() const { return m_values.previewMaximumScale; }
    QString position() const;
    int edgeMargin() const { return m_values.edgeMargin; }
    bool animationsEnabled() const { return m_values.animationsEnabled; }
    QString entryAnimation() const;
    QString exitAnimation() const;
    int entryDurationMs() const { return m_values.entryDurationMs; }
    int exitDurationMs() const { return m_values.exitDurationMs; }
    QString entryEasing() const;
    QString exitEasing() const;
    bool showThumbnail() const { return m_values.showThumbnail; }
    bool autoCopy() const { return m_values.autoCopy; }
    bool dragEnabled() const { return m_values.dragEnabled; }
    int displayDurationMs() const { return m_values.displayDurationMs; }
    QString clickAction() const;
    QString status() const { return m_status; }
    QString lastError() const { return m_lastError; }
    QString filePath() const { return m_filePath; }

    void setCornerRadius(int value);
    void setBorderEnabled(bool value);
    void setBorderWidth(int value);
    void setBorderColor(const QColor &value);
    void setShadowEnabled(bool value);
    void setShadowStrength(qreal value);
    void setThumbnailWidth(int value);
    void setThumbnailHeight(int value);
    void setPreviewMaximumScale(qreal value);
    void setPosition(const QString &value);
    void setEdgeMargin(int value);
    void setAnimationsEnabled(bool value);
    void setEntryAnimation(const QString &value);
    void setExitAnimation(const QString &value);
    void setEntryDurationMs(int value);
    void setExitDurationMs(int value);
    void setEntryEasing(const QString &value);
    void setExitEasing(const QString &value);
    void setShowThumbnail(bool value);
    void setAutoCopy(bool value);
    void setDragEnabled(bool value);
    void setDisplayDurationMs(int value);
    void setClickAction(const QString &value);

    Q_INVOKABLE void resetToDefaults();

signals:
    void configurationChanged();
    void statusChanged();

private:
    static constexpr qint64 kMaximumFileBytes = 64 * 1024;

    static Values defaults();
    bool loadFile(bool initialLoad);
    bool writeCandidate(const Values &candidate, const QString &changedField,
                        bool writeEveryField = false);
    void update(const QString &changedField, const Values &candidate);
    void setStatus(QString status, QString error = {});
    void scheduleReload();
    void refreshWatchPaths();
    void reloadFromDisk();

    QString m_filePath;
    Values m_values;
    QJsonObject m_rootObject;
    QByteArray m_lastBytes;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadDebounce;
    QString m_status = QStringLiteral("ready");
    QString m_lastError;
    bool m_writable = true;
    bool m_hasInvalidFields = false;
};
