#include "platform/typhon/TyphonWorkspaceState.hpp"

#include <algorithm>
#include <utility>

void TyphonWorkspaceState::beginGeneration(std::uint64_t generation)
{
    m_generation = generation;
    m_currentIndex = -1;
    m_pending.clear();
    m_committed.clear();
}

void TyphonWorkspaceState::beginWorkspace(const QString &id)
{
    if (id.isEmpty()) {
        m_currentIndex = -1;
        return;
    }
    const auto existing = std::find_if(m_pending.cbegin(), m_pending.cend(),
                                       [&id](const TyphonWorkspaceRecord &workspace) {
        return workspace.id == id;
    });
    if (existing == m_pending.cend()) {
        m_pending.append({id, id, {}, false, false, false, false});
        m_currentIndex = m_pending.size() - 1;
    } else {
        m_currentIndex = static_cast<int>(std::distance(m_pending.cbegin(), existing));
    }
}

TyphonWorkspaceRecord *TyphonWorkspaceState::currentWorkspace()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_pending.size())
        return nullptr;
    return &m_pending[m_currentIndex];
}

const TyphonWorkspaceRecord *TyphonWorkspaceState::currentWorkspace() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_pending.size())
        return nullptr;
    return &m_pending.at(m_currentIndex);
}

void TyphonWorkspaceState::setWorkspaceName(const QString &name)
{
    if (auto *workspace = currentWorkspace())
        workspace->name = name;
}

void TyphonWorkspaceState::setWorkspaceCoordinates(QVector<std::uint32_t> coordinates)
{
    if (auto *workspace = currentWorkspace())
        workspace->coordinates = std::move(coordinates);
}

void TyphonWorkspaceState::setWorkspaceState(std::uint32_t state)
{
    if (auto *workspace = currentWorkspace()) {
        workspace->active = (state & 1u) != 0;
        workspace->urgent = (state & 2u) != 0;
        workspace->hidden = (state & 4u) != 0;
    }
}

void TyphonWorkspaceState::setWorkspaceCapabilities(std::uint32_t capabilities)
{
    if (auto *workspace = currentWorkspace())
        workspace->activationAvailable = (capabilities & 1u) != 0;
}

bool TyphonWorkspaceState::commitDone(std::uint64_t generation)
{
    if (generation != m_generation)
        return false;
    m_committed = m_pending;
    m_committed.erase(std::remove_if(m_committed.begin(), m_committed.end(),
                                     [](const TyphonWorkspaceRecord &workspace) {
        return workspace.hidden;
    }), m_committed.end());
    return true;
}

void TyphonWorkspaceState::clear(std::uint64_t generation)
{
    if (generation != m_generation)
        return;
    m_currentIndex = -1;
    m_pending.clear();
    m_committed.clear();
}
