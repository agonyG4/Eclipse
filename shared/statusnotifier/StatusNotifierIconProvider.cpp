#include "statusnotifier/StatusNotifierIconProvider.hpp"

#include "statusnotifier/StatusNotifierIconStore.hpp"

#include <QUrl>

namespace Astrea::StatusNotifier {

StatusNotifierIconProvider::StatusNotifierIconProvider(StatusNotifierIconStore *store)
    : QQuickImageProvider(QQuickImageProvider::Pixmap), m_store(store)
{
}

QPixmap StatusNotifierIconProvider::requestPixmap(const QString &id, QSize *size,
                                                   const QSize &requestedSize)
{
    const int query = id.indexOf(QLatin1Char('?'));
    const QString key = QUrl::fromPercentEncoding((query < 0 ? id : id.left(query)).toUtf8());
    const QSize target = requestedSize.isValid() ? requestedSize : QSize(16, 16);
    const QPixmap result = m_store ? m_store->pixmap(key, target) : QPixmap();
    if (size)
        *size = result.isNull() ? target : result.size();
    return result;
}

} // namespace Astrea::StatusNotifier
