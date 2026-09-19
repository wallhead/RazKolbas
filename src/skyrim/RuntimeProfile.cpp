#include "rk/RendererHook.hpp"
#include "rk/SwapObserver.hpp"
#include <algorithm>
#include <cstring>
#include <optional>
namespace rk {
bool rendererObserverRequested(const Settings& settings) {
    return settings.get<bool>("General.Enabled") && !settings.get<bool>("General.SafeMode") &&
        settings.get<bool>("Patching.EnableVersionedPatches") && settings.get<bool>("Patching.ExperimentalPatches") &&
        !patchDisabled(settings.get<Text>("Patching.DisabledPatchIds").value,rendererObserverPatchId);
}
const CreationImportProfile& skyrim1170CreationProfile() {
    static constexpr CreationImportProfile profile{
        "c434208894f07f604b852f29b8edc3a58c4de63de783373733e72b2b73f33be9",
        37157144, 0x3870000, 0x17502a0};
    return profile;
}
const CreationOwnerProfile* creationOwnerProfile(std::string_view hash) {
    static constexpr CreationOwnerProfile owners[] = {
        {"local ENB d3d11 wrapper", "47ff220dd26a44520d4cec2d515d89effe87b632c1885c32388c93e8d0ceda58",
         4664320, 0xaae000, 0x5e410, {0x48,0x8b,0xc4,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x81,0xec,0x40}},
        {"local Windows d3d11 runtime", "722871e4ac32972617483197709fe0d924ced5ed894b18fd13e0813d0b25950f",
         2476632, 0x257000, 0x429f0, {0x48,0x8b,0xc4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x48,0x89,0x78,0x18,0x55}}
    };
    for (const auto& owner : owners) if (owner.sha256==hash) return &owner;
    return nullptr;
}
namespace {
template<class T> std::optional<T> read(std::span<const std::uint8_t> image, std::size_t offset) {
    if (offset>image.size() || sizeof(T)>image.size()-offset) return std::nullopt;
    T value{}; std::memcpy(&value,image.data()+offset,sizeof(value)); return value;
}
bool equalDll(std::string_view name, std::string_view expected) {
    if (name.size()!=expected.size()) return false;
    for (std::size_t i=0;i<name.size();++i) {
        const auto lower=[](char c) { return c>='A' && c<='Z' ? static_cast<char>(c-'A'+'a') : c; };
        if (lower(name[i])!=lower(expected[i])) return false;
    }
    return true;
}
}
Result<std::uint32_t> validateCreationImport(std::span<const std::uint8_t> mapped,
    std::string_view fileHash, std::size_t fileSize, const CreationImportProfile& profile) {
    const auto reject=[](const char* message) -> Result<std::uint32_t> { return Error{ErrorCode::Unsupported,message}; };
    if (fileHash!=profile.gameSha256 || fileSize!=profile.fileSize) return reject("Unknown executable identity");
    if (mapped.size()!=profile.imageSize) return reject("Mapped image size mismatch");
    const auto dos=read<IMAGE_DOS_HEADER>(mapped,0);
    if (!dos || dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0) return reject("Invalid DOS header");
    const auto ntOffset=static_cast<std::size_t>(dos->e_lfanew);
    const auto nt=read<IMAGE_NT_HEADERS64>(mapped,ntOffset);
    if (!nt || nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 ||
        nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt->FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64) ||
        nt->OptionalHeader.SizeOfImage!=profile.imageSize || nt->FileHeader.NumberOfSections==0 ||
        nt->FileHeader.NumberOfSections>96 || nt->OptionalHeader.NumberOfRvaAndSizes<=IMAGE_DIRECTORY_ENTRY_IMPORT)
        return reject("Unsupported mapped PE headers");
    std::size_t begin=0,end=0;
    for (std::size_t i=0;i<nt->FileHeader.NumberOfSections;++i) {
        const auto section=read<IMAGE_SECTION_HEADER>(mapped,ntOffset+sizeof(IMAGE_NT_HEADERS64)+i*sizeof(IMAGE_SECTION_HEADER));
        if (!section) return reject("Truncated section table");
        if (std::memcmp(section->Name,".rdata\0\0",8)!=0) continue;
        if (end!=0 || !(section->Characteristics&IMAGE_SCN_MEM_READ) ||
            (section->Characteristics&IMAGE_SCN_MEM_EXECUTE) || section->VirtualAddress>mapped.size() ||
            section->Misc.VirtualSize>mapped.size()-section->VirtualAddress) return reject("Invalid or ambiguous rdata section");
        begin=section->VirtualAddress; end=begin+section->Misc.VirtualSize;
    }
    const auto inRdata=[&](std::size_t offset,std::size_t size) {
        return offset>=begin && offset<=end && size<=end-offset && end!=0;
    };
    const auto stringAt=[&](std::size_t offset) -> std::optional<std::string_view> {
        if (!inRdata(offset,1)) return std::nullopt;
        const auto first=mapped.data()+offset;
        const auto last=std::find(first,mapped.data()+end,std::uint8_t{0});
        if (last==mapped.data()+end) return std::nullopt;
        return std::string_view(reinterpret_cast<const char*>(first),static_cast<std::size_t>(last-first));
    };
    const auto directory=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!inRdata(directory.VirtualAddress,directory.Size) || directory.Size<sizeof(IMAGE_IMPORT_DESCRIPTOR))
        return reject("Import directory outside rdata");
    std::optional<std::uint32_t> found;
    bool descriptorTerminated=false;
    for (std::size_t i=0;(i+1)*sizeof(IMAGE_IMPORT_DESCRIPTOR)<=directory.Size;++i) {
        const auto descriptor=read<IMAGE_IMPORT_DESCRIPTOR>(mapped,directory.VirtualAddress+i*sizeof(IMAGE_IMPORT_DESCRIPTOR));
        if (!descriptor) return reject("Truncated import descriptor");
        if (!descriptor->Name) {
            if (descriptor->OriginalFirstThunk || descriptor->FirstThunk) return reject("Malformed import terminator");
            descriptorTerminated=true; break;
        }
        const auto dll=stringAt(descriptor->Name);
        if (!dll) return reject("Invalid import module name");
        if (!equalDll(*dll,"d3d11.dll")) continue;
        if (!descriptor->OriginalFirstThunk || !descriptor->FirstThunk) return reject("Import names unavailable");
        bool thunkTerminated=false;
        for (std::size_t j=0;j<mapped.size()/sizeof(IMAGE_THUNK_DATA64);++j) {
            const auto nameOffset=static_cast<std::size_t>(descriptor->OriginalFirstThunk)+j*sizeof(IMAGE_THUNK_DATA64);
            const auto slotOffset=static_cast<std::size_t>(descriptor->FirstThunk)+j*sizeof(IMAGE_THUNK_DATA64);
            if (!inRdata(nameOffset,8) || !inRdata(slotOffset,8)) return reject("Import thunk outside rdata");
            const auto thunk=read<IMAGE_THUNK_DATA64>(mapped,nameOffset);
            if (!thunk) return reject("Truncated import thunk");
            if (!thunk->u1.AddressOfData) { thunkTerminated=true; break; }
            if (IMAGE_SNAP_BY_ORDINAL64(thunk->u1.Ordinal)) continue;
            if (thunk->u1.AddressOfData>mapped.size()) return reject("Invalid import name RVA");
            const auto symbol=stringAt(static_cast<std::size_t>(thunk->u1.AddressOfData)+2);
            if (!symbol) return reject("Unterminated import symbol");
            if (*symbol!="D3D11CreateDeviceAndSwapChain") continue;
            if (found || slotOffset!=profile.iatRva || slotOffset%alignof(void*)!=0)
                return reject("Ambiguous or changed creation IAT slot");
            found=static_cast<std::uint32_t>(slotOffset);
        }
        if (!thunkTerminated) return reject("Import thunks have no terminator");
    }
    if (!descriptorTerminated || !found) return reject("Creation import not uniquely resolved");
    return *found;
}
HRESULT observeDeviceCreation(CreateD3D11 original, const DeviceCreationArgs& a, CreationObserver observer) noexcept {
    if (!original) return E_POINTER;
    const auto result=original(a.adapter,a.driverType,a.software,a.flags,a.levels,a.levelCount,
        a.sdkVersion,a.swapDesc,a.swapChain,a.device,a.featureLevel,a.context);
    const auto lastError=GetLastError();
    try { if (observer) observer(a,result); } catch (...) { /* Diagnostics must never change rendering. */ }
    SetLastError(lastError);
    return result;
}
}
