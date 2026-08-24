#include "WallpaperValidationWorker.hpp"

namespace Paper {

WallpaperValidationWorker::WallpaperValidationWorker(WallpaperResolver resolver, QObject *parent)
    : QObject(parent)
    , m_resolver(std::move(resolver))
{
}

void WallpaperValidationWorker::validate(const quint64 token, WallpaperDescriptor descriptor)
{
    emit validated(token, m_resolver.resolve(descriptor));
}

} // namespace Paper
