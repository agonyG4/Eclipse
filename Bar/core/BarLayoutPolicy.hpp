#pragma once

class BarLayoutPolicy final {
public:
    static constexpr int kBarHeight = 45;
    static constexpr int kPillHeight = 36;
    static constexpr int kLauncherLeftMargin = 8;
    static constexpr int kPillTopMargin = 5;
    static constexpr int kStatusRightMargin = 6;
    static constexpr int kMinimumGap = 28;
    static constexpr int kSidePadding = 10;
    static constexpr int kPopupSidePadding = 8;
    static constexpr int kPopupTopOffset = 54;
    static constexpr int kTrayTooltipHeight = 28;
    static constexpr int kTrayTooltipTopMargin = 43;

    static int statusWidth(int outputWidth, int launcherWidth, int pillWidth);
    static int statusLeft(int outputWidth, int statusWidth);
    static int popupX(int outputWidth, int cardWidth, int anchorX,
                      int sidePadding = kPopupSidePadding);
};
