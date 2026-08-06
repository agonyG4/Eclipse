#pragma once

#include <QFlags>
#include <QMetaType>
#include <QString>
#include <QVector>

namespace Astrea::Typhon {

using Revision = quint64;
using FocusSerial = quint64;

enum class ToplevelKind {
    XdgToplevel,
    X11Toplevel,
    X11Dialog
};

enum class ToplevelStateFlag : quint32 {
    Active = 1u << 0,
    Minimized = 1u << 1,
    Maximized = 1u << 2,
    Fullscreen = 1u << 3
};
Q_DECLARE_FLAGS(ToplevelStates, ToplevelStateFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(ToplevelStates)

struct Toplevel {
    QString id;
    QString appId;
    QString title;
    quint32 pid = 0;
    ToplevelKind kind = ToplevelKind::XdgToplevel;
    ToplevelStates states;
    FocusSerial focusSerial = 0;
    Revision revision = 0;
    quint32 rawStateBits = 0;
};

struct Snapshot {
    QVector<Toplevel> windows;
    Revision revision = 0;
    quint32 total = 0;
    bool truncated = false;
    quint64 connectionGeneration = 0;
};

inline bool hasState(ToplevelStates states, ToplevelStateFlag flag)
{
    return states.testFlag(flag);
}

} // namespace Astrea::Typhon

Q_DECLARE_METATYPE(Astrea::Typhon::ToplevelKind)
Q_DECLARE_METATYPE(Astrea::Typhon::ToplevelStates)
Q_DECLARE_METATYPE(Astrea::Typhon::Toplevel)
Q_DECLARE_METATYPE(Astrea::Typhon::Snapshot)
