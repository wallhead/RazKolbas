#pragma once
#include <Windows.h>
#include <dxgi.h>
#include "rk/RendererHook.hpp"
namespace rk {
// Called only after the creation observer has verified the exact game identity.
void armFrameProbe(HMODULE verifiedGame);
// Numeric anchors only; call after successful captureRendererSnapshot validation.
void bindFrameProbe(const DeviceCreationArgs& args);
void probePresentCandidates(IDXGISwapChain* swap);
}
