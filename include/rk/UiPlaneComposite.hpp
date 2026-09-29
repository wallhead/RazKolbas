#pragma once
#include "rk/Result.hpp"
#include <d3d11.h>

namespace rk {
// Compose a premultiplied, display-sized RGBA8 UI plane onto the already
// published native colour without changing its alpha or caller graphics state.
Result<bool> compositePremultipliedUi(ID3D11DeviceContext* context,
    ID3D11Texture2D* uiPlane,ID3D11RenderTargetView* finalView);
}
