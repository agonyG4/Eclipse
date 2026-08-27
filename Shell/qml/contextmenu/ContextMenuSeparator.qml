import QtQuick
import "../../Bar/qml/components"

Rectangle {
    id: root
    ShellBarTheme { id: theme }
    height: 1
    color: theme.shellSeparator
    opacity: 0.8
}
