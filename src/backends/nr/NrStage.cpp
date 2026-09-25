#include "rk/NrStage.hpp"
#include "rk/PatchDescriptor.hpp"
#include "rk/PointerPatch.hpp"
#include <nvsdk_ngx.h>
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>
#include <vector>

namespace rk {
namespace {
using Microsoft::WRL::ComPtr;
namespace fs=std::filesystem;
constexpr std::string_view nrHash=
    "91ea4143d9ed1cb90b11a2851cfc68dabe7d1e7414f8dfaa8016d86b99e40be7";
std::atomic<ID3D12Device*> allocationDevice{};
std::atomic<HMODULE> callerIdentityModule{};

struct NrRuntimeSettings {
    bool enabled{};
    unsigned style{};
    float intensity{1.0f},tone{1.0f},structure{1.0f},skin{-1.0f};
    bool autoMask{},uiCorrection{};
    bool operator==(const NrRuntimeSettings&) const noexcept=default;
};

Result<NrRuntimeSettings> runtimeSettings(const Settings& settings) {
    NrRuntimeSettings result;
    result.enabled=settings.get<bool>("NeuralRendering.Enabled");
    const auto style=settings.get<std::int64_t>("NeuralRendering.Style");
    result.style=static_cast<unsigned>(style);
    result.intensity=static_cast<float>(settings.get<double>("NeuralRendering.Intensity"));
    result.tone=static_cast<float>(settings.get<double>("NeuralRendering.LocalToneStrength"));
    result.structure=static_cast<float>(
        settings.get<double>("NeuralRendering.LocalStructureStrength"));
    const auto& skin=settings.get<Text>("NeuralRendering.SkinStructureStrength").value;
    if(skin=="Auto"||skin=="-1")result.skin=result.structure;
    else {
        char* end{};
        result.skin=std::strtof(skin.c_str(),&end);
        if(!end||end==skin.c_str()||*end!='\0')
            return Error{ErrorCode::InvalidInput,"NR skin strength is invalid"};
    }
    result.autoMask=settings.get<bool>("NeuralRendering.UseAutoMask");
    result.uiCorrection=settings.get<bool>("NeuralRendering.NativeUICorrection");
    if(style<0||style>7||!std::isfinite(result.intensity)||
       !std::isfinite(result.tone)||!std::isfinite(result.structure)||
       !std::isfinite(result.skin)||result.intensity<0.0f||result.intensity>2.0f||
       result.tone<0.0f||result.tone>2.0f||result.structure<0.0f||
       result.structure>2.0f||result.skin>2.0f||
       (result.skin<0.0f&&result.skin!=-1.0f))
        return Error{ErrorCode::InvalidInput,"NR live setting is outside its supported range"};
    return result;
}

void __cdecl allocateNrResource(const D3D12_RESOURCE_DESC* desc,
    D3D12_RESOURCE_STATES state,const D3D12_HEAP_PROPERTIES* heap,
    ID3D12Resource** output) {
    if(!output)return;
    *output=nullptr;
    const auto device=allocationDevice.load(std::memory_order_acquire);
    if(!device||!desc||!heap)return;
    device->CreateCommittedResource(heap,D3D12_HEAP_FLAG_NONE,desc,state,nullptr,
        IID_PPV_ARGS(output));
}
void __cdecl releaseNrResource(ID3D12Resource* resource) {
    if(resource)resource->Release();
}
std::uint32_t __cdecl computeScalingRatio(NVSDK_NGX_Parameter* parameters) {
    if(!parameters)return 0xbad00005;
    int upscaling{};
    const auto result=parameters->Get("DLSSNR.Upscaling",&upscaling);
    if((static_cast<std::uint32_t>(result)&0xfff00000u)==0xbad00000u)
        return static_cast<std::uint32_t>(result);
    parameters->Set("DLSSNR.ScalingRatio",1.0f);
    return 1;
}
DWORD WINAPI callerNameProxy(HMODULE module,LPWSTR buffer,DWORD size) {
    if(module!=callerIdentityModule.load(std::memory_order_relaxed))
        return GetModuleFileNameW(module,buffer,size);
    constexpr wchar_t name[]=L"nvngx.dll";
    if(!buffer||!size){SetLastError(ERROR_INSUFFICIENT_BUFFER);return 0;}
    const DWORD copied=size>9?9:size-1;
    std::copy_n(name,copied,buffer);buffer[copied]=0;
    if(copied!=9){SetLastError(ERROR_INSUFFICIENT_BUFFER);return size;}
    return copied;
}
void** findModuleNameImport(HMODULE module) {
    const auto base=reinterpret_cast<std::uint8_t*>(module);
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return nullptr;
    const auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE)return nullptr;
    const auto directory=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if(!directory.VirtualAddress||directory.VirtualAddress>=nt->OptionalHeader.SizeOfImage||
       directory.Size>nt->OptionalHeader.SizeOfImage-directory.VirtualAddress)return nullptr;
    const auto imports=reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(
        base+directory.VirtualAddress);
    void** found{};
    for(std::size_t i=0;(i+1)*sizeof(IMAGE_IMPORT_DESCRIPTOR)<=directory.Size&&
        imports[i].Name;++i) {
        if(!imports[i].OriginalFirstThunk)continue;
        const auto names=reinterpret_cast<const IMAGE_THUNK_DATA64*>(
            base+imports[i].OriginalFirstThunk);
        auto slots=reinterpret_cast<IMAGE_THUNK_DATA64*>(base+imports[i].FirstThunk);
        for(std::size_t j=0;names[j].u1.AddressOfData;++j) {
            if(IMAGE_SNAP_BY_ORDINAL64(names[j].u1.Ordinal))continue;
            const auto entry=reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(
                base+names[j].u1.AddressOfData);
            if(std::string_view(reinterpret_cast<const char*>(entry->Name))==
               "GetModuleFileNameW") {
                if(found)return nullptr;
                found=reinterpret_cast<void**>(&slots[j].u1.Function);
            }
        }
    }
    return found;
}
fs::path moduleDirectory() {
    HMODULE self{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&moduleDirectory),&self))return {};
    std::array<wchar_t,32768> path{};
    const auto length=GetModuleFileNameW(self,path.data(),static_cast<DWORD>(path.size()));
    return length&&length<path.size()?fs::path(path.data()).parent_path():fs::path{};
}
Result<bool> exactNrRuntime(const fs::path& path) {
    std::error_code error;
    if(!fs::is_regular_file(path,error)||error)
        return Error{ErrorCode::Unavailable,"Pinned fast-FP16 NR runtime is missing"};
    const auto size=fs::file_size(path,error);
    if(error||size!=165840496)
        return Error{ErrorCode::Unavailable,"Fast-FP16 NR runtime size differs"};
    std::ifstream input(path,std::ios::binary);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    if(sha256(bytes)!=nrHash)
        return Error{ErrorCode::Conflict,"Fast-FP16 NR runtime hash differs"};
    return true;
}
}

