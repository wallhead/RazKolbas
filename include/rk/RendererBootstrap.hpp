#pragma once
#include "rk/RendererHook.hpp"
#include "rk/Settings.hpp"
namespace rk {
class OwnedSceneDomain;
class NativeUiRedirector;
using RendererObserved = void(*)(const RendererSnapshot&);
Result<bool> installRendererObserver(const Settings& settings, RendererObserved notification);
bool rendererObserverArmed() noexcept;
// Requires a process-lifetime domain and the exact verified ENB immediate
// context. Both downstream slots are validated before either is replaced.
Result<bool> installOwnedUiContextHooks(ID3D11DeviceContext* context,
    OwnedSceneDomain& domain,ID3D11Texture2D* reducedScene,
    ID3D11RenderTargetView* nativeTarget,std::string_view disabledPatchIds);
NativeUiRedirector* ownedUiRedirector() noexcept;
}
