#pragma once

namespace Astrea::PaperProtocol {

inline constexpr int kTransportDeadlineMs = 1000;
inline constexpr int kOperationDeadlineMs = 5000;
inline constexpr int kClientOperationMarginMs = 1000;
inline constexpr int kClientOperationDeadlineMs =
    kOperationDeadlineMs + kClientOperationMarginMs;

} // namespace Astrea::PaperProtocol