struct NrStage::Impl {
    using Init=std::uint32_t(__cdecl*)(std::uint64_t,const wchar_t*,ID3D12Device*,
        std::uint32_t,const void*);
    using Shutdown=std::uint32_t(__cdecl*)(ID3D12Device*);
    using Allocate=std::uint32_t(__cdecl*)(NVSDK_NGX_Parameter**);
    using Destroy=std::uint32_t(__cdecl*)(NVSDK_NGX_Parameter*);
    using Create=std::uint32_t(__cdecl*)(ID3D12GraphicsCommandList*,unsigned,
        NVSDK_NGX_Parameter*,NVSDK_NGX_Handle**);
    using Evaluate=std::uint32_t(__cdecl*)(ID3D12GraphicsCommandList*,
        NVSDK_NGX_Handle*,NVSDK_NGX_Parameter*,void*);
    using Release=std::uint32_t(__cdecl*)(NVSDK_NGX_Handle*);
    struct SharedTexture {
        ComPtr<ID3D11Texture2D> d11;
        ComPtr<ID3D12Resource> d12;
    } color,motion,depth,output;

    bool configured{},initialized{},runtimeInitialized{},disabled{};
    bool retirementUncertain{};
    unsigned preset{},passes{1};
    mutable std::mutex runtimeMutex;
    NrRuntimeSettings runtime;
    std::uint64_t runtimeGeneration{};
    bool runtimeResetPending{};
    UINT width{},height{};
    std::uint64_t fenceValue{},submitted{};
    HMODULE nr{},core{};
    HANDLE runtimeFile{INVALID_HANDLE_VALUE};
    PointerPatch shim;
    Init init{};Shutdown shutdown{};Allocate allocate{};Destroy destroy{};
    Create create{};Evaluate evaluate{};Release release{};
    NVSDK_NGX_Parameter* parameters{};
    NVSDK_NGX_Handle* feature{};
    ComPtr<ID3D11Device5> device11;
    ComPtr<ID3D11DeviceContext4> context11;
    ComPtr<ID3D11Fence> fence11;
    ComPtr<ID3D12Device> device12;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commands;
    ComPtr<ID3D12Fence> fence12;

