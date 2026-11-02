import QtQuick

// Compatibility adapter: app identity and initials feed the common presenter.
AstreaIcon {
    id: root
    property string iconSource: ""
    property string iconUrl: ""
    property string astreaIcon: ""
    property string astreaIconName: ""
    property string iconName: ""
    property string iconPath: ""
    property string icon: ""
    property string className: ""
    property string title: ""
    property string fallbackIconName: ""
    property string appName: ""
    property bool iconPending: false

    readonly property string effectiveIconName: {
        if (iconSource) return iconSource
        if (iconUrl) return iconUrl
        if (astreaIcon) return astreaIcon
        if (astreaIconName) return astreaIconName
        if (iconName) return iconName
        if (iconPath) return iconPath
        if (icon) return icon
        if (className) return className
        if (title && title !== appName) return title
        return fallbackIconName
    }

    readonly property string bestIconName: effectiveIconName
    readonly property string initials: {
        if (!appName) return ""
        var cleaned = appName.replace(/[-_.]/g, " ")
        var parts = cleaned.trim().split(/\s+/).filter(function(part) { return part.length > 0 })
        if (parts.length === 0) return ""
        if (parts.length === 1) return parts[0][0].toUpperCase()
        return (parts[0][0] + parts[1][0]).toUpperCase()
    }

    source: effectiveIconName
    fallbackText: initials
}
