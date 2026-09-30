#pragma once
#include "rk/Result.hpp"
#include <sl.h>
#include <Windows.h>
#include <filesystem>
#include <memory>

namespace rk {
// Private, exact-version Streamline loader. Initialization occurs outside
// DllMain and before the game's swap creation boundary. The caller must
// shut down only after all Streamline-owned GPU interfaces have retired.
class FgStreamlineRuntime {
public:
    static Result<std::unique_ptr<FgStreamlineRuntime>> initialize(
        const std::filesystem::path& binaryDirectory);
    ~FgStreamlineRuntime() noexcept;
    FgStreamlineRuntime(const FgStreamlineRuntime&)=delete;
    FgStreamlineRuntime& operator=(const FgStreamlineRuntime&)=delete;
    Result<bool> shutdown() noexcept;
    Result<bool> verifyLoadedModules() const;
    sl::Result setD3DDevice(void* device) const noexcept;
    sl::Result upgradeInterface(void** value) const noexcept;
    sl::Result getNativeInterface(void* proxy,void** native) const noexcept;
    sl::Result getFeatureFunction(sl::Feature feature,const char* name,
        void*& function) const noexcept;
    sl::Result newFrameToken(sl::FrameToken*& token) const noexcept;
    sl::Result setConstants(const sl::Constants& values,const sl::FrameToken& token,
        const sl::ViewportHandle& viewport) const noexcept;
    sl::Result setTags(const sl::FrameToken& token,const sl::ViewportHandle& viewport,
        const sl::ResourceTag* tags,std::uint32_t count,sl::CommandBuffer* commands) const noexcept;
    bool initialized() const noexcept { return initialized_; }
    const std::filesystem::path& directory() const noexcept { return directory_; }
private:
    FgStreamlineRuntime()=default;
    bool loadedModulesOwned() const noexcept;
    using Init=decltype(&::slInit);
    using Shutdown=decltype(&::slShutdown);
    using SetDevice=decltype(&::slSetD3DDevice);
    using Upgrade=decltype(&::slUpgradeInterface);
    using Native=decltype(&::slGetNativeInterface);
    using FeatureFunction=decltype(&::slGetFeatureFunction);
    using NewFrameToken=decltype(&::slGetNewFrameToken);
    using SetConstants=decltype(&::slSetConstants);
    using SetTags=decltype(&::slSetTagForFrame);
    std::filesystem::path directory_;
    HMODULE module_{};
    DLL_DIRECTORY_COOKIE cookie_{};
    Init init_{};
    Shutdown shutdown_{};
    SetDevice setDevice_{};
    Upgrade upgrade_{};
    Native native_{};
    FeatureFunction featureFunction_{};
    NewFrameToken newFrameToken_{};
    SetConstants setConstants_{};
    SetTags setTags_{};
    bool initialized_{};
};
}
