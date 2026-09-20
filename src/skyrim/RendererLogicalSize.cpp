#include "rk/RendererLogicalSize.hpp"

namespace rk {
BOOL RendererLogicalSize::query(HWND window,RECT* rect) const noexcept {
    if(!prior_)return FALSE;
    const auto result=prior_(window,rect);
    const auto owner=route_.renderThread()?route_.renderThread():renderThread_;
    if(!result||!rect||window!=gameWindow_||
       GetCurrentThreadId()!=owner||route_.phase()!=ScenePhase::World)return result;
    const auto& plan=route_.plan();
    if(!plan.valid()||rect->left!=0||rect->top!=0||
       rect->right!=static_cast<LONG>(plan.display.width)||
       rect->bottom!=static_cast<LONG>(plan.display.height))return result;
    rect->right=static_cast<LONG>(plan.render.width);
    rect->bottom=static_cast<LONG>(plan.render.height);
    return result;
}
}
