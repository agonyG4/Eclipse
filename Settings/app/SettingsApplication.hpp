#pragma once

#include <QObject>
#include <memory>

class AstreaIconProvider;
class QGuiApplication;
class QQmlApplicationEngine;
class SettingsController;

class SettingsApplication final : public QObject {
    Q_OBJECT

public:
    explicit SettingsApplication(QGuiApplication &app);
    ~SettingsApplication() override;
    int run();

private:
    bool initializeQml();

    QGuiApplication &m_app;
    std::unique_ptr<SettingsController> m_controller;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    AstreaIconProvider *m_iconProvider = nullptr;
};
