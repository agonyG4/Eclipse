#include "theme/Typography.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <cmath>
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace Astrea::Typography {
namespace {
bool familyValid(const QVariant &value)
{
    if (value.metaType().id() != QMetaType::QString)
        return false;
    const QString family = value.toString();
    if (family.trimmed().isEmpty() || family.size() > 80 || family.contains('/') || family.contains('\\'))
        return false;
    for (const auto c : family)
        if (c.category() == QChar::Other_Control)
            return false;
    return true;
}
bool numberValid(const QVariant &value, double min, double max, bool integral = false)
{
    const auto type = value.metaType().id();
    if (type != QMetaType::Double && type != QMetaType::Int && type != QMetaType::LongLong)
        return false;
    const double n = value.toDouble();
    return std::isfinite(n) && n >= min && n <= max && (!integral || n == std::floor(n));
}
const QStringList familyKeys{"fontFamily", "fontFamilyText", "fontFamilyDisplay", "monoFontFamily"};
}

QStringList roleNames() { return {"display", "title", "body", "label", "caption", "mono"}; }

QVariantMap validatedRoles(const QVariantMap &roles)
{
    QVariantMap result;
    for (const auto &name : roleNames()) {
        const auto input = roles.value(name).toMap();
        QVariantMap role;
        if (familyValid(input.value("family")))
            role.insert("family", input.value("family"));
        for (const auto &field : {QString("pixelSize"), QString("weight"), QString("letterSpacing"), QString("lineHeight")}) {
            const double min = field == "letterSpacing" ? -5 : 1;
            const double max = field == "pixelSize" ? 96 : field == "weight" ? 1000 : field == "lineHeight" ? 3 : 20;
            if (numberValid(input.value(field), min, max, field == "pixelSize" || field == "weight"))
                role.insert(field, input.value(field));
        }
        if (!role.isEmpty())
            result.insert(name, role);
    }
    return result;
}

QVariantMap validatedPreferences(const QVariantMap &preferences)
{
    QVariantMap result;
    for (const auto &key : familyKeys)
        if (familyValid(preferences.value(key)))
            result.insert(key, preferences.value(key));
    for (const auto &key : {QString("fontSizeNormal"), QString("fontSizeTitle")})
        if (numberValid(preferences.value(key), 9, 96, true))
            result.insert(key, preferences.value(key).toInt());
    if (numberValid(preferences.value("textScale"), 0.75, 2.0))
        result.insert("textScale", preferences.value("textScale"));
    const auto roles = validatedRoles(preferences.value("roles").toMap());
    if (!roles.isEmpty())
        result.insert("roles", roles);
    return result;
}

QVariantMap resolve(const QVariantMap &theme, const QVariantMap &rawPreferences,
                    const QString &proportional, const QString &monospace)
{
    const auto prefs = validatedPreferences(rawPreferences);
    QVariantMap result = theme;
    const auto fallback = [](const QVariantMap &map, const QString &key, const QString &value) {
        const QString family = map.value(key).toString();
        return family.isEmpty() ? value : family;
    };
    const QString primary = fallback(prefs, "fontFamily", fallback(theme, "fontFamily", proportional));
    const QString mono = fallback(prefs, "monoFontFamily", fallback(theme, "monoFontFamily", monospace));
    const auto inherited = [&](const QString &key) {
        return fallback(prefs, key, prefs.contains("fontFamily") ? primary : fallback(theme, key, primary));
    };
    const QString display = inherited("fontFamilyDisplay"), body = inherited("fontFamilyText");
    result.insert("fontFamily", primary);
    result.insert("fontFamilyDisplay", display);
    result.insert("fontFamilyText", body);
    result.insert("monoFontFamily", mono);
    const double scale = prefs.value("textScale", 1.0).toDouble();
    result.insert("textScale", scale);

    // Compatibility sizes preserve old Settings metrics. Icons never follow text scale.
    const QVariantMap sizes{{"fontSizeHero",20}, {"fontSizeHeader",24}, {"fontSizeSubtitle",16},
        {"fontSizeTitle",15}, {"fontSizeLarge",13}, {"fontSizeNormal",12}, {"fontSizeSmall",11},
        {"fontSizeTiny",10}, {"fontSizeMicro",9}, {"fontSizeBody",12}, {"fontSizeCaption",11},
        {"fontSizeExtraSmall",10}, {"tooltipFontSize",13}};
    for (auto it = sizes.cbegin(); it != sizes.cend(); ++it) {
        QVariant base = prefs.value(it.key(), theme.value(it.key(), it.value()));
        if (it.key() == "fontSizeBody" && !theme.contains(it.key()))
            base = prefs.value("fontSizeNormal", theme.value("fontSizeNormal", it.value()));
        result.insert(it.key(), qMax(1, qRound(base.toDouble() * scale)));
    }
    const auto themeRoles = validatedRoles(theme.value("typographyRoles").toMap());
    const auto userRoles = prefs.value("roles").toMap();
    const QStringList names = roleNames();
    const QList<int> defaults{24,18,14,13,11,13};
    const QStringList legacySizes{"fontSizeHeader", "fontSizeTitle", "fontSizeBody", "fontSizeLarge", "fontSizeCaption", ""};
    const QStringList legacyFallbacks{"fontSizeHero", "fontSizeSubtitle", "fontSizeNormal", "tooltipFontSize", "fontSizeSmall", ""};
    const QList<int> weights{QFont::DemiBold,QFont::Medium,QFont::Normal,QFont::Medium,QFont::Normal,QFont::Normal};
    QVariantMap roles;
    for (qsizetype i = 0; i < names.size(); ++i) {
        const QString name = names[i];
        const auto themed = themeRoles.value(name).toMap();
        const auto custom = userRoles.value(name).toMap();
        QString family = i <= 1 ? display : i == 2 ? body : i == 5 ? mono : primary;
        const bool userFamily = i == 5 ? prefs.contains("monoFontFamily")
            : prefs.contains("fontFamily") || (i <= 1 ? prefs.contains("fontFamilyDisplay") : i == 2 && prefs.contains("fontFamilyText"));
        if (!userFamily)
            family = themed.value("family", family).toString();
        family = custom.value("family", family).toString();
        double size = themed.value("pixelSize", theme.value(legacySizes[i], theme.value(legacyFallbacks[i], defaults[i]))).toDouble();
        if (i == 1 && prefs.contains("fontSizeTitle")) size = prefs.value("fontSizeTitle").toDouble();
        if (i == 2 && prefs.contains("fontSizeNormal")) size = prefs.value("fontSizeNormal").toDouble();
        size = custom.value("pixelSize", size).toDouble();
        QVariantMap role{{"family", family}, {"pixelSize", qMax(1,qRound(size * scale))},
            {"weight", custom.value("weight", themed.value("weight", weights[i]))},
            {"letterSpacing", custom.value("letterSpacing", themed.value("letterSpacing", 0.0))},
            {"lineHeight", custom.value("lineHeight", themed.value("lineHeight", i == 2 ? 1.4 : 1.2))}};
        roles.insert(name, role);
    }
    const QMap<QString, QStringList> aliases{
        {"display", {"fontSizeHeader", "fontSizeHero"}}, {"title", {"fontSizeTitle", "fontSizeSubtitle"}},
        {"body", {"fontSizeBody", "fontSizeNormal"}}, {"label", {"fontSizeLarge", "tooltipFontSize"}},
        {"caption", {"fontSizeCaption", "fontSizeSmall", "fontSizeExtraSmall"}}};
    for (auto it = aliases.cbegin(); it != aliases.cend(); ++it) {
        const auto themed = themeRoles.value(it.key()).toMap();
        const auto custom = userRoles.value(it.key()).toMap();
        if (themed.contains("pixelSize") || custom.contains("pixelSize"))
            for (const auto &key : it.value()) result.insert(key, roles.value(it.key()).toMap().value("pixelSize"));
    }
    result.insert("fontFamilyDisplay", roles.value("display").toMap().value("family"));
    result.insert("fontFamilyText", roles.value("body").toMap().value("family"));
    result.insert("monoFontFamily", roles.value("mono").toMap().value("family"));
    result.insert("fontWeightNormal", roles.value("body").toMap().value("weight"));
    result.insert("fontWeightMedium", roles.value("label").toMap().value("weight"));
    result.insert("fontWeightDemiBold", roles.value("title").toMap().value("weight"));
    result.insert("typographyRoles", roles);
    return result;
}

bool patchPreferences(const QString &path, const QVariantMap &legacy, const QString &field,
                      const QVariant &value, bool reset, QString *error)
{
    const auto fail = [&](const QString &message) { *error = message; return false; };
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return fail("Cannot create typography preferences directory.");
    const int fd = ::open(QFile::encodeName(path + ".lock").constData(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (fd < 0) return fail("Cannot lock typography preferences.");
    struct Lock { int fd; ~Lock() { ::flock(fd, LOCK_UN); ::close(fd); } } lock{fd};
    int rc;
    do { rc = ::flock(fd, LOCK_EX); } while (rc < 0 && errno == EINTR);
    if (rc < 0) return fail("Cannot lock typography preferences.");
    QJsonObject document;
    QVariantMap prefs = legacy;
    if (QFileInfo::exists(path)) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return fail("Cannot read typography preferences.");
        QJsonParseError parse;
        const auto json = QJsonDocument::fromJson(file.readAll(), &parse);
        if (parse.error != QJsonParseError::NoError || !json.isObject())
            return fail("Typography preferences contain invalid JSON.");
        document = json.object();
        if (document.value("schema").toInt() != 1 || !document.value("preferences").isObject())
            return fail("Unsupported typography preferences format.");
        prefs = document.value("preferences").toObject().toVariantMap();
    }
    if (reset) {
        // Preserve extension keys, removing only fields owned by this typography contract.
        for (const auto &key : familyKeys) prefs.remove(key);
        for (const auto &key : {"fontSizeNormal", "fontSizeTitle", "textScale"}) prefs.remove(key);
        auto roles = prefs.value("roles").toMap();
        for (const auto &name : roleNames()) {
            auto role = roles.value(name).toMap();
            for (const auto &key : {"family", "pixelSize", "weight", "letterSpacing", "lineHeight"}) role.remove(key);
            if (role.isEmpty()) roles.remove(name); else roles.insert(name, role);
        }
        if (roles.isEmpty()) prefs.remove("roles"); else prefs.insert("roles", roles);
    } else if (field.contains('.')) {
        auto roles = prefs.value("roles").toMap();
        auto role = roles.value(field.section('.',0,0)).toMap();
        if (value.isValid()) role.insert(field.section('.',1), value); else role.remove(field.section('.',1));
        if (role.isEmpty()) roles.remove(field.section('.',0,0)); else roles.insert(field.section('.',0,0), role);
        if (roles.isEmpty()) prefs.remove("roles"); else prefs.insert("roles", roles);
    } else if (value.isValid()) prefs.insert(field, value);
    else prefs.remove(field);
    document.insert("schema", 1);
    document.insert("preferences", QJsonObject::fromVariantMap(prefs));
    QSaveFile output(path);
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) return fail("Cannot write typography preferences.");
    const auto bytes = QJsonDocument(document).toJson(QJsonDocument::Indented);
    if (output.write(bytes) != bytes.size() || !output.commit())
        return fail("Cannot save typography preferences.");
    error->clear();
    return true;
}
} // namespace Astrea::Typography
