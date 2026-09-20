#pragma once
#include "rk/Result.hpp"
#include <d3d11.h>

namespace rk {
// Copy one validated, display-sized SDR NGX result to the currently bound
// backbuffer. This does not change the caller's graphics bindings.
Result<bool> copySdrDisplayFrame(ID3D11DeviceContext* context,
    ID3D11Texture2D* backbuffer,ID3D11Texture2D* output);
}
