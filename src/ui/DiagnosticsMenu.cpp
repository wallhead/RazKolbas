#include "rk/DiagnosticsMenu.hpp"
#include "rk/D3D11StateScope.hpp"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <mutex>

namespace rk {
namespace {
using Microsoft::WRL::ComPtr;
struct MenuState {
    std::mutex mutex;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ImGuiContext* imgui{};
    ULONGLONG lastFrameTick{};
    int hotkey{VK_END};
    const char* hotkeyName{"End"};
    float fontScale{1.0f};
    bool enabled{};
    bool visible{},endWasDown{},failed{};
    std::uint64_t visibleFrames{};
};
MenuState& menu() {
    // The plugin and swap observer are pinned for the process lifetime.
    static auto* state=new MenuState;
    return *state;
}
bool initialize(MenuState& state,ID3D11Device* device,
    ID3D11DeviceContext* context) {
    if(state.imgui)return state.device.Get()==device&&state.context.Get()==context;
    state.imgui=ImGui::CreateContext();
    if(!state.imgui)return false;
    ImGui::SetCurrentContext(state.imgui);
    ImGui::StyleColorsDark();
    if(!ImGui_ImplDX11_Init(device,context))return false;
    state.device=device;state.context=context;
    return true;
}
void updateMouse(HWND window,ImGuiIO& io) {
    POINT cursor{};
    if(GetCursorPos(&cursor)&&ScreenToClient(window,&cursor))
        io.AddMousePosEvent(static_cast<float>(cursor.x),static_cast<float>(cursor.y));
    else io.AddMousePosEvent(-3.4e38f,-3.4e38f);
    io.AddMouseButtonEvent(0,(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0);
    io.AddMouseButtonEvent(1,(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0);
}
void drawStatus(const DiagnosticsSnapshot& status,UINT width,UINT height,
    const char* hotkeyName) {
    const char* mode=status.mode==DisplayMode::Dlaa?"DLAA":
        status.mode==DisplayMode::DlssSr?"DLSS Super Resolution":
        status.mode==DisplayMode::SpatialFallback?"Spatial upscaling":"Native";
    ImGui::SetNextWindowPos(ImVec2(32,32),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(370,0),ImGuiCond_FirstUseEver);
    if(ImGui::Begin("RazKolbas  |  Diagnostics",nullptr,
        ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysAutoResize|
        ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextDisabled("%s to hide",hotkeyName);
        ImGui::Text("Effective frame: %s",mode);
        ImGui::Separator();
        ImGui::Text("DLAA: %s",status.mode==DisplayMode::Dlaa?"On":"Off");
        ImGui::Text("DLSS Super Resolution: %s",
            status.mode==DisplayMode::DlssSr?"On":"Off");
        ImGui::Text("Reduced scene source: %s",status.srSourceReady?
            "ready":status.ownedSceneActive?"active; guides pending":
            status.srRequested?"awaiting same-frame proof":"not requested");
        ImGui::Text("Native display: %u x %u",width,height);
        if(status.engineDrsKnown) {
            ImGui::Text("%s: %u x %u",status.ownedSceneActive?
                "Owned render target":"Skyrim DRS target",
                status.renderWidth,status.renderHeight);
            ImGui::Text("%s: %.1f%% x %.1f%%",status.ownedSceneActive?
                "Owned scale":"Skyrim scale",
                width?100.0f*static_cast<float>(status.renderWidth)/width:0.0f,
                height?100.0f*static_cast<float>(status.renderHeight)/height:0.0f);
        } else ImGui::TextDisabled("Skyrim DRS target: unavailable");
        if(status.dlaaSuspendedByDrs)
            ImGui::TextColored(ImVec4(1.0f,0.6f,0.3f,1.0f),
                "RazKolbas DLAA suspended by Skyrim DRS transition");
        ImGui::Separator();
        ImGui::Text("World frames: %llu",
            static_cast<unsigned long long>(status.worldFrames));
        ImGui::Text("DLSS submissions: %llu",
            static_cast<unsigned long long>(status.dlssFrames));
        ImGui::Text("SR fallback frames: %llu",
            static_cast<unsigned long long>(status.skippedFrames));
        if(status.dlssDisabled)ImGui::TextColored(ImVec4(1.0f,0.6f,0.3f,1.0f),
            "DLSS disabled for this session");
        ImGui::Separator();
        ImGui::TextDisabled("Skyrim TAA: %s",
            status.skyrimTaaActive?"still enabled":"off");
    }
    ImGui::End();
}
}

void configureDiagnosticsMenu(bool enabled,std::string_view key,
    double fontScale) noexcept {
    auto& state=menu();
    std::scoped_lock guard(state.mutex);
    const struct { std::string_view name; int vk; const char* label; } keys[]{
        {"End",VK_END,"End"},{"Home",VK_HOME,"Home"},
        {"Insert",VK_INSERT,"Insert"},{"F10",VK_F10,"F10"}};
    state.enabled=false;
    for(const auto& choice:keys)if(key==choice.name) {
        state.hotkey=choice.vk;state.hotkeyName=choice.label;
        state.enabled=enabled;break;
    }
    if(fontScale>=0.5&&fontScale<=4.0)state.fontScale=static_cast<float>(fontScale);
}

void drawDiagnosticsMenu(IDXGISwapChain* swap,
    const DiagnosticsSnapshot& snapshot) noexcept {
    if(!swap)return;
    auto& state=menu();
    std::unique_lock guard(state.mutex,std::try_to_lock);
    if(!guard||state.failed||!state.enabled)return;
    try {
        DXGI_SWAP_CHAIN_DESC swapDesc{};
        if(FAILED(swap->GetDesc(&swapDesc))||!swapDesc.OutputWindow)return;
        const bool focused=GetForegroundWindow()==swapDesc.OutputWindow;
        const bool endDown=focused&&(GetAsyncKeyState(state.hotkey)&0x8000)!=0;
        if(endDown&&!state.endWasDown) {
            state.visible=!state.visible;
            state.visibleFrames=0;
            spdlog::info("Diagnostics menu {} by {} key",
                state.visible?"opened":"closed",state.hotkeyName);
        }
        state.endWasDown=endDown;
        if(!state.visible||!focused) {
            if(state.imgui) {
                ImGui::SetCurrentContext(state.imgui);
                ImGui::GetIO().MouseDrawCursor=false;
            }
            return;
        }
        ComPtr<ID3D11Device> device;
        if(FAILED(swap->GetDevice(IID_PPV_ARGS(&device)))||!device)return;
        ComPtr<ID3D11DeviceContext> context;
        device->GetImmediateContext(&context);
        if(!context)return;
        ComPtr<ID3D11Texture2D> backbuffer;
        if(FAILED(swap->GetBuffer(0,IID_PPV_ARGS(&backbuffer)))||!backbuffer)return;
        D3D11_TEXTURE2D_DESC backDesc{};backbuffer->GetDesc(&backDesc);
        if(!backDesc.Width||!backDesc.Height||
           backDesc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM)return;
        ComPtr<ID3D11RenderTargetView> target;
        if(FAILED(device->CreateRenderTargetView(backbuffer.Get(),nullptr,&target)))return;
        if(!initialize(state,device.Get(),context.Get())) {
            state.failed=true;
            spdlog::warn("Diagnostics menu disabled: D3D11/ImGui initialization failed");
            return;
        }
        auto isolated=D3D11StateScope::begin(context.Get());
        if(const auto error=std::get_if<Error>(&isolated)) {
            state.failed=true;
            spdlog::warn("Diagnostics menu disabled: {}",error->message);
            return;
        }
        auto scope=std::move(std::get<std::unique_ptr<D3D11StateScope>>(isolated));
        ImGui::SetCurrentContext(state.imgui);
        auto& io=ImGui::GetIO();
        io.FontGlobalScale=state.fontScale;
        // Skyrim's visible cursor is drawn by the game before this overlay.
        // ImGui must draw its own cursor above the diagnostics window.
        io.MouseDrawCursor=true;
        io.DisplaySize=ImVec2(static_cast<float>(backDesc.Width),
            static_cast<float>(backDesc.Height));
        const auto now=GetTickCount64();
        io.DeltaTime=state.lastFrameTick?
            std::clamp(static_cast<float>(now-state.lastFrameTick)/1000.0f,
                1.0f/1000.0f,0.1f):1.0f/60.0f;
        state.lastFrameTick=now;
        updateMouse(swapDesc.OutputWindow,io);
        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();
        drawStatus(snapshot,backDesc.Width,backDesc.Height,state.hotkeyName);
        ImGui::Render();
        ++state.visibleFrames;
        if(state.visibleFrames<=3||state.visibleFrames%600==0)
            spdlog::info("Diagnostics mouse: frame={} pos=({:.0f},{:.0f}) left={} capture={}",
                state.visibleFrames,io.MousePos.x,io.MousePos.y,io.MouseDown[0],
                io.WantCaptureMouse);
        context->OMSetRenderTargets(1,target.GetAddressOf(),nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    } catch(const std::exception& error) {
        state.failed=true;
        try { spdlog::warn("Diagnostics menu disabled: {}",error.what()); } catch(...) {}
    } catch(...) {
        state.failed=true;
        try { spdlog::warn("Diagnostics menu disabled after an unexpected error"); } catch(...) {}
    }
}
}
