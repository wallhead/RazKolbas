#pragma once
#include <atomic>
#include <cstdint>
#include <stdexcept>
namespace rk {
struct Extent { std::uint32_t width{}, height{}; bool valid() const { return width > 0 && height > 0; } };
class HistoryEpoch {
public:
    std::uint64_t request();
    std::uint64_t capture() const;
    void consume(std::uint64_t captured, bool success);
    bool pending() const;
private:
    std::atomic<std::uint64_t> requested_{1}, consumed_{0};
};
class FrameIdentity {
public:
    std::uint64_t beginSource();
    bool presentGenerated(std::uint64_t source);
    std::uint64_t source() const { return source_; }
    std::uint64_t generated() const { return generated_; }
private:
    std::uint64_t source_{}, generated_{};
};
}
