#include "rk/PatchDescriptor.hpp"
#include "rk/PointerPatch.hpp"
#include "rk/ProbeRetirement.hpp"
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>

using Microsoft::WRL::ComPtr;

// Experimental exact-runtime calling convention observed at Fallout RVA
// 0x3424e..0x34278. This is not a public NVIDIA ABI or a shipping backend.
using InitExt = std::uint32_t(__cdecl*)(std::uint64_t, const wchar_t*, ID3D12Device*, std::uint32_t, const void*);
using Shutdown = std::uint32_t(__cdecl*)(ID3D12Device*);

DWORD WINAPI callerNameProxy(HMODULE module, LPWSTR buffer, DWORD size) {
    if (module != GetModuleHandleW(nullptr)) return GetModuleFileNameW(module, buffer, size);
    constexpr wchar_t observedName[] = L"nvngx.dll";
    if (!buffer || size == 0) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return 0; }
    const DWORD copied = size > 9 ? 9 : size-1;
    std::copy_n(observedName, copied, buffer);
    buffer[copied] = 0;
    if (copied != 9) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return size; }
    return copied;
}

void** findModuleNameImport(HMODULE module) {
    // Input module is already exact-file-hash gated; these are loader-mapped PE RVAs.
    const auto base = reinterpret_cast<std::uint8_t*>(module);
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    const auto directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!directory.VirtualAddress || directory.VirtualAddress >= nt->OptionalHeader.SizeOfImage || directory.Size > nt->OptionalHeader.SizeOfImage-directory.VirtualAddress) return nullptr;
    const auto imports = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base+directory.VirtualAddress);
    void** found = nullptr;
    for (std::size_t i=0; (i+1)*sizeof(IMAGE_IMPORT_DESCRIPTOR)<=directory.Size && imports[i].Name; ++i) {
        if (!imports[i].OriginalFirstThunk) continue;
        const auto names = reinterpret_cast<const IMAGE_THUNK_DATA64*>(base+imports[i].OriginalFirstThunk);
        auto slots = reinterpret_cast<IMAGE_THUNK_DATA64*>(base+imports[i].FirstThunk);
        for (std::size_t j=0; names[j].u1.AddressOfData; ++j) {
            if (IMAGE_SNAP_BY_ORDINAL64(names[j].u1.Ordinal)) continue;
            const auto name = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base+names[j].u1.AddressOfData);
            if (std::string_view(name->Name) == "GetModuleFileNameW") {
                if (found) return nullptr;
                found = reinterpret_cast<void**>(&slots[j].u1.Function);
            }
        }
    }
    return found;
}

