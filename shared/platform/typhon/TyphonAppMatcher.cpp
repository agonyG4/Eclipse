#include "platform/typhon/TyphonAppMatcher.hpp"

#include <QFileInfo>

#include <algorithm>

using namespace Astrea::Typhon;

namespace {

bool visible(const DesktopEntryRecord &entry)
{
    return !entry.hidden && !entry.noDisplay;
}

QString normalizedDesktopId(QString value)
{
    value = value.trimmed().toLower();
    if (value.endsWith(QStringLiteral(".desktop")))
        value.chop(QStringLiteral(".desktop").size());
    value.replace(QLatin1Char('_'), QLatin1Char('-'));
    return value;
}

std::optional<DesktopEntryRecord> choose(const DesktopEntrySnapshot &snapshot,
                                         QVector<int> indices)
{
    indices.erase(std::remove_if(indices.begin(), indices.end(), [&snapshot](int index) {
        return index < 0 || index >= snapshot.entries.size();
    }), indices.end());
    if (indices.isEmpty())
        return std::nullopt;

    std::sort(indices.begin(), indices.end(), [&snapshot](int left, int right) {
        const DesktopEntryRecord &leftEntry = snapshot.entries.at(left);
        const DesktopEntryRecord &rightEntry = snapshot.entries.at(right);
        if (visible(leftEntry) != visible(rightEntry))
            return visible(leftEntry);
        return leftEntry.desktopFileName < rightEntry.desktopFileName;
    });
    return snapshot.entries.at(indices.first());
}

TyphonAppMatch makeMatch(const DesktopEntryRecord &entry, MatchConfidence confidence)
{
    TyphonAppMatch result;
    result.desktopFileName = entry.desktopFileName;
    result.desktopId = entry.id;
    result.displayName = entry.name;
    result.confidence = confidence;
    if (entry.icon.startsWith(QLatin1Char('/')) || entry.icon.startsWith(QStringLiteral("file://")))
        result.iconPath = entry.icon;
    else
        result.iconName = entry.icon;
    return result;
}

QVector<int> matchingEntries(const DesktopEntrySnapshot &snapshot,
                             const std::function<bool(const DesktopEntryRecord &)> &predicate)
{
    QVector<int> result;
    for (int index = 0; index < snapshot.entries.size(); ++index) {
        if (predicate(snapshot.entries.at(index)))
            result.append(index);
    }
    return result;
}

} // namespace

TyphonAppMatcher::TyphonAppMatcher(std::shared_ptr<const DesktopEntrySnapshot> snapshot)
    : m_snapshot(std::move(snapshot))
{
    if (!m_snapshot)
        m_snapshot = std::make_shared<const DesktopEntrySnapshot>();
}

void TyphonAppMatcher::setSnapshot(std::shared_ptr<const DesktopEntrySnapshot> snapshot)
{
    m_snapshot = std::move(snapshot);
    if (!m_snapshot)
        m_snapshot = std::make_shared<const DesktopEntrySnapshot>();
}

TyphonAppMatch TyphonAppMatcher::match(const TyphonAppMatchInput &input) const
{
    Q_UNUSED(input.title);
    Q_UNUSED(input.pid);
    Q_UNUSED(input.kind);

    if (!m_snapshot || input.appId.trimmed().isEmpty())
        return {};

    const QString appId = input.appId.trimmed();
    if (appId.endsWith(QStringLiteral(".desktop"), Qt::CaseSensitive)) {
        const auto fileIt = m_snapshot->byDesktopFileName.constFind(appId);
        if (fileIt != m_snapshot->byDesktopFileName.constEnd())
            return makeMatch(m_snapshot->entries.at(fileIt.value()), MatchConfidence::ExactDesktopFileName);
    }

    QString desktopId = appId;
    if (desktopId.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive))
        desktopId.chop(QStringLiteral(".desktop").size());

    const auto exactId = m_snapshot->byDesktopId.constFind(desktopId);
    if (exactId != m_snapshot->byDesktopId.constEnd())
        return makeMatch(m_snapshot->entries.at(exactId.value()), MatchConfidence::ExactDesktopId);

    const auto caseInsensitiveId = choose(*m_snapshot, matchingEntries(
        *m_snapshot, [&desktopId](const DesktopEntryRecord &entry) {
            return entry.id.compare(desktopId, Qt::CaseInsensitive) == 0;
        }));
    if (caseInsensitiveId.has_value())
        return makeMatch(caseInsensitiveId.value(), MatchConfidence::CaseInsensitiveDesktopId);

    const auto exactWmClass = choose(*m_snapshot, matchingEntries(
        *m_snapshot, [&appId](const DesktopEntryRecord &entry) {
            return !entry.startupWmClass.isEmpty() && entry.startupWmClass == appId;
        }));
    if (exactWmClass.has_value())
        return makeMatch(exactWmClass.value(), MatchConfidence::ExactStartupWmClass);

    const auto caseInsensitiveWmClass = choose(*m_snapshot, matchingEntries(
        *m_snapshot, [&appId](const DesktopEntryRecord &entry) {
            return !entry.startupWmClass.isEmpty()
                && entry.startupWmClass.compare(appId, Qt::CaseInsensitive) == 0;
        }));
    if (caseInsensitiveWmClass.has_value())
        return makeMatch(caseInsensitiveWmClass.value(), MatchConfidence::CaseInsensitiveStartupWmClass);

    const QString normalized = normalizedDesktopId(desktopId);
    const auto normalizedId = choose(*m_snapshot, matchingEntries(
        *m_snapshot, [&normalized](const DesktopEntryRecord &entry) {
            return normalizedDesktopId(entry.id) == normalized;
        }));
    if (normalizedId.has_value())
        return makeMatch(normalizedId.value(), MatchConfidence::NormalizedReverseDnsDesktopId);

    return {};
}
