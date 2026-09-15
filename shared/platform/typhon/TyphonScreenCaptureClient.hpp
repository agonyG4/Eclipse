#pragma once

#include <QImage>
#include <QObject>

#include <cstdint>
#include <memory>

class TyphonSharedConnection;

class TyphonScreenCaptureClient final : public QObject {
    Q_OBJECT

public:
    explicit TyphonScreenCaptureClient(TyphonSharedConnection *sharedConnection,
                                       QObject *parent = nullptr);
    ~TyphonScreenCaptureClient() override;

    void start();
    void stop();
    bool requestCapture();
    void cancelCapture();
    bool isReady() const;
    std::uint64_t connectionGeneration() const;

signals:
    void ready(QImage image, std::uint64_t generation);
    void failed(QString reason, std::uint64_t generation);
    void stateChanged(QString state);
    void diagnostic(QString message);

private:
    struct Private;
    std::unique_ptr<Private> m_private;
};
