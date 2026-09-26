#include "rk/DiagnosticsMenu.hpp"
#include "rk/D3D11StateScope.hpp"
#include "rk/MenuInputCapture.hpp"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <string>

namespace rk {
namespace {
using Microsoft::WRL::ComPtr;
std::atomic<bool> inputDispatchCapture{};
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
    Settings activeSettings;
    Settings requestedSettings;
    std::filesystem::path iniPath;
    std::optional<SharpeningUpdate> pendingSharpening;
    std::optional<Settings> pendingNrRuntime;
    std::string saveMessage;
    bool controlsConfigured{},settingsDirty{};
    MenuInputCapture inputCapture;
    bool inputCaptureWarningLogged{};
    std::uintptr_t controlMapSingletonRva{};
    std::uintptr_t ignoreKeyboardMouseOffset{};
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
bool memoryRangeAvailable(const void* address,std::size_t size,bool writable) noexcept {
    if(!address||!size)return false;
    MEMORY_BASIC_INFORMATION info{};
    if(!VirtualQuery(address,&info,sizeof(info))||info.State!=MEM_COMMIT||
       (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    const auto start=reinterpret_cast<std::uintptr_t>(address);
    const auto regionEnd=reinterpret_cast<std::uintptr_t>(info.BaseAddress)+info.RegionSize;
    if(start>regionEnd||size>regionEnd-start)return false;
    if(!writable)return true;
    const auto access=info.Protect&0xff;
    return access==PAGE_READWRITE||access==PAGE_WRITECOPY||
        access==PAGE_EXECUTE_READWRITE||access==PAGE_EXECUTE_WRITECOPY;
}
std::uint8_t* resolveIgnoreKeyboardMouse(MenuState& state) noexcept {
    if(!state.controlMapSingletonRva||!state.ignoreKeyboardMouseOffset)return nullptr;
    const auto game=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(!game||state.controlMapSingletonRva>
       std::numeric_limits<std::uintptr_t>::max()-game)return nullptr;
    const auto slot=reinterpret_cast<const void*>(game+state.controlMapSingletonRva);
    if(!memoryRangeAvailable(slot,sizeof(void*),false))return nullptr;
    void* controls{};
    std::memcpy(&controls,slot,sizeof(controls));
    if(!controls)return nullptr;
    const auto flag=reinterpret_cast<std::uint8_t*>(controls)+
        state.ignoreKeyboardMouseOffset;
    return memoryRangeAvailable(flag,sizeof(*flag),true)?flag:nullptr;
}
void updateGameInputCapture(MenuState& state,bool shouldCapture) noexcept {
    inputDispatchCapture.store(shouldCapture,std::memory_order_release);
    try {
        auto* flag=resolveIgnoreKeyboardMouse(state);
        if(!flag) {
            if(shouldCapture&&!state.inputCaptureWarningLogged) {
                state.inputCaptureWarningLogged=true;
                spdlog::warn("Diagnostics mouse capture unavailable: Skyrim ControlMap flag was not resolved");
            }
            return;
        }
        const bool wasActive=state.inputCapture.active();
        const auto requested=state.inputCapture.update(
            shouldCapture,*flag!=0);
        if(requested)*flag=*requested?1u:0u;
        if(wasActive!=state.inputCapture.active())
            spdlog::info("Diagnostics mouse capture {} (restored state={})",
                state.inputCapture.active()?"enabled":"released",
                *flag!=0);
    } catch(...) {
        if(!state.inputCaptureWarningLogged) {
            state.inputCaptureWarningLogged=true;
            try { spdlog::warn("Diagnostics mouse capture unavailable after an unexpected error"); }
            catch(...) {}
        }
    }
}
bool persistSettings(MenuState& state) {
    if(!state.controlsConfigured||state.iniPath.empty())return false;
    const auto saved=saveIni(state.iniPath,state.requestedSettings);
    if(const auto error=std::get_if<Error>(&saved)) {
        state.saveMessage="Save failed: "+error->message;
        spdlog::warn("Diagnostics settings save failed: {}",error->message);
        return false;
    }
    state.settingsDirty=false;
    state.saveMessage="Saved to RazKolbas.ini";
    return true;
}
struct MenuChoice { const char* value;const char* label; };
const char* choiceLabel(std::string_view value,std::span<const MenuChoice> choices) {
    for(const auto& choice:choices)if(value==choice.value)return choice.label;
    return "Unknown";
}
bool choiceControl(const char* label,const char* key,
    std::span<const MenuChoice> choices,MenuState& state) {
    auto& current=state.requestedSettings.values.at(key);
    auto& selected=std::get<Choice>(current).value;
    bool changed=false;
    if(ImGui::BeginCombo(label,choiceLabel(selected,choices))) {
        for(const auto& choice:choices) {
            const bool active=selected==choice.value;
            if(ImGui::Selectable(choice.label,active)) {
                selected=choice.value;changed=true;
            }
            if(active)ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}
void drawControls(MenuState& state,const DiagnosticsSnapshot& status) {
    if(!state.controlsConfigured)return;
    static constexpr MenuChoice qualities[]{
        {"Performance","Performance"},{"Balanced","Balanced"},
        {"Quality","Quality"},{"UltraPerformance","Ultra Performance"},
        {"NativeAA","Native (DLAA)"}};
    static constexpr MenuChoice presets[]{
        {"Auto","Default (Auto)"},{"J","Preset J"},{"K","Preset K"},
        {"L","Preset L"},{"M","Preset M"}};
    bool restartChanged=false;
    ImGui::SetNextItemWidth(220.0f);
    restartChanged|=choiceControl("Quality Level","Upscaling.Quality",qualities,state);
    if(ImGui::IsItemHovered())
        ImGui::SetTooltip("Selects the DLSS input resolution.\nNative uses DLAA at display resolution.");
    ImGui::SetNextItemWidth(220.0f);
    restartChanged|=choiceControl("DLSS Preset","Upscaling.ModelPreset",presets,state);
    if(ImGui::IsItemHovered())
        ImGui::SetTooltip("Presets select a trained model and do not change render resolution.\nK: recommended for DLAA, Quality and Balanced\nJ: less ghosting, more flicker\nL: Ultra Performance default\nM: Performance default");
    if(restartChanged) {
        state.settingsDirty=true;
        persistSettings(state);
    }
    const auto& activeQuality=state.activeSettings.get<Choice>("Upscaling.Quality").value;
    const auto& activePreset=state.activeSettings.get<Choice>("Upscaling.ModelPreset").value;
    const auto& requestedQuality=state.requestedSettings.get<Choice>("Upscaling.Quality").value;
    const auto& requestedPreset=state.requestedSettings.get<Choice>("Upscaling.ModelPreset").value;
    ImGui::TextDisabled("Active: %s / %s",
        choiceLabel(activeQuality,qualities),choiceLabel(activePreset,presets));
    if(requestedQuality!=activeQuality||requestedPreset!=activePreset)
        ImGui::TextColored(ImVec4(1.0f,0.75f,0.25f,1.0f),
            "Quality or preset saved - restart Skyrim to apply");

    bool sharpening=state.requestedSettings.get<bool>("Upscaling.Sharpening");
    float sharpness=static_cast<float>(
        state.requestedSettings.get<double>("Upscaling.Sharpness"));
    ImGui::Spacing();
    if(ImGui::Checkbox("Enable Sharpening",&sharpening)) {
        state.requestedSettings.values["Upscaling.Sharpening"]=sharpening;
        state.pendingSharpening=SharpeningUpdate{sharpening,sharpness};
        state.settingsDirty=true;
        persistSettings(state);
    }
    ImGui::BeginDisabled(!sharpening);
    ImGui::SetNextItemWidth(220.0f);
    if(ImGui::SliderFloat("Sharpness",&sharpness,0.0f,1.0f,"%.2f")) {
        state.requestedSettings.values["Upscaling.Sharpness"]=
            static_cast<double>(sharpness);
        state.pendingSharpening=SharpeningUpdate{sharpening,sharpness};
        state.settingsDirty=true;
    }
    const bool sliderFinished=ImGui::IsItemDeactivatedAfterEdit();
    ImGui::EndDisabled();
    if(sliderFinished)persistSettings(state);
    ImGui::TextDisabled("Applied after DLSS, before native UI. Effective: %.2f",
        status.postSharpness);
    if(state.settingsDirty)ImGui::TextDisabled("Release the slider to save");
    else if(!state.saveMessage.empty())
        ImGui::TextDisabled("%s",state.saveMessage.c_str());
}
void drawUpscalingTab(MenuState& state,const DiagnosticsSnapshot& status,
    UINT width,UINT height,const char* mode) {
    ImGui::Text("Upscaler: NVIDIA DLSS");
    ImGui::Text("Current Mode: %s",mode);
    ImGui::Separator();
    ImGui::Text("Display Resolution: %u x %u",width,height);
    if(status.engineDrsKnown) {
        ImGui::Text("Render Resolution: %u x %u",status.renderWidth,status.renderHeight);
        const auto scaleX=width?100.0f*static_cast<float>(status.renderWidth)/width:0.0f;
        const auto scaleY=height?100.0f*static_cast<float>(status.renderHeight)/height:0.0f;
        ImGui::Text("Render Scale: %.1f%% x %.1f%%",scaleX,scaleY);
    } else ImGui::TextDisabled("Render Resolution: unavailable");
    ImGui::Spacing();
    drawControls(state,status);
}
void drawNeuralRenderingTab(MenuState& state) {
    if(!state.controlsConfigured)return;
    static constexpr MenuChoice presets[]{
        {"Auto","Auto (preset 0)"},{"Default","Default (preset 0)"},
        {"Shipping","Shipping (preset 1)"}};
    static constexpr MenuChoice resolves[]{
        {"Auto","Auto"},{"Residual","Residual"},{"Ratio","Ratio / OkLab"}};
    auto queueRuntime=[&] { state.pendingNrRuntime=state.requestedSettings; };
    auto save=[&](bool live=false) {
        if(live)queueRuntime();
        state.settingsDirty=true;
        persistSettings(state);
    };
    auto sliderFloat=[&](const char* label,const char* key,float minimum,
        float maximum,bool live,const char* format="%.2f") {
        float value=static_cast<float>(state.requestedSettings.get<double>(key));
        if(!ImGui::SliderFloat(label,&value,minimum,maximum,format))return;
        state.requestedSettings.values[key]=static_cast<double>(value);
        if(live)queueRuntime();
        state.settingsDirty=true;
        if(ImGui::IsItemDeactivatedAfterEdit())persistSettings(state);
    };

    ImGui::TextWrapped("Pipeline: HUD-free scene -> Neural Rendering -> DLSS SR -> optional frame generation -> native UI");
    ImGui::TextColored(ImVec4(1.0f,0.75f,0.25f,1.0f),
        "NR runtime: experimental exact-build direct path.");
    ImGui::TextDisabled("Evaluation controls apply live and reset NR history. Network preset requires restart.");
    ImGui::Separator();
    bool enabled=state.requestedSettings.get<bool>("NeuralRendering.Enabled");
    if(ImGui::Checkbox("Enable Neural Rendering",&enabled)) {
        state.requestedSettings.values["NeuralRendering.Enabled"]=enabled;save(true);
    }
    ImGui::TextDisabled("Position: Before DLSS Super Resolution / DLAA (fixed)");
    ImGui::SetNextItemWidth(220.0f);
    if(choiceControl("Network Preset","NeuralRendering.Preset",presets,state))save();
    const auto& activePreset=state.activeSettings.get<Choice>("NeuralRendering.Preset").value;
    const auto& requestedPreset=state.requestedSettings.get<Choice>(
        "NeuralRendering.Preset").value;
    if(requestedPreset!=activePreset)
        ImGui::TextColored(ImVec4(1.0f,0.75f,0.25f,1.0f),
            "Network preset saved - restart Skyrim to apply");
    int style=static_cast<int>(state.requestedSettings.get<std::int64_t>(
        "NeuralRendering.Style"));
    ImGui::SetNextItemWidth(220.0f);
    if(ImGui::SliderInt("Style",&style,0,7)) {
        state.requestedSettings.values["NeuralRendering.Style"]=
            static_cast<std::int64_t>(style);state.settingsDirty=true;
        queueRuntime();
        if(ImGui::IsItemDeactivatedAfterEdit())persistSettings(state);
    }
    sliderFloat("Intensity","NeuralRendering.Intensity",0.0f,2.0f,true);
    sliderFloat("Local Tone","NeuralRendering.LocalToneStrength",0.0f,2.0f,true);
    sliderFloat("Local Structure","NeuralRendering.LocalStructureStrength",0.0f,2.0f,true);

    auto skinText=state.requestedSettings.get<Text>(
        "NeuralRendering.SkinStructureStrength").value;
    bool autoSkin=skinText=="Auto"||skinText=="-1";
    if(ImGui::Checkbox("Auto Skin Mask Strength",&autoSkin)) {
        state.requestedSettings.values["NeuralRendering.SkinStructureStrength"]=
            Text{autoSkin?"Auto":"1.000000"};save(true);
    }
    if(!autoSkin) {
        float skin=std::strtof(skinText.c_str(),nullptr);
        ImGui::SetNextItemWidth(220.0f);
        if(ImGui::SliderFloat("Skin Structure",&skin,0.0f,2.0f,"%.2f")) {
            state.requestedSettings.values["NeuralRendering.SkinStructureStrength"]=
                Text{std::to_string(skin)};state.settingsDirty=true;
            queueRuntime();
            if(ImGui::IsItemDeactivatedAfterEdit())persistSettings(state);
        }
    }
    bool autoMask=state.requestedSettings.get<bool>("NeuralRendering.UseAutoMask");
    if(ImGui::Checkbox("Generate Skin Mask Automatically",&autoMask)) {
        state.requestedSettings.values["NeuralRendering.UseAutoMask"]=autoMask;save(true);
    }
    ImGui::BeginDisabled();
    bool uiCorrection=false;
    ImGui::Checkbox("Model UI Correction",&uiCorrection);
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("UI is composed at native resolution after NR/SR; model UI/alpha inputs are unavailable.");
    ImGui::TextDisabled("Passes: 1 (multi-pass is not validated in this build)");
    ImGui::BeginDisabled();
    bool hdr=state.requestedSettings.get<bool>("NeuralRendering.InputColorIsHDR");
    ImGui::Checkbox("NR Input Is HDR",&hdr);
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered())
        ImGui::SetTooltip("The current pre-SR NR input is SDR RGBA8.");

    ImGui::SeparatorText("Advanced resolve");
    double scale=state.requestedSettings.get<double>(
        "NeuralRendering.InputResolutionScale");
    bool fullResolution=scale==0.0||scale>=1.0;
    ImGui::BeginDisabled();
    ImGui::Checkbox("Full Resolution NR Input",&fullResolution);
    ImGui::EndDisabled();
    ImGui::TextDisabled("Reduced NR input and custom resolve controls are not active in the direct path.");
    ImGui::BeginDisabled();
    ImGui::SetNextItemWidth(220.0f);
    if(choiceControl("Resolve Method","NeuralRendering.Resolve",resolves,state))save();
    sliderFloat("Transfer Strength","NeuralRendering.TransferStrength",0.0f,2.0f,false);
    sliderFloat("Colour Strength","NeuralRendering.ColourStrength",0.0f,2.0f,false);
    sliderFloat("Maximum Ratio","NeuralRendering.MaxRatio",1.0f,16.0f,false);
    sliderFloat("White Point","NeuralRendering.WhitePoint",0.01f,16.0f,false);
    ImGui::EndDisabled();
}
void drawDiagnosticsTab(const DiagnosticsSnapshot& status,
    UINT width,UINT height,const char* mode) {
    ImGui::Text("Effective frame: %s",mode);
    ImGui::Text("DLAA: %s",status.mode==DisplayMode::Dlaa?"On":"Off");
    ImGui::Text("DLSS Super Resolution: %s",
        status.mode==DisplayMode::DlssSr?"On":"Off");
    ImGui::Text("Reduced scene source: %s",status.srSourceReady?
        "ready":status.ownedSceneActive?"active; guides pending":
        status.srRequested?"awaiting same-frame proof":"not requested");
    ImGui::Text("Native display: %u x %u",width,height);
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
void drawStatus(MenuState& state,const DiagnosticsSnapshot& status,
    UINT width,UINT height,const char* hotkeyName) {
    const char* mode=status.mode==DisplayMode::Dlaa?"DLAA":
        status.mode==DisplayMode::DlssSr?"DLSS Super Resolution":
        status.mode==DisplayMode::SpatialFallback?"Spatial upscaling":"Native";
    ImGui::SetNextWindowPos(ImVec2(32,32),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(470,0),ImGuiCond_FirstUseEver);
    if(ImGui::Begin("RazKolbas Upscaler",nullptr,
        ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysAutoResize|
        ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextDisabled("%s to hide",hotkeyName);
        if(ImGui::BeginTabBar("RazKolbas Tabs")) {
            if(ImGui::BeginTabItem("Upscaling")) {
                drawUpscalingTab(state,status,width,height,mode);
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Neural Rendering")) {
                drawNeuralRenderingTab(state);
                ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Diagnostics")) {
                drawDiagnosticsTab(status,width,height,mode);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
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

void configureDiagnosticsMenu(bool enabled,std::string_view key,double fontScale,
    const Settings& settings,const std::filesystem::path& iniPath,
    std::uintptr_t controlMapSingletonRva,
    std::uintptr_t ignoreKeyboardMouseOffset) noexcept {
    configureDiagnosticsMenu(enabled,key,fontScale);
    auto& state=menu();
    std::scoped_lock guard(state.mutex);
    state.activeSettings=settings;
    state.requestedSettings=settings;
    state.iniPath=iniPath;
    state.controlMapSingletonRva=controlMapSingletonRva;
    state.ignoreKeyboardMouseOffset=ignoreKeyboardMouseOffset;
    state.controlsConfigured=true;
    state.settingsDirty=false;
    state.saveMessage.clear();
}

std::optional<SharpeningUpdate> consumeDiagnosticsSharpeningUpdate() noexcept {
    auto& state=menu();
    std::scoped_lock guard(state.mutex);
    auto update=state.pendingSharpening;
    state.pendingSharpening.reset();
    return update;
}

std::optional<Settings> consumeDiagnosticsNrRuntimeUpdate() {
    auto& state=menu();
    std::scoped_lock guard(state.mutex);
    auto update=std::move(state.pendingNrRuntime);
    state.pendingNrRuntime.reset();
    return update;
}

bool diagnosticsMenuCapturingInput() noexcept {
    return inputDispatchCapture.load(std::memory_order_acquire);
}

void drawDiagnosticsMenu(IDXGISwapChain* swap,
    const DiagnosticsSnapshot& snapshot) noexcept {
    auto& state=menu();
    std::unique_lock guard(state.mutex,std::try_to_lock);
    if(!guard)return;
    if(!swap||state.failed||!state.enabled) {
        updateGameInputCapture(state,false);
        return;
    }
    try {
        DXGI_SWAP_CHAIN_DESC swapDesc{};
        if(FAILED(swap->GetDesc(&swapDesc))||!swapDesc.OutputWindow) {
            updateGameInputCapture(state,false);
            return;
        }
        const bool focused=GetForegroundWindow()==swapDesc.OutputWindow;
        const bool endDown=focused&&(GetAsyncKeyState(state.hotkey)&0x8000)!=0;
        if(endDown&&!state.endWasDown) {
            if(state.visible&&state.settingsDirty)persistSettings(state);
            state.visible=!state.visible;
            state.visibleFrames=0;
            spdlog::info("Diagnostics menu {} by {} key",
                state.visible?"opened":"closed",state.hotkeyName);
        }
        state.endWasDown=endDown;
        updateGameInputCapture(state,state.visible&&focused);
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
        drawStatus(state,snapshot,backDesc.Width,backDesc.Height,state.hotkeyName);
        ImGui::Render();
        ++state.visibleFrames;
        if(state.visibleFrames<=3||state.visibleFrames%600==0)
            spdlog::info("Diagnostics mouse: frame={} pos=({:.0f},{:.0f}) left={} capture={}",
                state.visibleFrames,io.MousePos.x,io.MousePos.y,io.MouseDown[0],
                io.WantCaptureMouse);
        context->OMSetRenderTargets(1,target.GetAddressOf(),nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    } catch(const std::exception& error) {
        updateGameInputCapture(state,false);
        state.failed=true;
        try { spdlog::warn("Diagnostics menu disabled: {}",error.what()); } catch(...) {}
    } catch(...) {
        updateGameInputCapture(state,false);
        state.failed=true;
        try { spdlog::warn("Diagnostics menu disabled after an unexpected error"); } catch(...) {}
    }
}
}
