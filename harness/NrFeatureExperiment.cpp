#include "NrFeatureExperiment.hpp"
#include "rk/PatchDescriptor.hpp"
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <vector>

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
using SetResource = void(__cdecl*)(void*, const char*, ID3D12Resource*);
using GetInt = std::uint32_t(__cdecl*)(void*, const char*, int*);

std::uint16_t floatToHalf(float value) noexcept {
    const auto bits=std::bit_cast<std::uint32_t>(value);
    const auto sign=static_cast<std::uint16_t>((bits>>16)&0x8000u);
    auto exponent=static_cast<int>((bits>>23)&0xffu)-127+15;
    auto mantissa=bits&0x7fffffu;
    if(exponent<=0) {
        if(exponent<-10)return sign;
        mantissa=(mantissa|0x800000u)>>static_cast<unsigned>(1-exponent);
        return static_cast<std::uint16_t>(sign+((mantissa+0x1000u)>>13));
    }
    if(exponent>=31)return static_cast<std::uint16_t>(sign|0x7c00u);
    mantissa+=0x1000u;
    if(mantissa&0x800000u) { mantissa=0;++exponent; }
    if(exponent>=31)return static_cast<std::uint16_t>(sign|0x7c00u);
    return static_cast<std::uint16_t>(sign|static_cast<unsigned>(exponent<<10)|
        (mantissa>>13));
}
float halfToFloat(std::uint16_t half) noexcept {
    const auto sign=static_cast<std::uint32_t>(half&0x8000u)<<16;
    auto exponent=static_cast<std::uint32_t>((half>>10)&0x1fu);
    auto mantissa=static_cast<std::uint32_t>(half&0x3ffu);
    std::uint32_t bits{};
    if(exponent==0) {
        if(!mantissa)bits=sign;
        else {
            exponent=1;
            while((mantissa&0x400u)==0) { mantissa<<=1;--exponent; }
            mantissa&=0x3ffu;
            bits=sign|((exponent+127-15)<<23)|(mantissa<<13);
        }
    } else if(exponent==31) bits=sign|0x7f800000u|(mantissa<<13);
    else bits=sign|((exponent+127-15)<<23)|(mantissa<<13);
    return std::bit_cast<float>(bits);
}

