#pragma once

#include <QObject>

class BarLayoutMetrics final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int barHeight READ barHeight CONSTANT)
    Q_PROPERTY(int pillHeight READ pillHeight CONSTANT)
    Q_PROPERTY(int topMargin READ topMargin CONSTANT)
    Q_PROPERTY(int launcherLeftMargin READ launcherLeftMargin CONSTANT)
    Q_PROPERTY(int statusRightMargin READ statusRightMargin CONSTANT)
    Q_PROPERTY(int sidePadding READ sidePadding CONSTANT)
    Q_PROPERTY(int minimumGap READ minimumGap CONSTANT)
    Q_PROPERTY(int popupSidePadding READ popupSidePadding CONSTANT)
    Q_PROPERTY(int popupTop READ popupTop CONSTANT)

public:
    explicit BarLayoutMetrics(QObject *parent = nullptr);

    int barHeight() const;
    int pillHeight() const;
    int topMargin() const;
    int launcherLeftMargin() const;
    int statusRightMargin() const;
    int sidePadding() const;
    int minimumGap() const;
    int popupSidePadding() const;
    int popupTop() const;

    Q_INVOKABLE int statusWidth(int outputWidth, int launcherWidth, int pillWidth) const;
    Q_INVOKABLE int statusLeft(int outputWidth, int statusWidth) const;
    Q_INVOKABLE int statusAnchorX(int outputWidth, int statusWidth,
                                  int indicatorLocalX) const;
    Q_INVOKABLE int launcherAnchorX(int launcherWidth) const;
    Q_INVOKABLE int popupWidth(int outputWidth, int cardWidth,
                               int sidePadding = -1) const;
    Q_INVOKABLE int popupX(int outputWidth, int cardWidth, int anchorX,
                           int sidePadding = -1) const;
};
