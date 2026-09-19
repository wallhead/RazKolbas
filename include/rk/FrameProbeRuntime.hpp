#pragma once
#include <Windows.h>
#include <dxgi.h>
namespace rk {
// Called only after the creation observer has verified the exact game identity.
void armFrameProbe(HMODULE verifiedGame);
void probePresentCandidates(IDXGISwapChain* swap);
}
