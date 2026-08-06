#pragma once

#include <QObject>
#include <QString>
#include <memory>

struct wl_display;
class TyphonProtocolAdapter;

class TyphonWaylandDisplay final : public QObject {
    Q_OBJECT

public:
    explicit TyphonWaylandDisplay(QObject *parent = nullptr);
    ~TyphonWaylandDisplay() override;

    bool connectToDisplay(const QString &displayName = {});
    void disconnectFromDisplay();
    int fileDescriptor() const;
    bool dispatchPending();
    bool flush();
    bool isConnected() const;
    wl_display *nativeDisplay() const;

signals:
    void connected();
    void readable();
    void disconnected();
    void protocolError(QString diagnostic);

private:
    struct Private;
    void armRead();
    void onReadable();
    void onWritable();
    void fail(const QString &diagnostic);

    std::unique_ptr<Private> m_private;
    friend class TyphonProtocolAdapter;
};
