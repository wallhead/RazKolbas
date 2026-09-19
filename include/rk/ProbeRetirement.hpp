#pragma once
#include "rk/Result.hpp"
#include <cstdint>
#include <functional>
namespace rk {
Result<bool> retireProbeRuntime(const std::function<std::uint32_t()>& shutdown,
                               const std::function<Result<bool>()>& restore,
                               const std::function<void()>& unload);
}
