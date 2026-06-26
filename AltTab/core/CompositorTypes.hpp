#pragma once

#include <QString>
#include <QHash>
#include <QMetaType>

struct WindowId {
    QString value;
    bool operator==(const WindowId &other) const { return value == other.value; }
    bool operator!=(const WindowId &other) const { return value != other.value; }
    bool isEmpty() const { return value.isEmpty(); }
};

struct WorkspaceId {
    QString value;
    bool operator==(const WorkspaceId &other) const { return value == other.value; }
    bool operator!=(const WorkspaceId &other) const { return value != other.value; }
    bool isEmpty() const { return value.isEmpty(); }
};

struct OutputId {
    QString value;
    bool operator==(const OutputId &other) const { return value == other.value; }
    bool operator!=(const OutputId &other) const { return value != other.value; }
    bool isEmpty() const { return value.isEmpty(); }
};

inline uint qHash(const WindowId &key, uint seed = 0) { return qHash(key.value, seed); }
inline uint qHash(const WorkspaceId &key, uint seed = 0) { return qHash(key.value, seed); }
inline uint qHash(const OutputId &key, uint seed = 0) { return qHash(key.value, seed); }

Q_DECLARE_METATYPE(WindowId)
Q_DECLARE_METATYPE(WorkspaceId)
Q_DECLARE_METATYPE(OutputId)
