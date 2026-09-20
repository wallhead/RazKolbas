#pragma once
#include "rk/OwnedSceneDomain.hpp"
#include <Windows.h>

namespace rk {
// Body of the single verified Renderer::Begin GetClientRect replacement.
// The caller must establish the exact callsite/previous target and retain
// this controller for as long as its callback is reachable.
class RendererLogicalSize final {
public:
    using Prior=BOOL(WINAPI*)(HWND,RECT*);
    RendererLogicalSize(OwnedSceneDomain& route,Prior prior,HWND gameWindow,
        DWORD renderThread) noexcept:
        route_(route),prior_(prior),gameWindow_(gameWindow),renderThread_(renderThread) {}
    BOOL query(HWND window,RECT* rect) const noexcept;
private:
    OwnedSceneDomain& route_;
    Prior prior_{};
    HWND gameWindow_{};
    DWORD renderThread_{};
};
}
