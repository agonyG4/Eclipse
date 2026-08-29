#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

struct DockConfig {
    int iconSize = 48;
    // Deprecated source compatibility field. New code uses edgeMargin.
    int bottomMargin = 12;
    int panelPadding = 14;
    int itemSpacing = 10;
    QString hoverEffect = QStringLiteral("magnification");
    double magnificationScale = 1.6;
    double magnificationRadius = 2.5;
    int edgeMargin = 12;
    QString position = QStringLiteral("bottom");
    bool floating = true;
    int cornerRadius = 23;
    QString autoHide = QStringLiteral("never");
    QString indicatorStyle = QStringLiteral("line");
    int indicatorSize = 3;
    bool animationsEnabled = true;
    double animationSpeed = 1.0;
    QStringList pins;

    static DockConfig defaults();

    int effectiveEdgeMargin() const { return floating ? edgeMargin : 0; }
};

namespace DockConfigCodec {

inline constexpr qint64 kMaximumConfigBytes = 1024LL * 1024LL;
inline constexpr int kMaximumPins = 256;
inline constexpr int kMaximumPinLength = 255;

struct JsonResult {
    QJsonObject object;
    bool exists = false;
    QString error;
};

JsonResult readJsonObject(const QString &path);
DockConfig parse(const QJsonObject &object, QStringList *errors = nullptr);
bool validDesktopFileName(const QString &fileName);
bool validatePinList(const QStringList &pins, QString *errorOut = nullptr);
bool validateConfig(const DockConfig &config, QString *errorOut = nullptr);
QJsonObject patchKnownFields(const QJsonObject &source, const DockConfig &config,
                             bool includePins);

} // namespace DockConfigCodec
