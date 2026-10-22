#pragma once
#include <QFutureWatcher>
#include <QObject>
#include <QThreadPool>
#include <QVariantMap>
#include <functional>
#include <stdexcept>

namespace NativeSettings
{
struct CommandResult {
    int code;
    QString out;
    QString error;
};
using Runner = std::function<CommandResult(const QString &, const QStringList &, int)>;
CommandResult run(const QString &program, const QStringList &args, int timeout = 10000);
QString require(const QString &program, const QStringList &args, int timeout = 10000);
[[noreturn]] void fail(const QString &message);
QString configPath();
QVariantMap readObject(const QString &path);
void patchObject(const QString &path, const std::function<void(QVariantMap &)> &patch);
} // namespace NativeSettings

class SettingsAsyncController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged)
  public:
    explicit SettingsAsyncController(QObject *parent = nullptr);
    ~SettingsAsyncController() override;
    QVariantMap state() const { return m_state; }
    bool busy() const { return m_busy; }
    QString lastError() const { return m_error; }
  signals:
    void stateChanged();
    void busyChanged();
    void errorChanged();
    void operationSucceeded();

  protected:
    void submit(std::function<QVariantMap()> job, bool mutation = false);
    void report(const QString &error);

  private:
    struct Result {
        QVariantMap state;
        QString error;
    };
    QVariantMap m_state;
    QString m_error;
    bool m_busy = false;
    bool m_mutation = false;
    QThreadPool m_pool;
    QFutureWatcher<Result> m_watcher;
};