int wmain(int argc, wchar_t** argv) {
    try {
        if (argc < 2 || argc > 3 || (argc == 3 && std::wstring_view(argv[2]) != L"--caller-shim")) { std::cerr << "Usage: RazKolbasNrBootstrapProbe <exact nvngx_dlssnr.dll> [--caller-shim]\n"; return 2; }
        const bool useShim = argc == 3;
        const auto path = std::filesystem::absolute(argv[1]);
        std::ifstream stream(path, std::ios::binary);
        if (!stream) { std::cerr << "MISSING_RUNTIME\n"; return 2; }
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(stream)), {});
        const auto hash = rk::sha256(bytes);
        if (hash != "8270b350cd82de5ce89806872cdd6b6a9249b80836b91bbeb3573470744cc206") {
            std::cerr << "HASH_MISMATCH " << hash << '\n'; return 2;
        }
        ComPtr<IDXGIFactory6> factory;
        HRESULT status = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
        if (FAILED(status)) { std::cerr << "DXGI_FACTORY_FAILED\n"; return 3; }
        ComPtr<IDXGIAdapter1> selected;
        DXGI_ADAPTER_DESC1 description{};
        for (UINT index=0;; ++index) {
            ComPtr<IDXGIAdapter1> candidate;
            status = factory->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&candidate));
            if (status == DXGI_ERROR_NOT_FOUND) break;
            if (FAILED(status)) { std::cerr << "ADAPTER_ENUM_FAILED\n"; return 3; }
            candidate->GetDesc1(&description);
            if (description.VendorId == 0x10de && !(description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) { selected = candidate; break; }
        }
        if (!selected) { std::cerr << "NVIDIA_ADAPTER_NOT_FOUND\n"; return 3; }
        ComPtr<ID3D12Device> device;
        status = D3D12CreateDevice(selected.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
        if (FAILED(status)) { std::cerr << "DEVICE_CREATE_FAILED 0x" << std::hex << status << '\n'; return 3; }
        std::wcout << L"Probe adapter: " << description.Description << L"; LUID " << std::hex << description.AdapterLuid.HighPart << L":" << description.AdapterLuid.LowPart << std::endl;
        std::cout << "This is the standalone probe adapter, not Skyrim render-adapter detection.\n" << std::flush;
        const auto module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module) { std::cerr << "LOAD_FAILED win32=" << GetLastError() << '\n'; return 4; }
        const auto init = reinterpret_cast<InitExt>(GetProcAddress(module, "NVSDK_NGX_D3D12_Init_Ext"));
        const auto shutdown = reinterpret_cast<Shutdown>(GetProcAddress(module, "NVSDK_NGX_D3D12_Shutdown1"));
        if (!init || !shutdown) { std::cerr << "REQUIRED_EXPORT_MISSING\n"; FreeLibrary(module); return 4; }
        rk::PointerPatch shim;
        if (useShim) {
            auto slot = findModuleNameImport(module);
            const auto original = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetModuleFileNameW"));
            const auto patched = shim.apply(slot, original, reinterpret_cast<void*>(&callerNameProxy));
            if (const auto error = std::get_if<rk::Error>(&patched)) {
                std::cerr << "SHIM_REJECTED: " << error->message << '\n'; FreeLibrary(module); return 6;
            }
            std::cout << "PATCH_APPLIED=nr.caller-name.experiment; atomic verified IAT slot\n";
        }
        const auto directory = std::filesystem::absolute("artifacts/local/nr-probe-data");
        std::filesystem::create_directories(directory);
        std::cout << "Calling Init_Ext; shim=" << (useShim ? "on" : "off") << "; appId=0x0876232c; sdkVersion=0x15; commonInfo=null\n" << std::flush;
        const auto result = init(0x0876232cULL, directory.c_str(), device.Get(), 0x15, nullptr);
        std::cout << "RAW_INIT_RESULT=0x" << std::hex << std::setw(8) << std::setfill('0') << result << std::endl;
        const bool referenceFailure = (result & 0xfff00000U) == 0xbad00000U;
        const auto restore = [&]() -> rk::Result<bool> {
            if (!useShim) return true;
            const auto restored = shim.restore();
            if (std::holds_alternative<bool>(restored)) std::cout << "PATCH_RESTORED=nr.caller-name.experiment\n";
            return restored;
        };
        if (!referenceFailure) {
            const auto retired = rk::retireProbeRuntime([&] {
                const auto released = shutdown(device.Get());
                std::cout << "RAW_SHUTDOWN_RESULT=0x" << std::hex << released << std::endl;
                return released;
            }, restore, [&] { FreeLibrary(module); });
            if (const auto error = std::get_if<rk::Error>(&retired)) {
                std::cerr << "RETIREMENT_FAILED: " << error->message << std::endl;
                ExitProcess(8); // No destructors release a device/module still retained by the runtime.
            }
        } else {
            const auto restored = restore();
            if (std::holds_alternative<rk::Error>(restored)) {
                std::cerr << "SHIM_RESTORE_FAILED; controlled process termination\n" << std::flush;
                ExitProcess(7);
            }
            FreeLibrary(module);
        }
        std::cout << "FEATURE_CREATE=NOT_RUN; EVALUATE=NOT_RUN; GPU_OUTPUT=NOT_RUN\n";
        return referenceFailure ? 5 : 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 9; }
}
