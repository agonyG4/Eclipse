#pragma once

#include "WallpaperResolver.hpp"

#include <QObject>

namespace Paper {

class WallpaperValidationWorker final : public QObject
{
    Q_OBJECT

public:
    explicit WallpaperValidationWorker(WallpaperResolver resolver,
                                       QObject *parent = nullptr);

public slots:
    void validate(quint64 token, WallpaperDescriptor descriptor);

signals:
    void validated(quint64 token, WallpaperResolution result);

private:
    WallpaperResolver m_resolver;
};

} // namespace Paper

Q_DECLARE_METATYPE(Paper::WallpaperResolution)
