#pragma once

#include <QQuickImageProvider>

class DockWindowPreviewService;

class DockWindowPreviewImageProvider final : public QQuickImageProvider {
public:
    explicit DockWindowPreviewImageProvider(DockWindowPreviewService *service);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    DockWindowPreviewService *m_service = nullptr;
};
