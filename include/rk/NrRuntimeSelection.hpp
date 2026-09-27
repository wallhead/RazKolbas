#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <filesystem>
#include "rk/Result.hpp"
#include <Windows.h>

namespace rk {
struct NrAdapterIdentity {
    std::uint32_t vendorId{}, deviceId{};
    bool software{};
};
struct NrAdapterLuid {std::uint32_t low{};std::int32_t high{};};
bool sameNrLuid(NrAdapterLuid renderer,NrAdapterLuid nr) noexcept;
enum class NrGpuFamily { Unsupported, Rtx20, Rtx30, Rtx40, Rtx50 };
NrGpuFamily classifyNrGpu(NrAdapterIdentity adapter) noexcept;

enum class NrFamilyMask : unsigned {
    None=0, Rtx20=1, Rtx30=2, Rtx40=4, Rtx50=8
};
constexpr NrFamilyMask operator|(NrFamilyMask a,NrFamilyMask b) noexcept {
    return static_cast<NrFamilyMask>(static_cast<unsigned>(a)|static_cast<unsigned>(b));
}
enum class NrValidation { Disabled, Experimental, Validated };
struct NrRuntimeProfile {
    std::string_view id,relativePath,sha256;
    std::uint64_t size{};
    NrFamilyMask families{NrFamilyMask::None};
    NrValidation validation{NrValidation::Disabled};
    int priority{};
    bool callerIdentityShim{};
    std::uint32_t validatedDeviceId{};
    std::string_view hostContract{"direct-nr-310.8-v1"};
};
struct NrRuntimeArtifact {
    std::string_view id;
    bool present{},exactIdentity{};
};
struct NrRuntimeSelection {
    const NrRuntimeProfile* profile{};
    std::string reason;
};
std::span<const NrRuntimeProfile> nrRuntimeCatalog() noexcept;
NrRuntimeSelection selectNrRuntime(std::span<const NrRuntimeProfile> catalog,
    std::span<const NrRuntimeArtifact> artifacts,NrAdapterIdentity adapter,
    std::string_view requested,bool allowExperimental);
Result<std::filesystem::path> nrRuntimePath(const std::filesystem::path& root,
    const NrRuntimeProfile& profile);
Result<bool> verifyNrRuntimeFile(const std::filesystem::path& path,
    const NrRuntimeProfile& profile);
Result<bool> verifyNrRuntimeHandle(HANDLE file,const NrRuntimeProfile& profile);
bool sameNrFile(HANDLE locked,const std::filesystem::path& other) noexcept;
}
