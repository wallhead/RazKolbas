#pragma once
#include "rk/Result.hpp"
#include <cstdint>
#include <span>

namespace rk {
struct NgxJitter { float x{},y{}; };
// Decode the exact 1.6.1170 game camera fields observed after world draw.
// Projection-to-NGX signs follow the hash-verified reference callback contract.
Result<NgxJitter> ngxJitterFromGameCamera(std::span<const std::uint8_t> camera,
    std::uint32_t targetWidth,std::uint32_t targetHeight);
}
