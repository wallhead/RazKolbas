#pragma once
#include <cstdint>
#include <optional>

namespace rk {
enum class MenuInputRuntime : std::uint8_t {
    SkyrimSe1597,
    SkyrimAe161170
};

struct MenuInputProfile {
    std::uintptr_t controlMapSingletonRva{};
    std::uintptr_t ignoreKeyboardMouseOffset{};
};

[[nodiscard]] MenuInputProfile menuInputProfile(MenuInputRuntime runtime) noexcept;

class MenuInputCapture {
public:
    std::optional<bool> update(bool shouldCapture,bool currentlyIgnored) noexcept;
    [[nodiscard]] bool active() const noexcept;

private:
    bool active_{};
    bool restoreIgnored_{};
};
}
