#include "app/SettingsFontPolicy.hpp"

#include <QGuiApplication>
#include <QScopeGuard>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QtTest>

#include <memory>

class SettingsFontPolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void configuresNativeRenderingAndFontDefaults()
    {
        QGuiApplication *app = qGuiApp;
        QVERIFY(app != nullptr);
        const QFont previousFont = app->font();
        const auto previousRenderer = QQuickWindow::textRenderType();

        const auto restore=qScopeGuard([app,previousFont,previousRenderer] {
            QQuickWindow::setTextRenderType(previousRenderer);
            app->setFont(previousFont);
        });
        QFont pinnedFont=previousFont;
        pinnedFont.setStyleName(QStringLiteral("Regular"));
        app->setFont(pinnedFont);
        applySettingsFontPolicy(*app);
        QVERIFY(app->font().styleName().isEmpty());
        QCOMPARE(app->font().family(),previousFont.family());
        QCOMPARE(QQuickWindow::textRenderType(), QQuickWindow::NativeTextRendering);
        QCOMPARE(app->font().hintingPreference(), QFont::PreferVerticalHinting);
        QVERIFY(app->font().kerning());

        QQmlEngine engine;
        QQmlComponent textComponent(&engine);
        textComponent.setData(R"(
            import QtQuick
            Text {
                text: "Astrea Settings"
                objectName: "fontPolicyText"
            }
        )", QUrl());
        QVERIFY2(textComponent.isReady(), qPrintable(textComponent.errorString()));
        std::unique_ptr<QObject> text(textComponent.create());
        QVERIFY2(text != nullptr, qPrintable(textComponent.errorString()));
        QCOMPARE(text->property("renderType").toInt(), int(QQuickWindow::NativeTextRendering));

        const QFont textFont = text->property("font").value<QFont>();
        QCOMPARE(textFont.hintingPreference(), QFont::PreferVerticalHinting);
        QVERIFY(textFont.kerning());

    }
};

QTEST_MAIN(SettingsFontPolicyTest)
#include "SettingsFontPolicyTest.moc"
