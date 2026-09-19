#include "NrFeatureExperiment.hpp"
#include "rk/PatchDescriptor.hpp"
#include <wrl/client.h>
#include <atomic>
#include <fstream>
#include <exception>
#include <iostream>
#include <stdexcept>

namespace {
using Microsoft::WRL::ComPtr;
std::atomic<ID3D12Device*> allocationDevice{nullptr}; // One probe; device lives through shutdown.
std::atomic<unsigned> allocations{0}, releases{0};
std::atomic<bool> omitOneRelease{false}; // Opt-in probe fault, never in the plugin.

[[noreturn]] void stop(const char* reason) {
    std::cerr << "FEATURE_EXPERIMENT_STOP=" << reason << std::endl;
    ExitProcess(10); // Preserve retained GPU/module ownership until process teardown.
}

// Fallout allocation callback 0x31370: RCX=resource descriptor, EDX=state,
// R8=heap properties, R9=output. Forwards to CreateCommittedResource, no return.
void __cdecl allocateResource(const D3D12_RESOURCE_DESC* desc, D3D12_RESOURCE_STATES state,
                             const D3D12_HEAP_PROPERTIES* heap, ID3D12Resource** output) {
    if (!output) return;
    *output = nullptr;
    const auto device = allocationDevice.load();
    if (!device || !desc || !heap) return;
    const auto hr = device->CreateCommittedResource(heap, D3D12_HEAP_FLAG_NONE,
        desc, state, nullptr, IID_PPV_ARGS(output));
    if (SUCCEEDED(hr)) {
        ++allocations;
        // Reference callback requests high residency priority for default heaps.
        if (heap->Type == D3D12_HEAP_TYPE_DEFAULT) {
            ComPtr<ID3D12Device1> device1;
            if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&device1)))) {
                ID3D12Pageable* pageable = *output;
                const auto priority = D3D12_RESIDENCY_PRIORITY_HIGH;
                device1->SetResidencyPriority(1, &pageable, &priority);
            }
        }
    } else {
        std::cerr << "RESOURCE_ALLOC_HRESULT=0x" << std::hex << hr << std::endl;
    }
}

// Leaf reference 0x34e50: null check and tail-call IUnknown::Release.
void __cdecl releaseResource(ID3D12Resource* resource) {
    if (resource) {
        if (omitOneRelease.exchange(false)) {
            std::cerr << "INJECTED_OMITTED_RELEASE" << std::endl;
            return; // Retirement count must reject PASS; child teardown reclaims it.
        }
        ++releases;
        resource->Release();
    }
}

template<class Fn> Fn method(void* object, std::size_t slot) {
    return reinterpret_cast<Fn>((*reinterpret_cast<void***>(object))[slot]);
}
using SetUInt = void(__cdecl*)(void*, const char*, unsigned);
using SetInt = void(__cdecl*)(void*, const char*, int);
using SetFloat = void(__cdecl*)(void*, const char*, float);
using SetPointer = void(__cdecl*)(void*, const char*, void*);
using GetInt = std::uint32_t(__cdecl*)(void*, const char*, int*);

// Reference 0x33da0 checks Upscaling, propagates Get failure, then writes 1.0f.
std::uint32_t __cdecl computeScalingRatio(void* parameters) {
    if (!parameters) return 0xbad00005;
    int upscaling = 0;
    const auto result = method<GetInt>(parameters, 11)(parameters, "DLSSNR.Upscaling", &upscaling);
    if ((result & 0xfff00000U) == 0xbad00000U) return result;
    method<SetFloat>(parameters, 6)(parameters, "DLSSNR.ScalingRatio", 1.0f);
    return 1;
}
}