    Result<SharedTexture> makeShared(DXGI_FORMAT format,UINT bindFlags) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;
        desc.Format=format;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
        desc.BindFlags=bindFlags;
        desc.MiscFlags=D3D11_RESOURCE_MISC_SHARED|D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
        SharedTexture result;
        if(FAILED(device11->CreateTexture2D(&desc,nullptr,&result.d11)))
            return Error{ErrorCode::Unavailable,"Cannot create shared NR D3D11 texture"};
        ComPtr<IDXGIResource1> shared;
        if(FAILED(result.d11.As(&shared)))
            return Error{ErrorCode::Unsupported,"NR texture has no NT sharing interface"};
        HANDLE handle{};
        const auto created=shared->CreateSharedHandle(nullptr,
            DXGI_SHARED_RESOURCE_READ|DXGI_SHARED_RESOURCE_WRITE,nullptr,&handle);
        if(FAILED(created)||!handle)
            return Error{ErrorCode::Unavailable,"Cannot create shared NR texture handle"};
        const auto opened=device12->OpenSharedHandle(handle,IID_PPV_ARGS(&result.d12));
        CloseHandle(handle);
        if(FAILED(opened)||!result.d12)
            return Error{ErrorCode::Unavailable,"Cannot open NR texture on D3D12 device"};
        return result;
    }
    void barrier(ID3D12Resource* resource,D3D12_RESOURCE_STATES before,
        D3D12_RESOURCE_STATES after) {
        D3D12_RESOURCE_BARRIER value{};
        value.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        value.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after};
        commands->ResourceBarrier(1,&value);
    }
    Result<bool> waitCpu(std::uint64_t value) {
        if(fence12->GetCompletedValue()>=value)return true;
        const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if(!event)return Error{ErrorCode::Unavailable,"Cannot create NR fence event"};
        const auto armed=fence12->SetEventOnCompletion(value,event);
        const auto waited=SUCCEEDED(armed)?WaitForSingleObject(event,5000):WAIT_FAILED;
        CloseHandle(event);
        if(waited!=WAIT_OBJECT_0||FAILED(device12->GetDeviceRemovedReason()))
            return Error{ErrorCode::DeviceRemoved,"NR D3D12 work did not retire"};
        return true;
    }
    Result<bool> submitAndWait() {
        if(FAILED(commands->Close()))
            return Error{ErrorCode::Unavailable,"Cannot close NR command list"};
        ID3D12CommandList* lists[]{commands.Get()};queue->ExecuteCommandLists(1,lists);
        const auto value=++fenceValue;
        if(FAILED(queue->Signal(fence12.Get(),value))) {
            retirementUncertain=true;
            return Error{ErrorCode::DeviceRemoved,"Cannot signal NR completion"};
        }
        const auto waited=waitCpu(value);
        retirementUncertain=std::holds_alternative<Error>(waited);
        return waited;
    }
    Result<bool> createFeature() {
        auto* p=parameters;
        p->Set("ResourceAllocCallback",reinterpret_cast<void*>(&allocateNrResource));
        p->Set("ResourceReleaseCallback",reinterpret_cast<void*>(&releaseNrResource));
        p->Set("DLSSNRComputeScalingRatioCallback",reinterpret_cast<void*>(&computeScalingRatio));
        for(const char* key:{"Width","OutWidth","DLSSNR.Width","DLSSNR.InputWidth",
            "DLSSNR.OutputWidth","DLSSNR.Output.Width"})p->Set(key,width);
        for(const char* key:{"Height","OutHeight","DLSSNR.Height","DLSSNR.InputHeight",
            "DLSSNR.OutputHeight","DLSSNR.Output.Height"})p->Set(key,height);
        p->Set("DLSSNR.ScalingRatio",1.0f);p->Set("DLSSNR.Scale",1.0f);
        p->Set("DLSSNR.Upscaling",0);p->Set("DLSSNR.Hint.Render.Preset",preset);
        p->Set("PerfQualityValue",2u);p->Set("DLSS.Feature.Create.Flags",0x42);
        p->Set("CreationNodeMask",1u);p->Set("VisibilityNodeMask",1u);
        const auto result=create(commands.Get(),0x12,p,&feature);
        if(NVSDK_NGX_FAILED(static_cast<NVSDK_NGX_Result>(result))||!feature)
            return Error{ErrorCode::Unavailable,"Fast-FP16 NR feature creation failed"};
        return submitAndWait();
    }
    Result<bool> rebuild(UINT newWidth,UINT newHeight) {
        if(feature) {
            const auto result=release(feature);
            if(NVSDK_NGX_FAILED(static_cast<NVSDK_NGX_Result>(result)))
                return Error{ErrorCode::Unavailable,"NR feature release failed"};
            feature=nullptr;
        }
        color={};motion={};depth={};output={};width=newWidth;height=newHeight;
        auto made=makeShared(DXGI_FORMAT_R8G8B8A8_UNORM,D3D11_BIND_SHADER_RESOURCE);
        if(const auto error=std::get_if<Error>(&made))return *error;
        color=std::move(std::get<SharedTexture>(made));
        made=makeShared(DXGI_FORMAT_R16G16_FLOAT,D3D11_BIND_SHADER_RESOURCE);
        if(const auto error=std::get_if<Error>(&made))return *error;
        motion=std::move(std::get<SharedTexture>(made));
        made=makeShared(DXGI_FORMAT_R32_FLOAT,D3D11_BIND_SHADER_RESOURCE);
        if(const auto error=std::get_if<Error>(&made))return *error;
        depth=std::move(std::get<SharedTexture>(made));
        made=makeShared(DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS);
        if(const auto error=std::get_if<Error>(&made))return *error;
        output=std::move(std::get<SharedTexture>(made));
        if(FAILED(allocator->Reset())||FAILED(commands->Reset(allocator.Get(),nullptr)))
            return Error{ErrorCode::Unavailable,"Cannot reset NR creation commands"};
        return createFeature();
    }
    Result<bool> start(ID3D11Device* device,ID3D11DeviceContext* context) {
        if(FAILED(device->QueryInterface(IID_PPV_ARGS(&device11)))||
           FAILED(context->QueryInterface(IID_PPV_ARGS(&context11))))
            return Error{ErrorCode::Unsupported,"D3D11 fence interfaces are unavailable"};
        ComPtr<IDXGIDevice> dxgiDevice;ComPtr<IDXGIAdapter> adapter;
        if(FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice)))||
           FAILED(dxgiDevice->GetAdapter(&adapter))||
           FAILED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,
               IID_PPV_ARGS(&device12))))
            return Error{ErrorCode::Unsupported,"Cannot create same-adapter NR D3D12 device"};
        const D3D12_COMMAND_QUEUE_DESC queueDesc{};
        if(FAILED(device12->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&queue)))||
           FAILED(device12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
               IID_PPV_ARGS(&allocator)))||
           FAILED(device12->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,
               allocator.Get(),nullptr,IID_PPV_ARGS(&commands)))||
           FAILED(device11->CreateFence(0,D3D11_FENCE_FLAG_SHARED,IID_PPV_ARGS(&fence11))))
            return Error{ErrorCode::Unavailable,"Cannot create NR queue or shared fence"};
        if(FAILED(commands->Close()))
            return Error{ErrorCode::Unavailable,"Cannot close initial NR command list"};
        HANDLE fenceHandle{};
        const auto shared=fence11->CreateSharedHandle(nullptr,GENERIC_ALL,nullptr,&fenceHandle);
        if(FAILED(shared)||!fenceHandle)
            return Error{ErrorCode::Unavailable,"Cannot share NR interop fence"};
        const auto opened=device12->OpenSharedHandle(fenceHandle,IID_PPV_ARGS(&fence12));
        CloseHandle(fenceHandle);
        if(FAILED(opened))return Error{ErrorCode::Unavailable,"Cannot open NR interop fence"};

        const auto directory=moduleDirectory();
        const auto runtimePath=directory/L"RazKolbasRuntime"/L"nvngx_dlssnr.dll";
        if(const auto checked=exactNrRuntime(runtimePath);
           const auto error=std::get_if<Error>(&checked))return *error;
        runtimeFile=CreateFileW(runtimePath.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,
            OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(runtimeFile==INVALID_HANDLE_VALUE)
            return Error{ErrorCode::Unavailable,"Cannot lock fast-FP16 NR runtime"};
        nr=LoadLibraryExW(runtimePath.c_str(),nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!nr)return Error{ErrorCode::Unavailable,"Cannot load fast-FP16 NR runtime"};
        init=reinterpret_cast<Init>(GetProcAddress(nr,"NVSDK_NGX_D3D12_Init_Ext"));
        shutdown=reinterpret_cast<Shutdown>(GetProcAddress(nr,"NVSDK_NGX_D3D12_Shutdown1"));
        create=reinterpret_cast<Create>(GetProcAddress(nr,"NVSDK_NGX_D3D12_CreateFeature"));
        evaluate=reinterpret_cast<Evaluate>(GetProcAddress(nr,"NVSDK_NGX_D3D12_EvaluateFeature"));
        release=reinterpret_cast<Release>(GetProcAddress(nr,"NVSDK_NGX_D3D12_ReleaseFeature"));
        core=GetModuleHandleW(L"_nvngx.dll");if(!core)core=GetModuleHandleW(L"nvngx.dll");
        allocate=core?reinterpret_cast<Allocate>(GetProcAddress(core,
            "NVSDK_NGX_D3D12_AllocateParameters")):nullptr;
        destroy=core?reinterpret_cast<Destroy>(GetProcAddress(core,
            "NVSDK_NGX_D3D12_DestroyParameters")):nullptr;
        if(!init||!shutdown||!create||!evaluate||!release||!allocate||!destroy)
            return Error{ErrorCode::Unavailable,"NR or D3D12 driver-core export is missing"};
        auto* slot=findModuleNameImport(nr);
        HMODULE callerModule{};
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&callerNameProxy),&callerModule))
            return Error{ErrorCode::Unavailable,"Cannot identify NR caller module"};
        callerIdentityModule.store(callerModule,std::memory_order_relaxed);
        const auto original=reinterpret_cast<void*>(GetProcAddress(
            GetModuleHandleW(L"kernel32.dll"),"GetModuleFileNameW"));
        if(const auto patched=shim.apply(slot,original,reinterpret_cast<void*>(&callerNameProxy));
           const auto error=std::get_if<Error>(&patched))return *error;
        std::error_code filesystemError;
        const auto data=fs::temp_directory_path(filesystemError)/L"RazKolbasNrData";
        if(filesystemError)return Error{ErrorCode::Io,"Cannot locate NR data directory"};
        fs::create_directories(data,filesystemError);
        if(filesystemError)return Error{ErrorCode::Io,"Cannot create NR data directory"};
        allocationDevice.store(device12.Get(),std::memory_order_release);
        const auto initResult=init(0x0876232cULL,data.c_str(),device12.Get(),0x15,nullptr);
        if(NVSDK_NGX_FAILED(static_cast<NVSDK_NGX_Result>(initResult)))
            return Error{ErrorCode::Unavailable,"Fast-FP16 NR Init_Ext failed"};
        runtimeInitialized=true;
        const auto allocated=allocate(&parameters);
        if(NVSDK_NGX_FAILED(static_cast<NVSDK_NGX_Result>(allocated))||!parameters)
            return Error{ErrorCode::Unavailable,"NR parameters are unavailable"};
        initialized=true;return true;
    }
};

