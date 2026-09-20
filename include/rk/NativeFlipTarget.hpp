#pragma once
#include "rk/FrameContracts.hpp"
#include "rk/Result.hpp"
#include <d3d11.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

namespace rk {
struct NativeFlipTarget {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view;
    UINT index{};
};
// Resolve the current native flip buffer through the caller's existing swap
// wrapper. Every invocation reacquires the index; the caller must retire the
// returned references before ResizeBuffers.
Result<NativeFlipTarget> acquireNativeFlipTarget(IDXGISwapChain* swap,
    ID3D11Device* device,Extent display);
}
