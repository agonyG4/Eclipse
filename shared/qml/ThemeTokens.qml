import QtQuick

QtObject {
    property var state: typeof ThemeController === "undefined" ? null : ThemeController
    readonly property var overrides: state && state.styleTokens ? state.styleTokens : ({})

    function value(name, fallback) {
        const current = overrides
        return current && Object.prototype.hasOwnProperty.call(current, name)
            && current[name] !== undefined ? current[name] : fallback
    }
}
