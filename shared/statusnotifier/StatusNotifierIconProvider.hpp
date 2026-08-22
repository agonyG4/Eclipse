#pragma once

#include <QQuickImageProvider>

namespace Astrea::StatusNotifier {
class StatusNotifierIconStore;

class StatusNotifierIconProvider final : public QQuickImageProvider {
public:
    explicit StatusNotifierIconProvider(StatusNotifierIconStore *store);
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    StatusNotifierIconStore *m_store = nullptr;
};
} // namespace Astrea::StatusNotifier
