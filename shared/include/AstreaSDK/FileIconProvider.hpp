#pragma once
#include <AstreaSDK/Export.hpp>
#include <AstreaSDK/IconThemeService.hpp>
#include <QQuickImageProvider>
#include <QPointer>

namespace AstreaSDK {

class ASTREA_SDK_EXPORT FileIconProvider final : public QQuickImageProvider
{
public:
    explicit FileIconProvider(IconThemeService *service);

    IconThemeService *service() const { return m_service.data(); }

    QImage requestImage(
        const QString &id,
        QSize *size,
        const QSize &requestedSize) override;

private:
    QPointer<IconThemeService> m_service;
};

} // namespace AstreaSDK