void runNrFeatureExperiment(HMODULE nr, ID3D12Device* device, const std::filesystem::path& corePath) {
    const auto absolute = std::filesystem::absolute(corePath);
    const auto lock = CreateFileW(absolute.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (lock == INVALID_HANDLE_VALUE) stop("CORE_FILE_OPEN");
    std::ifstream input(absolute, std::ios::binary);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
    if (rk::sha256(bytes) != "91d3742f0df3f2dd85141fa39ea257f7b9243c3e6755cd7fea894c721e7491aa")
        stop("CORE_HASH_MISMATCH");
    const auto core = LoadLibraryExW(absolute.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    CloseHandle(lock);
    if (!core) stop("CORE_LOAD");
    using Allocate = std::uint32_t(__cdecl*)(void**);
    using Destroy = std::uint32_t(__cdecl*)(void*);
    using Create = std::uint32_t(__cdecl*)(ID3D12GraphicsCommandList*, unsigned, void*, void**);
    using Release = std::uint32_t(__cdecl*)(void*);
    const auto allocate = reinterpret_cast<Allocate>(GetProcAddress(core, "NVSDK_NGX_D3D12_AllocateParameters"));
    const auto destroy = reinterpret_cast<Destroy>(GetProcAddress(core, "NVSDK_NGX_D3D12_DestroyParameters"));
    const auto create = reinterpret_cast<Create>(GetProcAddress(nr, "NVSDK_NGX_D3D12_CreateFeature"));
    const auto release = reinterpret_cast<Release>(GetProcAddress(nr, "NVSDK_NGX_D3D12_ReleaseFeature"));
    if (!allocate || !destroy || !create || !release) stop("EXPORT_MISSING");
    void* parameters = nullptr;
    const auto allocated = allocate(&parameters);
    std::cout << "RAW_PARAMETER_ALLOCATE=0x" << std::hex << allocated << std::endl;
    if (allocated != 1 || !parameters) stop("PARAMETER_ALLOCATE");
    const auto base = reinterpret_cast<std::uintptr_t>(core);
    auto table = *reinterpret_cast<void***>(parameters);
    if (reinterpret_cast<std::uintptr_t>(table) != base + 0xb59b8) stop("PARAMETER_VTABLE");
    const std::pair<unsigned, unsigned> slots[] = {{0,0x37a0},{3,0x2f30},{4,0x2eb0},{6,0x2e70},{11,0x3310}};
    for (const auto [slot, rva] : slots)
        if (reinterpret_cast<std::uintptr_t>(table[slot]) != base + rva) stop("PARAMETER_SLOT");
    allocationDevice = device;
    wchar_t fault[32]{};
    GetEnvironmentVariableW(L"RAZKOLBAS_NR_PROBE_FAULT", fault, 32);
    omitOneRelease = std::wstring_view(fault) == L"omit_one_release";
    const auto pointer = method<SetPointer>(parameters, 0);
    const auto uint = method<SetUInt>(parameters, 4);
    const auto integer = method<SetInt>(parameters, 3);
    const auto real = method<SetFloat>(parameters, 6);
    pointer(parameters, "ResourceAllocCallback", reinterpret_cast<void*>(&allocateResource));
    pointer(parameters, "ResourceReleaseCallback", reinterpret_cast<void*>(&releaseResource));
    pointer(parameters, "DLSSNRComputeScalingRatioCallback", reinterpret_cast<void*>(&computeScalingRatio));
    // Concrete small 1:1 creation case using the reference's exact key spelling.
    for (const char* key : {"Width", "OutWidth", "DLSSNR.Width", "DLSSNR.InputWidth",
                           "DLSSNR.OutputWidth", "DLSSNR.Output.Width"}) uint(parameters, key, 640);
    for (const char* key : {"Height", "OutHeight", "DLSSNR.Height", "DLSSNR.InputHeight",
                           "DLSSNR.OutputHeight", "DLSSNR.Output.Height"}) uint(parameters, key, 360);
    real(parameters, "DLSSNR.ScalingRatio", 1.0f);
    real(parameters, "DLSSNR.Scale", 1.0f);
    integer(parameters, "DLSSNR.Upscaling", 0);
    uint(parameters, "DLSSNR.Hint.Render.Preset", 0);
    uint(parameters, "PerfQualityValue", 2); // Reference mode 3 -> stored value 2.
    integer(parameters, "DLSS.Feature.Create.Flags", 0x42);
    uint(parameters, "CreationNodeMask", 1);
    uint(parameters, "VisibilityNodeMask", 1);

    ComPtr<ID3D12CommandQueue> queue;
    const D3D12_COMMAND_QUEUE_DESC queueDesc{};
    if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)))) stop("QUEUE_CREATE");
    ComPtr<ID3D12CommandAllocator> allocator;
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)))) stop("COMMAND_ALLOCATOR");
    ComPtr<ID3D12GraphicsCommandList> commands;
    if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
                                       IID_PPV_ARGS(&commands)))) stop("COMMAND_LIST");
    ComPtr<ID3D12Fence> fence;
    struct StopBeforeGpuOwnersUnwind {
        int initial = std::uncaught_exceptions();
        ~StopBeforeGpuOwnersUnwind() {
            if (std::uncaught_exceptions() > initial) stop("EXCEPTION_WITH_GPU_OWNERS");
        }
    } unwindGuard;
    void* feature = nullptr;
    std::cout << "FEATURE_ATTEMPT=0x12; size=640x360; NR export direct; core used only for parameters" << std::endl;
    const auto result = create(commands.Get(), 0x12, parameters, &feature);
    std::cout << "RAW_FEATURE_CREATE=0x" << std::hex << result << "; handle_nonnull=" << (feature != nullptr)
              << "; allocations=" << std::dec << allocations.load() << std::endl;
    if (result != 1 || !feature) stop("FEATURE_CREATE_FAILED");
    if (FAILED(commands->Close())) stop("COMMAND_CLOSE");
    ID3D12CommandList* lists[] = {commands.Get()};
    queue->ExecuteCommandLists(1, lists);
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) stop("FENCE_CREATE");
    const auto event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event || FAILED(queue->Signal(fence.Get(), 1)) || FAILED(fence->SetEventOnCompletion(1, event))) stop("FENCE_SIGNAL");
    if (WaitForSingleObject(event, 20000) != WAIT_OBJECT_0 || FAILED(device->GetDeviceRemovedReason())) stop("GPU_WAIT");
    CloseHandle(event);
    const auto released = release(feature);
    std::cout << "RAW_FEATURE_RELEASE=0x" << std::hex << released << "; callbacks_released="
              << std::dec << releases.load() << std::endl;
    if (released != 1) stop("FEATURE_RELEASE");
    if (allocations.load() != releases.load()) stop("RESOURCE_CALLBACK_RETIREMENT_UNBALANCED");
    const auto destroyed = destroy(parameters);
    std::cout << "RAW_PARAMETER_DESTROY=0x" << std::hex << destroyed << std::endl;
    if (destroyed != 1) stop("PARAMETER_DESTROY");
    std::cout << "FEATURE_CREATION_AND_RETIREMENT=PASS; EVALUATE=NOT_RUN; GPU_OUTPUT=NOT_RUN" << std::endl;
}