D3D12_RESOURCE_DESC textureDesc(unsigned width,unsigned height,DXGI_FORMAT format,
    D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_NONE) noexcept {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment=0;desc.Width=width;desc.Height=height;
    desc.DepthOrArraySize=1;desc.MipLevels=1;desc.Format=format;
    desc.SampleDesc={1,0};desc.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags=flags;
    return desc;
}
ComPtr<ID3D12Resource> createTexture(ID3D12Device* device,
    const D3D12_RESOURCE_DESC& desc) {
    const D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_DEFAULT,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,D3D12_MEMORY_POOL_UNKNOWN,1,1};
    ComPtr<ID3D12Resource> resource;
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,
        D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&resource))))
        stop("EVALUATION_TEXTURE_CREATE");
    return resource;
}
ComPtr<ID3D12Resource> createBuffer(ID3D12Device* device,std::uint64_t size,
    D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state) {
    const D3D12_HEAP_PROPERTIES heap{type,D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,1,1};
    const D3D12_RESOURCE_DESC desc{D3D12_RESOURCE_DIMENSION_BUFFER,0,size,1,1,1,
        DXGI_FORMAT_UNKNOWN,{1,0},D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        D3D12_RESOURCE_FLAG_NONE};
    ComPtr<ID3D12Resource> resource;
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,
        state,nullptr,IID_PPV_ARGS(&resource))))stop("EVALUATION_BUFFER_CREATE");
    return resource;
}
struct TextureUpload {
    ComPtr<ID3D12Resource> buffer;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
};
TextureUpload uploadTexture(ID3D12Device* device,ID3D12GraphicsCommandList* commands,
    ID3D12Resource* target,const void* pixels,std::size_t rowBytes,unsigned rows) {
    const auto desc=target->GetDesc();
    TextureUpload upload;
    std::uint64_t size{};
    device->GetCopyableFootprints(&desc,0,1,0,&upload.footprint,nullptr,nullptr,&size);
    upload.buffer=createBuffer(device,size,D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_GENERIC_READ);
    std::uint8_t* mapped{};
    if(FAILED(upload.buffer->Map(0,nullptr,reinterpret_cast<void**>(&mapped))))
        stop("EVALUATION_UPLOAD_MAP");
    for(unsigned y=0;y<rows;++y)
        std::memcpy(mapped+upload.footprint.Offset+
            static_cast<std::size_t>(y)*upload.footprint.Footprint.RowPitch,
            static_cast<const std::uint8_t*>(pixels)+static_cast<std::size_t>(y)*rowBytes,
            rowBytes);
    upload.buffer->Unmap(0,nullptr);
    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource=upload.buffer.Get();
    source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint=upload.footprint;
    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource=target;destination.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.SubresourceIndex=0;
    commands->CopyTextureRegion(&destination,0,0,0,&source,nullptr);
    return upload;
}
void transition(ID3D12GraphicsCommandList* commands,ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource=resource;
    barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore=before;
    barrier.Transition.StateAfter=after;
    commands->ResourceBarrier(1,&barrier);
}
void submitAndWait(ID3D12Device* device,ID3D12CommandQueue* queue,
    ID3D12GraphicsCommandList* commands,ID3D12Fence* fence,std::uint64_t value) {
    if(FAILED(commands->Close()))stop("COMMAND_CLOSE");
    ID3D12CommandList* lists[]{commands};queue->ExecuteCommandLists(1,lists);
    const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event||FAILED(queue->Signal(fence,value))||
        FAILED(fence->SetEventOnCompletion(value,event)))stop("FENCE_SIGNAL");
    const auto wait=WaitForSingleObject(event,30000);CloseHandle(event);
    if(wait!=WAIT_OBJECT_0||FAILED(device->GetDeviceRemovedReason()))stop("GPU_WAIT");
}

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
    using Evaluate = std::uint32_t(__cdecl*)(ID3D12GraphicsCommandList*, void*, void*, void*);
    using Release = std::uint32_t(__cdecl*)(void*);
    const auto allocate = reinterpret_cast<Allocate>(GetProcAddress(core, "NVSDK_NGX_D3D12_AllocateParameters"));
    const auto destroy = reinterpret_cast<Destroy>(GetProcAddress(core, "NVSDK_NGX_D3D12_DestroyParameters"));
    const auto create = reinterpret_cast<Create>(GetProcAddress(nr, "NVSDK_NGX_D3D12_CreateFeature"));
    const auto evaluate = reinterpret_cast<Evaluate>(GetProcAddress(nr, "NVSDK_NGX_D3D12_EvaluateFeature"));
    const auto release = reinterpret_cast<Release>(GetProcAddress(nr, "NVSDK_NGX_D3D12_ReleaseFeature"));
    if (!allocate || !destroy || !create || !evaluate || !release) stop("EXPORT_MISSING");
    void* parameters = nullptr;
    const auto allocated = allocate(&parameters);
    std::cout << "RAW_PARAMETER_ALLOCATE=0x" << std::hex << allocated << std::endl;
    if (allocated != 1 || !parameters) stop("PARAMETER_ALLOCATE");
    const auto base = reinterpret_cast<std::uintptr_t>(core);
    auto table = *reinterpret_cast<void***>(parameters);
    if (reinterpret_cast<std::uintptr_t>(table) != base + 0xb59b8) stop("PARAMETER_VTABLE");
    const std::pair<unsigned, unsigned> slots[] = {
        {0,0x37a0},{1,0x641b0},{3,0x2f30},{4,0x2eb0},{6,0x2e70},{11,0x3310}};
    for (const auto [slot, rva] : slots)
        if (reinterpret_cast<std::uintptr_t>(table[slot]) != base + rva) stop("PARAMETER_SLOT");
    allocationDevice = device;
    wchar_t fault[32]{};
    GetEnvironmentVariableW(L"RAZKOLBAS_NR_PROBE_FAULT", fault, 32);
    omitOneRelease = std::wstring_view(fault) == L"omit_one_release";
    const auto pointer = method<SetPointer>(parameters, 0);
    const auto resource = method<SetResource>(parameters, 1);
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
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) stop("FENCE_CREATE");
    submitAndWait(device,queue.Get(),commands.Get(),fence.Get(),1);

    constexpr unsigned width=640,height=360;
    wchar_t formatOption[16]{};
    GetEnvironmentVariableW(L"RAZKOLBAS_NR_PROBE_FORMAT",formatOption,16);
    const bool rgba8=std::wstring_view(formatOption)==L"rgba8";
    const auto colorFormat=rgba8?DXGI_FORMAT_R8G8B8A8_UNORM:
        DXGI_FORMAT_R16G16B16A16_FLOAT;
    std::cout<<"EVALUATION_COLOR_FORMAT="<<(rgba8?"R8G8B8A8_UNORM":
        "R16G16B16A16_FLOAT")<<std::endl;
    const auto colorDesc=textureDesc(width,height,colorFormat);
    const auto motionDesc=textureDesc(width,height,DXGI_FORMAT_R16G16_FLOAT);
    const auto depthDesc=textureDesc(width,height,DXGI_FORMAT_R32_FLOAT);
    const auto outputDesc=textureDesc(width,height,colorFormat,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto color=createTexture(device,colorDesc);
    auto motion=createTexture(device,motionDesc);
    auto depth=createTexture(device,depthDesc);
    auto output=createTexture(device,outputDesc);

    const auto colorRowBytes=static_cast<std::size_t>(width)*(rgba8?4:8);
    std::vector<std::uint8_t> colorBytes(colorRowBytes*height);
    std::vector<std::uint8_t> sentinelBytes(colorRowBytes*height);
    std::vector<std::uint16_t> motionPixels(static_cast<std::size_t>(width)*height*2,
        floatToHalf(0.0f));
    std::vector<float> depthPixels(static_cast<std::size_t>(width)*height,0.5f);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        const auto index=(static_cast<std::size_t>(y)*width+x)*4;
        const float checker=((x/8+y/8)&1)?0.1f:0.9f;
        if(rgba8) {
            colorBytes[index+0]=static_cast<std::uint8_t>(checker*255.0f);
            colorBytes[index+1]=static_cast<std::uint8_t>(255*x/(width-1));
            colorBytes[index+2]=static_cast<std::uint8_t>(255*y/(height-1));
            colorBytes[index+3]=255;
        } else {
            auto* colorPixels=reinterpret_cast<std::uint16_t*>(colorBytes.data());
            colorPixels[index+0]=floatToHalf(checker);
            colorPixels[index+1]=floatToHalf(static_cast<float>(x)/(width-1));
            colorPixels[index+2]=floatToHalf(static_cast<float>(y)/(height-1));
            colorPixels[index+3]=floatToHalf(1.0f);
        }
    }
    if(rgba8)std::fill(sentinelBytes.begin(),sentinelBytes.end(),17u);
    else std::fill(reinterpret_cast<std::uint16_t*>(sentinelBytes.data()),
        reinterpret_cast<std::uint16_t*>(sentinelBytes.data()+sentinelBytes.size()),
        floatToHalf(-0.25f));

    if(FAILED(allocator->Reset())||FAILED(commands->Reset(allocator.Get(),nullptr)))
        stop("COMMAND_RESET");
    std::vector<TextureUpload> uploads;
    uploads.emplace_back(uploadTexture(device,commands.Get(),color.Get(),colorBytes.data(),
        colorRowBytes,height));
    uploads.emplace_back(uploadTexture(device,commands.Get(),motion.Get(),motionPixels.data(),
        static_cast<std::size_t>(width)*4,height));
    uploads.emplace_back(uploadTexture(device,commands.Get(),depth.Get(),depthPixels.data(),
        static_cast<std::size_t>(width)*4,height));
    uploads.emplace_back(uploadTexture(device,commands.Get(),output.Get(),sentinelBytes.data(),
        colorRowBytes,height));
    transition(commands.Get(),color.Get(),D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(commands.Get(),motion.Get(),D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(commands.Get(),depth.Get(),D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(commands.Get(),output.Get(),D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    resource(parameters,"DLSSNR.Color",color.Get());
    resource(parameters,"DLSSNR.Output",output.Get());
    resource(parameters,"DLSSNR.MVec",motion.Get());
    resource(parameters,"DLSSNR.Depth",depth.Get());
    for(const char* key:{"Width","DLSSNR.Width","DLSSNR.InputWidth",
        "DLSSNR.ColorSubrectWidth","DLSSNR.MVecSubrectWidth",
        "DLSSNR.DepthSubrectWidth"})uint(parameters,key,width);
    for(const char* key:{"Height","DLSSNR.Height","DLSSNR.InputHeight",
        "DLSSNR.ColorSubrectHeight","DLSSNR.MVecSubrectHeight",
        "DLSSNR.DepthSubrectHeight"})uint(parameters,key,height);
    for(const char* key:{"OutWidth","DLSSNR.OutputWidth",
        "DLSSNR.OutputSubrectWidth"})uint(parameters,key,width);
    for(const char* key:{"OutHeight","DLSSNR.OutputHeight",
        "DLSSNR.OutputSubrectHeight"})uint(parameters,key,height);
    for(const char* key:{"DLSSNR.ColorSubrectBaseX","DLSSNR.ColorSubrectBaseY",
        "DLSSNR.MVecSubrectBaseX","DLSSNR.MVecSubrectBaseY",
        "DLSSNR.DepthSubrectBaseX","DLSSNR.DepthSubrectBaseY",
        "DLSSNR.OutputSubrectBaseX","DLSSNR.OutputSubrectBaseY"})uint(parameters,key,0);
    real(parameters,"DLSSNR.MVecScaleX",1.0f);
    real(parameters,"DLSSNR.MVecScaleY",1.0f);
    real(parameters,"DLSSNR.ScalingRatio",1.0f);
    real(parameters,"DLSSNR.Scale",1.0f);
    integer(parameters,"DLSSNR.Upscaling",0);
    integer(parameters,"DLSSNR.Enabled",1);
    integer(parameters,"DLSSNR.Reset",1);
    integer(parameters,"DLSSNR.DepthInverted",0);
    real(parameters,"DLSSNR.Intensity",1.0f);
    real(parameters,"DLSSNR.LocalToneStrength",1.0f);
    real(parameters,"DLSSNR.LocalStructureStrength",1.0f);
    real(parameters,"DLSSNR.SkinStructureStrength",1.0f);
    integer(parameters,"DLSSNR.UseAutoMask",0);
    uint(parameters,"DLSSNR.Style",0);
    integer(parameters,"DLSSNR.UICorrection",0);
    integer(parameters,"DLSS.Indicator.Invert.X.Axis",0);
    integer(parameters,"DLSS.Indicator.Invert.Y.Axis",0);

    const auto evaluated=evaluate(commands.Get(),feature,parameters,nullptr);
    std::cout<<"RAW_FEATURE_EVALUATE=0x"<<std::hex<<evaluated<<std::endl;
    if(evaluated!=1)stop("FEATURE_EVALUATE_FAILED");
    transition(commands.Get(),output.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT readbackFootprint{};
    std::uint64_t readbackSize{};
    device->GetCopyableFootprints(&outputDesc,0,1,0,&readbackFootprint,nullptr,nullptr,
        &readbackSize);
    auto readback=createBuffer(device,readbackSize,D3D12_HEAP_TYPE_READBACK,
        D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION readbackLocation{};
    readbackLocation.pResource=readback.Get();
    readbackLocation.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    readbackLocation.PlacedFootprint=readbackFootprint;
    D3D12_TEXTURE_COPY_LOCATION outputLocation{};
    outputLocation.pResource=output.Get();
    outputLocation.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    outputLocation.SubresourceIndex=0;
    commands->CopyTextureRegion(&readbackLocation,0,0,0,&outputLocation,nullptr);
    submitAndWait(device,queue.Get(),commands.Get(),fence.Get(),2);
    uploads.clear();

    void* mappedAddress{};
    if(FAILED(readback->Map(0,nullptr,&mappedAddress)))stop("EVALUATION_READBACK_MAP");
    const auto* mapped=static_cast<const std::uint8_t*>(mappedAddress);
    std::uint64_t checksum=1469598103934665603ULL;
    std::size_t changedFromSentinel{},changedFromInput{},finiteValues{};
    double absoluteDifference{};
    for(unsigned y=0;y<height;++y)if(rgba8) {
        const auto* row=mapped+readbackFootprint.Offset+
            static_cast<std::size_t>(y)*readbackFootprint.Footprint.RowPitch;
        const auto* inputRow=colorBytes.data()+static_cast<std::size_t>(y)*colorRowBytes;
        const auto* sentinelRow=sentinelBytes.data()+static_cast<std::size_t>(y)*colorRowBytes;
        for(std::size_t x=0;x<colorRowBytes;++x) {
            checksum=(checksum^row[x])*1099511628211ULL;
            changedFromSentinel+=row[x]!=sentinelRow[x];
            changedFromInput+=row[x]!=inputRow[x];
            ++finiteValues;
            absoluteDifference+=std::abs(static_cast<int>(row[x])-inputRow[x])/255.0;
        }
    } else {
        const auto* row=reinterpret_cast<const std::uint16_t*>(mapped+
            readbackFootprint.Offset+static_cast<std::size_t>(y)*
            readbackFootprint.Footprint.RowPitch);
        const auto* inputRow=reinterpret_cast<const std::uint16_t*>(colorBytes.data()+
            static_cast<std::size_t>(y)*colorRowBytes);
        const auto* sentinelRow=reinterpret_cast<const std::uint16_t*>(sentinelBytes.data()+
            static_cast<std::size_t>(y)*colorRowBytes);
        for(unsigned x=0;x<width*4;++x) {
            const auto value=row[x];
            checksum=(checksum^value)*1099511628211ULL;
            changedFromSentinel+=value!=sentinelRow[x];
            changedFromInput+=value!=inputRow[x];
            const auto decoded=halfToFloat(value);
            finiteValues+=std::isfinite(decoded);
            absoluteDifference+=std::abs(decoded-halfToFloat(inputRow[x]));
        }
    }
    readback->Unmap(0,nullptr);
    const auto values=static_cast<std::size_t>(width)*height*4;
    std::cout<<"GPU_OUTPUT_CHECKSUM=0x"<<std::hex<<checksum<<std::dec
        <<"; finite="<<finiteValues<<"/"<<values
        <<"; changed_from_sentinel="<<changedFromSentinel
        <<"; changed_from_input="<<changedFromInput
        <<"; absolute_difference="<<absoluteDifference<<std::endl;
    if(finiteValues!=values||changedFromSentinel<values/100||
        changedFromInput<values/1000||absoluteDifference<1.0)
        stop("GPU_OUTPUT_TRIVIAL_OR_INVALID");

    const auto released = release(feature);
    std::cout << "RAW_FEATURE_RELEASE=0x" << std::hex << released << "; callbacks_released="
              << std::dec << releases.load() << std::endl;
    if (released != 1) stop("FEATURE_RELEASE");
    if (allocations.load() != releases.load()) stop("RESOURCE_CALLBACK_RETIREMENT_UNBALANCED");
    const auto destroyed = destroy(parameters);
    std::cout << "RAW_PARAMETER_DESTROY=0x" << std::hex << destroyed << std::endl;
    if (destroyed != 1) stop("PARAMETER_DESTROY");
    std::cout << "FEATURE_CREATION_RETIREMENT=PASS; EVALUATE=PASS; GPU_OUTPUT=NONTRIVIAL" << std::endl;
}