NrStage::NrStage():impl_(std::make_unique<Impl>()){}
NrStage::~NrStage(){
    if(!impl_)return;
    const auto stopped=stop();
    if(std::holds_alternative<Error>(stopped)&&impl_->retirementUncertain)
        impl_.release();
}
Result<bool> NrStage::configure(const Settings& settings) {
    if(impl_->configured||impl_->initialized)
        return Error{ErrorCode::Conflict,"Cannot reconfigure NR stage"};
    const auto decoded=runtimeSettings(settings);
    if(const auto error=std::get_if<Error>(&decoded))return *error;
    const auto runtime=std::get<NrRuntimeSettings>(decoded);
    if(runtime.enabled&&
       settings.get<Choice>("NeuralRendering.Backend").value=="Streamline")
        return Error{ErrorCode::Unsupported,"Streamline NR backend is not implemented"};
    impl_->preset=settings.get<Choice>("NeuralRendering.Preset").value=="Shipping"?1u:0u;
    impl_->passes=static_cast<unsigned>(settings.get<std::int64_t>("NeuralRendering.PassCount"));
    const auto scale=settings.get<double>("NeuralRendering.InputResolutionScale");
    if(runtime.enabled&&scale!=0.0&&scale<1.0)
        return Error{ErrorCode::Unsupported,"Reduced NR input scale is not integrated yet"};
    if(runtime.enabled&&settings.get<bool>("NeuralRendering.InputColorIsHDR"))
        return Error{ErrorCode::Unsupported,"Current pre-SR NR input is SDR RGBA8"};
    if(runtime.enabled&&impl_->passes!=1)
        return Error{ErrorCode::Unsupported,"Multi-pass NR is not validated yet"};
    {
        std::scoped_lock lock(impl_->runtimeMutex);
        impl_->runtime=runtime;
        ++impl_->runtimeGeneration;
        impl_->runtimeResetPending=runtime.enabled;
        impl_->configured=true;
    }
    return true;
}
Result<bool> NrStage::updateRuntime(const Settings& settings) {
    if(!impl_->configured)
        return Error{ErrorCode::Conflict,"NR stage was not configured at startup"};
    const auto decoded=runtimeSettings(settings);
    if(const auto error=std::get_if<Error>(&decoded))return *error;
    const auto next=std::get<NrRuntimeSettings>(decoded);
    if(next.enabled&&settings.get<Choice>("NeuralRendering.Backend").value=="Streamline")
        return Error{ErrorCode::Unsupported,"Streamline NR backend is not implemented"};
    const auto scale=settings.get<double>("NeuralRendering.InputResolutionScale");
    if(next.enabled&&scale!=0.0&&scale<1.0)
        return Error{ErrorCode::Unsupported,"Reduced NR input scale is not integrated yet"};
    if(next.enabled&&settings.get<bool>("NeuralRendering.InputColorIsHDR"))
        return Error{ErrorCode::Unsupported,"Current pre-SR NR input is SDR RGBA8"};
    if(next.enabled&&settings.get<std::int64_t>("NeuralRendering.PassCount")!=1)
        return Error{ErrorCode::Unsupported,"Multi-pass NR is not validated yet"};
    if(next.enabled&&impl_->disabled)
        return Error{ErrorCode::Conflict,"NR was disabled after a runtime failure; restart is required"};
    std::scoped_lock lock(impl_->runtimeMutex);
    if(next==impl_->runtime)return false;
    impl_->runtime=next;
    ++impl_->runtimeGeneration;
    impl_->runtimeResetPending=next.enabled;
    return true;
}
Result<bool> NrStage::process(ID3D11Device* device,ID3D11DeviceContext* context,
    PreparedSrInputs& frame,bool reset) {
    try {
        NrRuntimeSettings runtime;
        std::uint64_t runtimeGeneration{};
        bool runtimeReset{};
        {
            std::scoped_lock lock(impl_->runtimeMutex);
            runtime=impl_->runtime;
            runtimeGeneration=impl_->runtimeGeneration;
            runtimeReset=impl_->runtimeResetPending;
        }
        if(!runtime.enabled||impl_->disabled)return false;
        reset=reset||runtimeReset;
        if(!device||!context||!frame.color()||!frame.motion()||!frame.depth())
            return Error{ErrorCode::InvalidInput,"NR stage input is incomplete"};
        if(!impl_->initialized) {
            const auto started=impl_->start(device,context);
            if(const auto error=std::get_if<Error>(&started)) {
                const auto copy=*error;stop();impl_->disabled=true;return copy;
            }
        }
        D3D11_TEXTURE2D_DESC colorDesc{},motionDesc{},depthDesc{};
        frame.color()->GetDesc(&colorDesc);frame.motion()->GetDesc(&motionDesc);
        frame.depth()->GetDesc(&depthDesc);
        if(colorDesc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||
           motionDesc.Format!=DXGI_FORMAT_R16G16_FLOAT||
           depthDesc.Format!=DXGI_FORMAT_R32_FLOAT||
           colorDesc.Width!=frame.width()||colorDesc.Height!=frame.height()||
           motionDesc.Width!=frame.width()||motionDesc.Height!=frame.height()||
           depthDesc.Width!=frame.width()||depthDesc.Height!=frame.height()) {
            impl_->disabled=true;
            return Error{ErrorCode::Unsupported,"NR pre-SR resource contract differs"};
        }
        if(!impl_->feature||impl_->width!=frame.width()||impl_->height!=frame.height()) {
            const auto rebuilt=impl_->rebuild(frame.width(),frame.height());
            if(const auto error=std::get_if<Error>(&rebuilt)) {
                impl_->disabled=true;return *error;
            }
        }
        context->CopyResource(impl_->color.d11.Get(),frame.color());
        context->CopyResource(impl_->motion.d11.Get(),frame.motion());
        context->CopyResource(impl_->depth.d11.Get(),frame.depth());
        const auto inputReady=++impl_->fenceValue;
        if(FAILED(impl_->context11->Signal(impl_->fence11.Get(),inputReady))) {
            impl_->disabled=true;
            return Error{ErrorCode::DeviceRemoved,"NR input signal failed"};
        }
        // The followed D3D11->D3D12 NR transport explicitly flushes after
        // Signal. Without it, the queue wait may observe guide/color work a
        // frame late under load, producing temporal trails.
        impl_->context11->Flush();
        if(FAILED(impl_->queue->Wait(impl_->fence12.Get(),inputReady))) {
            impl_->disabled=true;
            return Error{ErrorCode::DeviceRemoved,"NR input handoff failed"};
        }
        if(FAILED(impl_->allocator->Reset())||
           FAILED(impl_->commands->Reset(impl_->allocator.Get(),nullptr))) {
            impl_->disabled=true;
            return Error{ErrorCode::Unavailable,"NR command reset failed"};
        }
        impl_->barrier(impl_->color.d12.Get(),D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        impl_->barrier(impl_->motion.d12.Get(),D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        impl_->barrier(impl_->depth.d12.Get(),D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        impl_->barrier(impl_->output.d12.Get(),D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        auto* p=impl_->parameters;
        p->Set("DLSSNR.Color",impl_->color.d12.Get());p->Set("DLSSNR.Output",impl_->output.d12.Get());
        p->Set("DLSSNR.MVec",impl_->motion.d12.Get());p->Set("DLSSNR.Depth",impl_->depth.d12.Get());
        for(const char* key:{"Width","DLSSNR.Width","DLSSNR.InputWidth",
            "DLSSNR.ColorSubrectWidth","DLSSNR.MVecSubrectWidth","DLSSNR.DepthSubrectWidth",
            "OutWidth","DLSSNR.OutputWidth","DLSSNR.OutputSubrectWidth"})p->Set(key,frame.width());
        for(const char* key:{"Height","DLSSNR.Height","DLSSNR.InputHeight",
            "DLSSNR.ColorSubrectHeight","DLSSNR.MVecSubrectHeight","DLSSNR.DepthSubrectHeight",
            "OutHeight","DLSSNR.OutputHeight","DLSSNR.OutputSubrectHeight"})p->Set(key,frame.height());
        for(const char* key:{"DLSSNR.ColorSubrectBaseX","DLSSNR.ColorSubrectBaseY",
            "DLSSNR.MVecSubrectBaseX","DLSSNR.MVecSubrectBaseY","DLSSNR.DepthSubrectBaseX",
            "DLSSNR.DepthSubrectBaseY","DLSSNR.OutputSubrectBaseX","DLSSNR.OutputSubrectBaseY"})
            p->Set(key,0u);
        p->Set("DLSSNR.MVecScaleX",static_cast<float>(frame.width()));
        p->Set("DLSSNR.MVecScaleY",static_cast<float>(frame.height()));
        p->Set("DLSSNR.ScalingRatio",1.0f);p->Set("DLSSNR.Scale",1.0f);
        p->Set("DLSSNR.Upscaling",0);p->Set("DLSSNR.Enabled",1);
        p->Set("DLSSNR.Reset",reset?1:0);p->Set("DLSSNR.DepthInverted",0);
        p->Set("DLSSNR.Intensity",runtime.intensity);
        p->Set("DLSSNR.LocalToneStrength",runtime.tone);
        p->Set("DLSSNR.LocalStructureStrength",runtime.structure);
        p->Set("DLSSNR.SkinStructureStrength",runtime.skin);
        p->Set("DLSSNR.UseAutoMask",runtime.autoMask?1:0);
        p->Set("DLSSNR.Style",runtime.style);
        p->Set("DLSSNR.UICorrection",runtime.uiCorrection?1:0);
        p->Set("DLSS.Indicator.Invert.X.Axis",0);p->Set("DLSS.Indicator.Invert.Y.Axis",0);
        const auto evaluateResult=impl_->evaluate(impl_->commands.Get(),impl_->feature,p,nullptr);
        impl_->barrier(impl_->color.d12.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_COMMON);
        impl_->barrier(impl_->motion.d12.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_COMMON);
        impl_->barrier(impl_->depth.d12.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_COMMON);
        impl_->barrier(impl_->output.d12.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COMMON);
        const auto retired=impl_->submitAndWait();
        if(const auto error=std::get_if<Error>(&retired)) {
            impl_->disabled=true;return *error;
        }
        if(NVSDK_NGX_FAILED(static_cast<NVSDK_NGX_Result>(evaluateResult))) {
            impl_->disabled=true;
            return Error{ErrorCode::Unavailable,"Fast-FP16 NR evaluation failed"};
        }
        const auto outputReady=impl_->fenceValue;
        if(FAILED(impl_->context11->Wait(impl_->fence11.Get(),outputReady))) {
            impl_->disabled=true;
            return Error{ErrorCode::DeviceRemoved,"NR output handoff failed"};
        }
        context->CopyResource(frame.color(),impl_->output.d11.Get());
        {
            std::scoped_lock lock(impl_->runtimeMutex);
            if(impl_->runtimeGeneration==runtimeGeneration)
                impl_->runtimeResetPending=false;
        }
        ++impl_->submitted;return true;
    } catch(const std::exception& error) {
        impl_->disabled=true;
        if(!impl_->retirementUncertain) {
            const auto ignored=stop();(void)ignored;impl_->disabled=true;
        }
        return Error{ErrorCode::Unavailable,std::string{"NR stage exception: "}+error.what()};
    } catch(...) {
        impl_->disabled=true;
        if(!impl_->retirementUncertain) {
            const auto ignored=stop();(void)ignored;impl_->disabled=true;
        }
        return Error{ErrorCode::Unavailable,"Unknown NR stage exception"};
    }
}
Result<bool> NrStage::stop() {
    if(!impl_)return true;
    if(impl_->retirementUncertain&&impl_->queue&&impl_->fence12) {
        const auto value=++impl_->fenceValue;
        if(FAILED(impl_->queue->Signal(impl_->fence12.Get(),value)))
            return Error{ErrorCode::DeviceRemoved,"Cannot prove NR queue retirement; resources retained"};
        if(const auto drained=impl_->waitCpu(value);
           const auto error=std::get_if<Error>(&drained))return *error;
        impl_->retirementUncertain=false;
    }
    Error first{};bool failed=false;
    if(impl_->feature&&impl_->release) {
        const auto result=impl_->release(impl_->feature);
        if(NVSDK_NGX_FAILED(static_cast<NVSDK_NGX_Result>(result))) {
            first={ErrorCode::Unavailable,"NR feature release failed"};failed=true;
        }
    }
    impl_->feature=nullptr;
    if(impl_->parameters&&impl_->destroy) {
        const auto result=impl_->destroy(impl_->parameters);
        if(NVSDK_NGX_FAILED(static_cast<NVSDK_NGX_Result>(result))&&!failed) {
            first={ErrorCode::Unavailable,"NR parameter destruction failed"};failed=true;
        }
    }
    impl_->parameters=nullptr;
    if(impl_->runtimeInitialized&&impl_->shutdown) {
        const auto result=impl_->shutdown(impl_->device12.Get());
        if(NVSDK_NGX_FAILED(static_cast<NVSDK_NGX_Result>(result))&&!failed) {
            first={ErrorCode::Unavailable,"NR shutdown failed"};failed=true;
        }
    }
    impl_->initialized=false;impl_->runtimeInitialized=false;
    allocationDevice.store(nullptr,std::memory_order_release);
    if(const auto restored=impl_->shim.restore();
       std::holds_alternative<Error>(restored)&&!failed) {
        first=std::get<Error>(restored);failed=true;
    }
    callerIdentityModule.store(nullptr,std::memory_order_relaxed);
    if(impl_->nr){FreeLibrary(impl_->nr);impl_->nr=nullptr;}
    if(impl_->runtimeFile!=INVALID_HANDLE_VALUE) {
        CloseHandle(impl_->runtimeFile);impl_->runtimeFile=INVALID_HANDLE_VALUE;
    }
    return failed?Result<bool>{first}:Result<bool>{true};
}
bool NrStage::enabled() const noexcept {
    if(!impl_||impl_->disabled)return false;
    std::scoped_lock lock(impl_->runtimeMutex);
    return impl_->runtime.enabled;
}
std::uint64_t NrStage::submittedFrames() const noexcept{return impl_?impl_->submitted:0;}
}
