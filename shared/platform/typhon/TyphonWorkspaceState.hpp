#pragma once

#include <QString>
#include <QVector>

#include <cstdint>

struct TyphonWorkspaceRecord {
    QString id;
    QString name;
    QVector<std::uint32_t> coordinates;
    bool active = false;
    bool urgent = false;
    bool hidden = false;
    bool activationAvailable = false;
};

class TyphonWorkspaceState final {
public:
    void beginGeneration(std::uint64_t generation);
    void beginWorkspace(const QString &id);
    void setWorkspaceName(const QString &name);
    void setWorkspaceCoordinates(QVector<std::uint32_t> coordinates);
    void setWorkspaceState(std::uint32_t state);
    void setWorkspaceCapabilities(std::uint32_t capabilities);
    bool commitDone(std::uint64_t generation);
    void clear(std::uint64_t generation);

    std::uint64_t generation() const { return m_generation; }
    const QVector<TyphonWorkspaceRecord> &pendingWorkspaces() const { return m_pending; }
    const QVector<TyphonWorkspaceRecord> &committedWorkspaces() const { return m_committed; }

private:
    TyphonWorkspaceRecord *currentWorkspace();
    const TyphonWorkspaceRecord *currentWorkspace() const;

    std::uint64_t m_generation = 0;
    int m_currentIndex = -1;
    QVector<TyphonWorkspaceRecord> m_pending;
    QVector<TyphonWorkspaceRecord> m_committed;
};
