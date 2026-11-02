#pragma once

#include <QFont>
#include <QGuiApplication>
#include <QQuickWindow>

// Establish one rendering policy before creating any Settings QML windows.
// The desktop's Fontconfig configuration still controls grayscale antialiasing;
// this deliberately does not override system LCD/subpixel settings.
inline void applySettingsFontPolicy(QGuiApplication &app)
{
    QFont font = app.font();
    // Match faces by family/weight rather than a platform-pinned style name.
    font.setStyleName(QString());
    font.setHintingPreference(QFont::PreferVerticalHinting);
    font.setKerning(true);
    app.setFont(font);

    // Raw QtQuick Text and QtQuick Controls now share the native renderer.
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
}
