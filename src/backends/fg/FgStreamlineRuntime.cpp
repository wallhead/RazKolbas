#include "rk/FgStreamlineRuntime.hpp"
#include "rk/PatchDescriptor.hpp"
#include <sl_dlss_g.h>
#include <sl_pcl.h>
#include <sl_reflex.h>
#include <array>
#include <string>
#include <variant>

namespace rk {
namespace {
struct RuntimeFile {const wchar_t* name;const char* sha256;};
constexpr std::array files{
    RuntimeFile{L"sl.interposer.dll",
        "8c87c9499461da561edd529aa9bf7831d67d7b94ebb1c1a5ed54ef4934e1ea4c"},
    RuntimeFile{L"sl.common.dll",
        "82924a8954dd671e09351c5de0eb87ad0eb25b944cc9f9ab955ca1d9950de15d"},
    RuntimeFile{L"sl.dlss_g.dll",
        "f4a6b2b14dcc0b1485989e430d3b4e3a44ac1800b92ba1ad74f476e64fb2b09c"},
    RuntimeFile{L"sl.reflex.dll",
        "0ce9725e3e03ea9e7f81d008b57f33ee365973d2e349131c8b1c3e3378fe2db0"},
    RuntimeFile{L"sl.pcl.dll",
        "f13d51cfa05f4cd514df2026049e2db8adf359221713170ad386fd499915b582"},
    RuntimeFile{L"nvngx_dlssg.dll",
        "ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82"}};
template<class Function> Function exportFrom(HMODULE module,const char* name) noexcept {
    return reinterpret_cast<Function>(GetProcAddress(module,name));
}
}
Result<std::unique_ptr<FgStreamlineRuntime>>
FgStreamlineRuntime::initialize(const std::filesystem::path& binaryDirectory) {
    std::error_code ec;
    const auto directory=std::filesystem::canonical(binaryDirectory,ec);
    if(ec||!directory.is_absolute()||
       !std::filesystem::is_directory(directory,ec)||ec)
        return Error{ErrorCode::InvalidInput,
            "Streamline runtime directory is unavailable"};
    for(const auto& file:files) {
        const auto digest=sha256File(directory/file.name);
        if(!std::holds_alternative<std::string>(digest)||
           std::get<std::string>(digest)!=file.sha256)
            return Error{ErrorCode::Conflict,
                "Pinned Streamline runtime file identity differs"};
    }
    auto runtime=std::unique_ptr<FgStreamlineRuntime>(new FgStreamlineRuntime);
    runtime->directory_=directory;
    runtime->cookie_=AddDllDirectory(directory.c_str());
    if(!runtime->cookie_)
        return Error{ErrorCode::Unavailable,
            "Cannot register private Streamline dependency directory"};
    runtime->module_=LoadLibraryExW((directory/L"sl.interposer.dll").c_str(),
        nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|
            LOAD_LIBRARY_SEARCH_DEFAULT_DIRS|
            LOAD_LIBRARY_SEARCH_USER_DIRS);
    if(!runtime->module_)
        return Error{ErrorCode::Unavailable,
            "Cannot load pinned private Streamline interposer"};
    runtime->init_=exportFrom<Init>(runtime->module_,"slInit");
    runtime->shutdown_=exportFrom<Shutdown>(runtime->module_,"slShutdown");
    runtime->setDevice_=exportFrom<SetDevice>(runtime->module_,"slSetD3DDevice");
    runtime->upgrade_=exportFrom<Upgrade>(runtime->module_,"slUpgradeInterface");
    runtime->native_=exportFrom<Native>(runtime->module_,"slGetNativeInterface");
    runtime->featureFunction_=exportFrom<FeatureFunction>(runtime->module_,
        "slGetFeatureFunction");
    if(!runtime->init_||!runtime->shutdown_||!runtime->setDevice_||
       !runtime->upgrade_||!runtime->native_||!runtime->featureFunction_)
        return Error{ErrorCode::Conflict,
            "Pinned Streamline core exports are incomplete"};
    const wchar_t* pluginPaths[]{runtime->directory_.c_str()};
    const sl::Feature features[]{sl::kFeatureDLSS_G,sl::kFeatureReflex,
        sl::kFeaturePCL};
    sl::Preferences preferences{};
    preferences.pathsToPlugins=pluginPaths;
    preferences.numPathsToPlugins=1;
    preferences.featuresToLoad=features;
    preferences.numFeaturesToLoad=3;
    preferences.flags=sl::PreferenceFlags::eDisableCLStateTracking|
        sl::PreferenceFlags::eUseManualHooking|
        sl::PreferenceFlags::eUseFrameBasedResourceTagging;
    preferences.engine=sl::EngineType::eCustom;
    preferences.engineVersion="0.1.130";
    preferences.projectId="b3340e44-a57e-4b98-9318-d7150829d110";
    preferences.renderAPI=sl::RenderAPI::eD3D12;
    const auto result=runtime->init_(preferences,sl::kSDKVersion);
    if(result!=sl::Result::eOk)
        return Error{ErrorCode::Unavailable,
            "Pinned Streamline initialization failed: "+
                std::to_string(static_cast<int>(result))};
    runtime->initialized_=true;
    return runtime;
}
FgStreamlineRuntime::~FgStreamlineRuntime() noexcept {
    // A live provider may still own callbacks into this module. In that case
    // deliberately retain both DLL and search directory for process lifetime.
    if(initialized_)return;
    if(module_)FreeLibrary(module_);
    if(cookie_)RemoveDllDirectory(cookie_);
}
Result<bool> FgStreamlineRuntime::shutdown() noexcept {
    if(!initialized_)return true;
    if(shutdown_()!=sl::Result::eOk)
        return Error{ErrorCode::Unavailable,
            "Streamline shutdown failed; runtime remains pinned"};
    initialized_=false;
    return true;
}
sl::Result FgStreamlineRuntime::setD3DDevice(void* device) const noexcept {
    return initialized_&&setDevice_&&device?setDevice_(device):
        sl::Result::eErrorInvalidParameter;
}
sl::Result FgStreamlineRuntime::upgradeInterface(void** value) const noexcept {
    return initialized_&&upgrade_&&value&&*value?upgrade_(value):
        sl::Result::eErrorInvalidParameter;
}
sl::Result FgStreamlineRuntime::getNativeInterface(void* proxy,
    void** native) const noexcept {
    return initialized_&&native_&&proxy&&native?native_(proxy,native):
        sl::Result::eErrorInvalidParameter;
}
sl::Result FgStreamlineRuntime::getFeatureFunction(sl::Feature feature,
    const char* name,void*& function) const noexcept {
    function=nullptr;
    return initialized_&&featureFunction_&&name?
        featureFunction_(feature,name,function):
        sl::Result::eErrorInvalidParameter;
}
}
