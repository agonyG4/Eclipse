#pragma once

#include <QQuickImageProvider>

class ScreenshotController;

class ScreenshotImageProvider final : public QQuickImageProvider {
public:
    explicit ScreenshotImageProvider(ScreenshotController *controller);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    ScreenshotController *m_controller = nullptr;
};
