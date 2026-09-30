#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace rk {
struct FgCameraWrite {
    std::array<std::uint8_t,720> bytes{};
    std::uint64_t revision{},buffer{},generation{},thread{},mapCaller{},unmapCaller{};
};
bool sameFgCameraWrite(const FgCameraWrite& a,const FgCameraWrite& b) noexcept;
bool canCompareFgCameraWriteHistory(const FgCameraWrite& prior,const FgCameraWrite& next) noexcept;

// Observes an already verified immediate context/constant buffer. The caller
// serializes access and supplies only a successful, descriptor-checked Map's
// pointer. Copy occurs BEFORE the matching native Unmap, never after it.
// A revision proves an observed write, not a game frame or an SL token.
class FgCameraWriteCapture {
public:
    explicit FgCameraWriteCapture(std::uint64_t context) noexcept : context_(context) {}
    void selectBuffer(std::uint64_t buffer) noexcept;
    bool mapped(std::uint64_t context,std::uint64_t buffer,std::uint64_t thread,
        std::uint64_t caller,const void* bytes,std::size_t size,bool succeeded) noexcept;
    bool beforeUnmap(std::uint64_t context,std::uint64_t buffer,
        std::uint64_t thread,std::uint64_t caller) noexcept;
    std::optional<FgCameraWrite> latest() const noexcept {return latest_;}
private:
    std::uint64_t context_{},buffer_{},revision_{},generation_{},thread_{},caller_{};
    const void* mapped_{};
    std::optional<FgCameraWrite> latest_;
};
}
