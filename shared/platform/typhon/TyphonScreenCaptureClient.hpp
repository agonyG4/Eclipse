#pragma once

#include <QImage>
#include <QObject>
#include <QSize>

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
    bool requestCapture(std::uint64_t operationToken);
    bool requestWindowCapture(const QString &windowId, std::uint32_t maxWidth,
                              std::uint32_t maxHeight, std::uint64_t operationToken);
    void cancelCapture();
    bool isReady() const;
    bool supportsWindowCapture() const;
    // Largest window capture edge the compositor accepts (4096 since v3).
    std::uint32_t maxWindowCaptureDimension() const;
    // The captured window's logical size; valid only while ready() is emitted
    // for a window capture from a v3 compositor, otherwise empty.
    QSize readyWindowSize() const;
    std::uint64_t connectionGeneration() const;

signals:
    void ready(QImage image, std::uint64_t generation, std::uint64_t operationToken);
    void failed(QString reason, std::uint64_t generation, std::uint64_t operationToken);
    void stateChanged(QString state);
    void diagnostic(QString message);

private:
    struct Private;
    std::unique_ptr<Private> m_private;
};
