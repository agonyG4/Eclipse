#pragma once

#include "core/WindowInfo.hpp"
#include <QHash>
#include <QReadWriteLock>

class IdentityCache {
public:
    explicit IdentityCache(int maxSize = 256) : m_maxSize(maxSize) {}

    bool lookup(const QString &key, AppIdentity &outIdentity) {
        QReadLocker locker(&m_lock);
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            outIdentity = it.value();
            return true;
        }
        return false;
    }

    void insert(const QString &key, const AppIdentity &identity) {
        QWriteLocker locker(&m_lock);
        if (m_cache.size() >= m_maxSize) {
            m_cache.clear();
        }
        m_cache.insert(key, identity);
    }

    void clear() {
        QWriteLocker locker(&m_lock);
        m_cache.clear();
    }

    int size() const {
        QReadLocker locker(&m_lock);
        return m_cache.size();
    }

private:
    QHash<QString, AppIdentity> m_cache;
    int m_maxSize;
    mutable QReadWriteLock m_lock;
};
