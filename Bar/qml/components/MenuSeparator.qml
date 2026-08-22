import QtQuick

Rectangle {
    objectName: "menuSeparator"
    ShellBarTheme { id: theme }

    width: parent ? parent.width - 16 : 0
    height: 1
    color: theme.shellSeparator
    anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
}
