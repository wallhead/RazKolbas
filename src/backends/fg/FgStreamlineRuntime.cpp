#include "rk/FgStreamlineRuntime.hpp"
#include "rk/PatchDescriptor.hpp"
#include <sl_dlss_g.h>
#include <sl_pcl.h>
#include <sl_reflex.h>
#include <TlHelp32.h>
#include <array>
#include <mutex>
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
std::mutex initializationMutex;
struct ReadLock {
    HANDLE handle{INVALID_HANDLE_VALUE};
    ~ReadLock(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
    ReadLock()=default;
    ReadLock(const ReadLock&)=delete;
    ReadLock& operator=(const ReadLock&)=delete;
};
struct Snapshot {
    HANDLE handle{INVALID_HANDLE_VALUE};
    ~Snapshot(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
};
Result<bool> inspectLoaded(const std::filesystem::path& directory,
    HMODULE expectedInterposer,bool requireInterposer) {
    Snapshot snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|
        TH32CS_SNAPMODULE32,GetCurrentProcessId())};
    if(snapshot.handle==INVALID_HANDLE_VALUE)
        return Error{ErrorCode::Unavailable,
            "Cannot enumerate process modules for Streamline ownership"};
    MODULEENTRY32W entry{};
    entry.dwSize=sizeof(entry);
    if(!Module32FirstW(snapshot.handle,&entry))
        return Error{ErrorCode::Unavailable,
            "Cannot begin process module ownership inspection"};
    bool foundInterposer=false;
    do {
        for(const auto& file:files) {
            if(_wcsicmp(entry.szModule,file.name)!=0)continue;
            if(!expectedInterposer)
                return Error{ErrorCode::Conflict,
                    "A Streamline runtime module is already loaded"};
            std::error_code ec;
            const auto expected=directory/file.name;
            if(!std::filesystem::equivalent(entry.szExePath,expected,ec)||ec)
                return Error{ErrorCode::Conflict,
                    "Loaded Streamline module path differs from pinned file"};
            const auto digest=sha256File(entry.szExePath);
            if(!std::holds_alternative<std::string>(digest)||
               std::get<std::string>(digest)!=file.sha256)
                return Error{ErrorCode::Conflict,
                    "Loaded Streamline module hash differs from pinned file"};
            if(_wcsicmp(file.name,L"sl.interposer.dll")==0) {
                if(entry.hModule!=expectedInterposer)
                    return Error{ErrorCode::Conflict,
                        "Loaded Streamline interposer handle differs"};
                foundInterposer=true;
            }
        }
        entry.dwSize=sizeof(entry);
    } while(Module32NextW(snapshot.handle,&entry));
    if(GetLastError()!=ERROR_NO_MORE_FILES)
        return Error{ErrorCode::Unavailable,
            "Process module enumeration ended unexpectedly"};
    if(requireInterposer&&!foundInterposer)
        return Error{ErrorCode::Unavailable,
            "Pinned Streamline interposer is absent from process modules"};
    return true;
}
template<class Function> Function exportFrom(HMODULE module,const char* name) noexcept {
    return reinterpret_cast<Function>(GetProcAddress(module,name));
}
bool exportOwned(HMODULE expected,FARPROC address) noexcept {
    HMODULE owner{};
    if(!address||!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(address),&owner))return false;
    const bool same=owner==expected;
    FreeLibrary(owner);
    return same;
}
bool pinnedFeatureOwner(const std::filesystem::path& directory,
    void* function) {
    HMODULE owner{};
    if(!function||!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(function),&owner))return false;
    wchar_t path[32768]{};
    const auto count=GetModuleFileNameW(owner,path,32768);
    FreeLibrary(owner);
    if(!count||count>=32768)return false;
    const auto moduleName=std::filesystem::path(path).filename().wstring();
    for(const auto& file:files) {
        if(_wcsicmp(moduleName.c_str(),file.name)!=0)continue;
        std::error_code ec;
        if(!std::filesystem::equivalent(path,directory/file.name,ec)||ec)
            return false;
        const auto digest=sha256File(path);
        return std::holds_alternative<std::string>(digest)&&
            std::get<std::string>(digest)==file.sha256;
    }
    return false;
}
}
Result<std::unique_ptr<FgStreamlineRuntime>>
FgStreamlineRuntime::initialize(const std::filesystem::path& binaryDirectory) {
    std::scoped_lock lock(initializationMutex);
    std::error_code ec;
    const auto directory=std::filesystem::canonical(binaryDirectory,ec);
    if(ec||!directory.is_absolute()||
       !std::filesystem::is_directory(directory,ec)||ec)
        return Error{ErrorCode::InvalidInput,
            "Streamline runtime directory is unavailable"};
    const auto existing=inspectLoaded(directory,nullptr,false);
    if(const auto error=std::get_if<Error>(&existing))return *error;
    std::array<ReadLock,files.size()> retained;
    for(std::size_t index=0;index<files.size();++index) {
        const auto& file=files[index];
        retained[index].handle=CreateFileW((directory/file.name).c_str(),
            GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,nullptr);
        if(retained[index].handle==INVALID_HANDLE_VALUE)
            return Error{ErrorCode::Unavailable,
                "Cannot hold pinned Streamline file through initialization"};
        const auto digest=sha256File(directory/file.name);
        if(!std::holds_alternative<std::string>(digest)||
           std::get<std::string>(digest)!=file.sha256)
            return Error{ErrorCode::Conflict,
                "Pinned Streamline runtime file identity differs"};
    }
    const auto rechecked=inspectLoaded(directory,nullptr,false);
    if(const auto error=std::get_if<Error>(&rechecked))return *error;
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
    const auto loadedModules=inspectLoaded(directory,runtime->module_,true);
    if(const auto error=std::get_if<Error>(&loadedModules))return *error;
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
    for(const auto address:{reinterpret_cast<FARPROC>(runtime->init_),
        reinterpret_cast<FARPROC>(runtime->shutdown_),
        reinterpret_cast<FARPROC>(runtime->setDevice_),
        reinterpret_cast<FARPROC>(runtime->upgrade_),
        reinterpret_cast<FARPROC>(runtime->native_),
        reinterpret_cast<FARPROC>(runtime->featureFunction_)})
        if(!exportOwned(runtime->module_,address))
            return Error{ErrorCode::Conflict,
                "Streamline core export is owned by another module"};
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
    const auto initializedModules=runtime->verifyLoadedModules();
    if(const auto error=std::get_if<Error>(&initializedModules)) {
        const auto stopped=runtime->shutdown();
        if(std::holds_alternative<Error>(stopped))
            return Error{ErrorCode::Conflict,
                "Streamline ownership conflict; shutdown failed and module retained"};
        return *error;
    }
    return runtime;
}
Result<bool> FgStreamlineRuntime::verifyLoadedModules() const {
    if(!initialized_||!module_)
        return Error{ErrorCode::Unavailable,
            "Streamline runtime is not initialized"};
    return inspectLoaded(directory_,module_,true);
}
bool FgStreamlineRuntime::loadedModulesOwned() const noexcept {
    try {return std::holds_alternative<bool>(verifyLoadedModules());}
    catch(...) {return false;}
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
    if(!initialized_||!setDevice_||!device)
        return sl::Result::eErrorInvalidParameter;
    if(!loadedModulesOwned())
        return sl::Result::eErrorInvalidState;
    const auto result=setDevice_(device);
    if(!loadedModulesOwned())
        return sl::Result::eErrorInvalidState;
    return result;
}
sl::Result FgStreamlineRuntime::upgradeInterface(void** value) const noexcept {
    if(!initialized_||!upgrade_||!value||!*value)
        return sl::Result::eErrorInvalidParameter;
    if(!loadedModulesOwned())
        return sl::Result::eErrorInvalidState;
    const auto result=upgrade_(value);
    if(!loadedModulesOwned())
        return sl::Result::eErrorInvalidState;
    return result;
}
sl::Result FgStreamlineRuntime::getNativeInterface(void* proxy,
    void** native) const noexcept {
    if(!initialized_||!native_||!proxy||!native)
        return sl::Result::eErrorInvalidParameter;
    if(!loadedModulesOwned())
        return sl::Result::eErrorInvalidState;
    const auto result=native_(proxy,native);
    if(!loadedModulesOwned())
        return sl::Result::eErrorInvalidState;
    return result;
}
sl::Result FgStreamlineRuntime::getFeatureFunction(sl::Feature feature,
    const char* name,void*& function) const noexcept {
    function=nullptr;
    if(!initialized_||!featureFunction_||!name)
        return sl::Result::eErrorInvalidParameter;
    if(!loadedModulesOwned())
        return sl::Result::eErrorInvalidState;
    const auto result=featureFunction_(feature,name,function);
    if(!loadedModulesOwned()) {
        function=nullptr;
        return sl::Result::eErrorInvalidState;
    }
    try {
        if(result==sl::Result::eOk&&
           !pinnedFeatureOwner(directory_,function)) {
            function=nullptr;
            return sl::Result::eErrorInvalidState;
        }
    } catch(...) {
        function=nullptr;
        return sl::Result::eErrorInvalidState;
    }
    return result;
}
}
