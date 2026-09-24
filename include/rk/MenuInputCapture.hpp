#pragma once
#include <optional>

namespace rk {
class MenuInputCapture {
public:
    std::optional<bool> update(bool shouldCapture,bool currentlyIgnored) noexcept;
    [[nodiscard]] bool active() const noexcept;

private:
    bool active_{};
    bool restoreIgnored_{};
};
}
