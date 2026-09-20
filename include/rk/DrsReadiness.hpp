#pragma once

namespace rk {
// A native-resolution DLAA submission needs a native current and previous
// engine DRS ratio. This is only a ratio gate; guide extents are checked at
// the D3D11 source boundary.
bool nativeDlaaRatiosReady(float currentWidth,float currentHeight,
    float previousWidth,float previousHeight) noexcept;
}
