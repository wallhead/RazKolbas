#include "rk/NrRuntimeSelection.hpp"
#include "rk/PatchDescriptor.hpp"
#include <algorithm>
#include <array>
#include <bcrypt.h>

namespace rk {
namespace {
// NVIDIA's installed nv_dispi.inf (SHA-256
// 1ca553b7c9518e953b0ac35950750b4dbcd64a4c5c48c7638c49405f499a9dfb)
// identifies these exact PCI IDs as desktop GeForce RTX 20/30/40/50.
// Unknown IDs fail closed; an architecture or marketing-name match is not enough.
constexpr std::array rtx20{0x1e04u,0x1e07u,0x1e81u,0x1e82u,0x1e84u,0x1e87u,
    0x1e89u,0x1ec2u,0x1ec7u,0x1f02u,0x1f03u,0x1f06u,0x1f07u,0x1f08u,
    0x1f42u,0x1f47u};
constexpr std::array rtx30{0x2203u,0x2204u,0x2206u,0x2207u,0x2208u,0x220au,
    0x2216u,0x2414u,0x2482u,0x2484u,0x2486u,0x2487u,0x2488u,0x2489u,
    0x24c7u,0x24c9u,0x2503u,0x2504u,0x2507u,0x2508u,0x2544u,0x2582u,0x2584u};
constexpr std::array rtx40{0x2684u,0x2685u,0x2689u,0x2702u,0x2704u,0x2705u,
    0x2709u,0x2782u,0x2783u,0x2786u,0x2788u,0x2803u,0x2805u,0x2808u,
    0x2882u};
constexpr std::array rtx50{0x2b85u,0x2b87u,0x2b8cu,0x2c02u,0x2c05u,0x2c09u,
    0x2d04u,0x2d05u,0x2d83u,0x2f04u,0x2f06u};
constexpr std::array profiles{
    NrRuntimeProfile{"legacy-fastfp16","nvngx_dlssnr.dll",
        "91ea4143d9ed1cb90b11a2851cfc68dabe7d1e7414f8dfaa8016d86b99e40be7",
        165840496,NrFamilyMask::Rtx40,NrValidation::Validated,10,true,0x2702},
    NrRuntimeProfile{"plain-fp16-20-30","NR/plain-fp16-20-30/nvngx_dlssnr.dll",
        "6dac1b40f0c87af84a8177b18c741e84fb0c914f204c9d87d95916b665ba3af8",
        309671536,NrFamilyMask::Rtx20|NrFamilyMask::Rtx30,
        NrValidation::Experimental,20,true,0},
    NrRuntimeProfile{"ada-fastfp16","NR/ada-fastfp16/nvngx_dlssnr.dll",
        "e67dee209320cdafe0e93e45675d7aa34323a53acc57a72b2e40a181581c989a",
        165840496,NrFamilyMask::Rtx40,NrValidation::Experimental,30,true,0},
    NrRuntimeProfile{"nvidia-50","NR/nvidia-50/nvngx_dlssnr.dll",
        "e16bcf15e16e13f527491cdf7845b2fe6521a738d8f7c9c721866a8496e1fc8e",
        165840496,NrFamilyMask::Rtx50,NrValidation::Experimental,40,false,0}
};
NrFamilyMask mask(NrGpuFamily family) noexcept {
    switch(family) {
    case NrGpuFamily::Rtx20:return NrFamilyMask::Rtx20;
    case NrGpuFamily::Rtx30:return NrFamilyMask::Rtx30;
    case NrGpuFamily::Rtx40:return NrFamilyMask::Rtx40;
    case NrGpuFamily::Rtx50:return NrFamilyMask::Rtx50;
    default:return NrFamilyMask::None;
    }
}
const NrRuntimeArtifact* artifact(std::span<const NrRuntimeArtifact> artifacts,
    std::string_view id) noexcept {
    const auto found=std::find_if(artifacts.begin(),artifacts.end(),
        [id](const NrRuntimeArtifact& value){return value.id==id;});
    return found==artifacts.end()?nullptr:&*found;
}
}
bool sameNrLuid(NrAdapterLuid renderer,NrAdapterLuid nr) noexcept {
    return renderer.low==nr.low&&renderer.high==nr.high;
}
NrGpuFamily classifyNrGpu(NrAdapterIdentity adapter) noexcept {
    if(adapter.vendorId!=0x10de||adapter.software)return NrGpuFamily::Unsupported;
    if(std::binary_search(rtx20.begin(),rtx20.end(),adapter.deviceId))
        return NrGpuFamily::Rtx20;
    if(std::binary_search(rtx30.begin(),rtx30.end(),adapter.deviceId))
        return NrGpuFamily::Rtx30;
    if(std::binary_search(rtx40.begin(),rtx40.end(),adapter.deviceId))
        return NrGpuFamily::Rtx40;
    if(std::binary_search(rtx50.begin(),rtx50.end(),adapter.deviceId))
        return NrGpuFamily::Rtx50;
    return NrGpuFamily::Unsupported;
}
std::span<const NrRuntimeProfile> nrRuntimeCatalog() noexcept {return profiles;}
NrRuntimeSelection selectNrRuntime(std::span<const NrRuntimeProfile> catalog,
    std::span<const NrRuntimeArtifact> artifacts,NrAdapterIdentity adapter,
    std::string_view requested,bool allowExperimental) {
    const auto family=classifyNrGpu(adapter);
    if(family==NrGpuFamily::Unsupported)
        return {nullptr,"NR requires a recognized NVIDIA GeForce RTX renderer adapter"};
    const NrRuntimeProfile* best{};
    std::string reason="No validated NR runtime matches the renderer and exact installed bytes";
    for(const auto& profile:catalog) {
        if(requested!="Auto"&&profile.id!=requested)continue;
        if(profile.hostContract!="direct-nr-310.8-v1") {
            if(requested!="Auto")reason="NR profile host contract is incompatible";
            continue;
        }
        if((static_cast<unsigned>(profile.families)&static_cast<unsigned>(mask(family)))==0||
           (profile.validatedDeviceId&&profile.validatedDeviceId!=adapter.deviceId)) {
            if(requested!="Auto")reason="NR profile is not eligible for the renderer GPU";
            continue;
        }
        if(profile.validation==NrValidation::Disabled||
           (profile.validation==NrValidation::Experimental&&
            (requested=="Auto"||!allowExperimental))) {
            if(requested!="Auto")reason="NR profile requires explicit experimental opt-in";
            continue;
        }
        const auto* installed=artifact(artifacts,profile.id);
        if(!installed||!installed->present||!installed->exactIdentity) {
            if(requested!="Auto")reason="NR profile is missing or its bytes do not match";
            continue;
        }
        if(!best||profile.priority>best->priority||
           (profile.priority==best->priority&&profile.id<best->id))best=&profile;
    }
    if(best)return {best,"Exact eligible NR runtime selected"};
    if(requested!="Auto"&&
       std::none_of(catalog.begin(),catalog.end(),
           [requested](const NrRuntimeProfile& profile){return profile.id==requested;}))
        reason="Unknown NR runtime profile";
    return {nullptr,reason};
}
Result<std::filesystem::path> nrRuntimePath(const std::filesystem::path& root,
    const NrRuntimeProfile& profile) {
    namespace fs=std::filesystem;
    if(root.empty()||!root.is_absolute()||profile.relativePath.empty())
        return Error{ErrorCode::InvalidInput,"NR runtime root or profile path is invalid"};
    const fs::path relative{profile.relativePath};
    if(relative.is_absolute()||relative.has_root_name()||
       relative.filename()!=L"nvngx_dlssnr.dll")
        return Error{ErrorCode::InvalidInput,"NR runtime path is not controlled"};
    for(const auto& component:relative)if(component==L".."||component==L".")
        return Error{ErrorCode::InvalidInput,"NR runtime path escapes its root"};
    return (root/relative).lexically_normal();
}
Result<bool> verifyNrRuntimeFile(const std::filesystem::path& path,
    const NrRuntimeProfile& profile) {
    namespace fs=std::filesystem;
    std::error_code error;
    if(!fs::is_regular_file(path,error)||error)
        return Error{ErrorCode::Unavailable,"NR runtime artifact is missing"};
    if(fs::file_size(path,error)!=profile.size||error)
        return Error{ErrorCode::Conflict,"NR runtime artifact size differs"};
    const auto digest=sha256File(path);
    if(const auto failure=std::get_if<Error>(&digest))return *failure;
    if(std::get<std::string>(digest)!=profile.sha256)
        return Error{ErrorCode::Conflict,"NR runtime artifact hash differs"};
    return true;
}
Result<bool> verifyNrRuntimeHandle(HANDLE file,const NrRuntimeProfile& profile) {
    if(!file||file==INVALID_HANDLE_VALUE)
        return Error{ErrorCode::InvalidInput,"NR runtime file handle is invalid"};
    LARGE_INTEGER size{};
    if(!GetFileSizeEx(file,&size)||size.QuadPart<0||
       static_cast<std::uint64_t>(size.QuadPart)!=profile.size)
        return Error{ErrorCode::Conflict,"Locked NR runtime size differs"};
    LARGE_INTEGER beginning{};
    if(!SetFilePointerEx(file,beginning,nullptr,FILE_BEGIN))
        return Error{ErrorCode::Io,"Cannot seek locked NR runtime"};
    struct HashHandles {
        BCRYPT_ALG_HANDLE algorithm{};
        BCRYPT_HASH_HANDLE hash{};
        ~HashHandles() {
            if(hash)BCryptDestroyHash(hash);
            if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
        }
    } handles;
    if(BCryptOpenAlgorithmProvider(&handles.algorithm,BCRYPT_SHA256_ALGORITHM,
           nullptr,0)<0||
       BCryptCreateHash(handles.algorithm,&handles.hash,nullptr,0,nullptr,0,0)<0)
        return Error{ErrorCode::Unavailable,"Cannot hash locked NR runtime"};
    std::array<std::uint8_t,64*1024> chunk{};
    std::uint64_t bytes{};
    for(;;) {
        DWORD read{};
        if(!ReadFile(file,chunk.data(),static_cast<DWORD>(chunk.size()),&read,nullptr))
            return Error{ErrorCode::Io,"Cannot read locked NR runtime"};
        if(!read)break;
        bytes+=read;
        if(BCryptHashData(handles.hash,chunk.data(),read,0)<0)
            return Error{ErrorCode::Unavailable,"Cannot update locked NR hash"};
    }
    if(bytes!=profile.size)
        return Error{ErrorCode::Conflict,"Locked NR runtime changed during read"};
    std::array<std::uint8_t,32> digest{};
    if(BCryptFinishHash(handles.hash,digest.data(),
           static_cast<ULONG>(digest.size()),0)<0)
        return Error{ErrorCode::Unavailable,"Cannot finish locked NR hash"};
    constexpr char digits[]="0123456789abcdef";
    std::string hex;
    hex.reserve(64);
    for(const auto byte:digest) {hex+=digits[byte>>4];hex+=digits[byte&15];}
    if(hex!=profile.sha256)
        return Error{ErrorCode::Conflict,"Locked NR runtime hash differs"};
    return true;
}
bool sameNrFile(HANDLE locked,const std::filesystem::path& other) noexcept {
    if(!locked||locked==INVALID_HANDLE_VALUE)return false;
    const auto opened=CreateFileW(other.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,
        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(opened==INVALID_HANDLE_VALUE)return false;
    BY_HANDLE_FILE_INFORMATION a{},b{};
    const bool same=GetFileInformationByHandle(locked,&a)&&
        GetFileInformationByHandle(opened,&b)&&
        a.dwVolumeSerialNumber==b.dwVolumeSerialNumber&&
        a.nFileIndexHigh==b.nFileIndexHigh&&a.nFileIndexLow==b.nFileIndexLow;
    CloseHandle(opened);
    return same;
}
}
