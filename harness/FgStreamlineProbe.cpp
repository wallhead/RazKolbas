#include <sl.h>
#include <sl_dlss_g.h>
#include <sl_reflex.h>
#include "rk/FgD3D11SwapFacade.hpp"
#include "rk/FactoryCreateTrace.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include "rk/PatchDescriptor.hpp"
#include "rk/PointerPatch.hpp"
#include "../tests/support/FgObservedSwap.hpp"
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cwchar>
#include <exception>
#include <filesystem>
#include <iostream>
#include <variant>

using Microsoft::WRL::ComPtr;

int probeSyntheticOn(ID3D12Device*, ID3D12CommandQueue*, IDXGISwapChain1*,
    IDXGIAdapter1*, bool, const wchar_t*);

namespace {
bool sameIdentity(IUnknown* a,IUnknown* b);
struct ReShadeInsertion {
    rk::FactoryCreateFn original{};
    IDXGIFactory* nativeFactory{};
    IUnknown* creationDevice{};
    IDXGISwapChain4* facade{};
    std::atomic<unsigned> substitutions{0};
};
std::atomic<ReShadeInsertion*> activeReShadeInsertion{nullptr};
HRESULT WINAPI insertFacadeAtNativeFactory(IDXGIFactory* factory,
    IUnknown* device,DXGI_SWAP_CHAIN_DESC* desc,
    IDXGISwapChain** result) noexcept {
    auto* state=activeReShadeInsertion.load(std::memory_order_acquire);
    if(!state||!state->original)return E_UNEXPECTED;
    if(factory!=state->nativeFactory||device!=state->creationDevice||
       !desc||!result||desc->BufferDesc.Width==0||
       desc->BufferDesc.Height==0)
        return state->original(factory,device,desc,result);
    *result=state->facade;
    state->facade->AddRef();
    state->substitutions.fetch_add(1,std::memory_order_relaxed);
    return S_OK;
}
int wrapFacadeWithReshade(const wchar_t* path,ID3D11Device* d11,
    IDXGISwapChain4* facade,ComPtr<IDXGISwapChain>& upper) {
    const auto& site=rk::reshade680FactoryCreateSite();
    const auto digest=rk::sha256File(path);
    if(!std::holds_alternative<std::string>(digest)||
       std::get<std::string>(digest)!=site.moduleSha256)return 61;
    const auto module=LoadLibraryW(path);
    if(!module)return 62;
    using CreateFactory=HRESULT(WINAPI*)(REFIID,void**);
    const auto create=reinterpret_cast<CreateFactory>(
        GetProcAddress(module,"CreateDXGIFactory1"));
    if(!create)return 63;
    ComPtr<IDXGIFactory1> wrapper;
    const auto made=create(IID_PPV_ARGS(&wrapper));
    std::cout<<"ReShade CreateDXGIFactory1=0x"<<std::hex<<
        static_cast<std::uint32_t>(made)<<std::dec<<'\n';
    if(FAILED(made))return 64;
    auto** wrapperTable=*reinterpret_cast<void***>(wrapper.Get());
    const auto wrapperBase=reinterpret_cast<std::uintptr_t>(module);
    if(reinterpret_cast<std::uintptr_t>(wrapperTable)-wrapperBase!=site.tableRva)
        return 65;
    const auto method=reinterpret_cast<rk::FactoryCreateFn>(
        wrapperBase+site.methodRva);
    if(!rk::isReshadeFactoryDelegateSite(wrapper.Get(),wrapperBase,
        site.moduleSha256,method,site))return 66;
    const auto facts=rk::inspectReshadeFactoryDelegate(wrapper.Get());
    const auto& nativeSite=rk::win11DxgiFactoryCreateSite();
    HMODULE nativeModule{};
    if(!facts.methodExecutable||
       !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
           reinterpret_cast<LPCWSTR>(facts.vtable),&nativeModule))return 67;
    const auto nativeBase=reinterpret_cast<std::uintptr_t>(nativeModule);
    wchar_t nativePath[32768]{},systemDirectory[MAX_PATH]{};
    const auto pathLength=GetModuleFileNameW(nativeModule,nativePath,32768);
    const auto systemLength=GetSystemDirectoryW(systemDirectory,MAX_PATH);
    if(!pathLength||pathLength>=32768||!systemLength||
       systemLength>=MAX_PATH) {
        FreeLibrary(nativeModule);
        return 68;
    }
    const auto expectedPath=std::filesystem::path(systemDirectory)/L"dxgi.dll";
    const auto nativeHash=rk::sha256File(std::filesystem::path(nativePath));
    std::wcout<<L"ReShade delegate module="<<nativePath<<L" expected="<<
        expectedPath.wstring()<<L'\n';
    std::cout<<"ReShade delegate tableRVA=0x"<<std::hex<<
        (facts.vtable-nativeBase)<<std::dec<<" hash="<<
        (std::holds_alternative<std::string>(nativeHash)?
            std::get<std::string>(nativeHash):"unreadable")<<'\n';
    HMODULE methodModule{};
    bool streamlineSlot=false;
    if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(facts.createMethod),&methodModule)) {
        const auto& foreign=rk::streamline2141FactoryCreateSite();
        wchar_t methodPath[32768]{};
        const auto methodPathLength=GetModuleFileNameW(methodModule,
            methodPath,32768);
        std::wcout<<L"ReShade delegate slot10 owner="<<methodPath<<L'\n';
        const auto methodHash=rk::sha256File(std::filesystem::path(methodPath));
        const auto methodBase=reinterpret_cast<std::uintptr_t>(methodModule);
        std::cout<<"ReShade delegate slot10 methodRVA=0x"<<std::hex<<
            (facts.createMethod-methodBase)<<std::dec<<" hash="<<
            (std::holds_alternative<std::string>(methodHash)?
                std::get<std::string>(methodHash):"unreadable")<<'\n';
        streamlineSlot=methodPathLength&&methodPathLength<32768&&
            std::filesystem::path(methodPath).filename()==
                L"sl.interposer.dll"&&
            std::holds_alternative<std::string>(methodHash)&&
            std::filesystem::file_size(methodPath)==foreign.fileSize&&
            std::holds_alternative<bool>(rk::validateForeignFactoryMethod(
                {reinterpret_cast<const std::uint8_t*>(methodBase),
                    foreign.imageSize},methodBase,
                std::get<std::string>(methodHash),foreign.fileSize,
                facts.createMethod,foreign));
        FreeLibrary(methodModule);
    }
    if(!std::holds_alternative<std::string>(nativeHash)||
       std::get<std::string>(nativeHash)!=nativeSite.moduleSha256||
       _wcsicmp(nativePath,expectedPath.c_str())!=0||
       facts.vtable-nativeBase!=nativeSite.tableRva||
       (!streamlineSlot&&facts.createMethod-nativeBase!=
           nativeSite.methodRva)) {
        FreeLibrary(nativeModule);
        return 68;
    }
    FreeLibrary(nativeModule);
    DXGI_SWAP_CHAIN_DESC desc{};
    if(FAILED(facade->GetDesc(&desc)))return 69;
    ReShadeInsertion insertion{reinterpret_cast<rk::FactoryCreateFn>(
        facts.createMethod),reinterpret_cast<IDXGIFactory*>(facts.delegate),
        d11,facade};
    rk::PointerPatch patch;
    auto** nativeTable=reinterpret_cast<void**>(facts.vtable);
    activeReShadeInsertion.store(&insertion,std::memory_order_release);
    const auto applied=patch.apply(nativeTable+nativeSite.slot,
        reinterpret_cast<void*>(facts.createMethod),
        reinterpret_cast<void*>(&insertFacadeAtNativeFactory));
    if(!std::holds_alternative<bool>(applied)) {
        activeReShadeInsertion.store(nullptr,std::memory_order_release);
        return 70;
    }
    const auto result=wrapper->CreateSwapChain(d11,&desc,&upper);
    const auto restored=patch.restore();
    activeReShadeInsertion.store(nullptr,std::memory_order_release);
    std::cout<<"ReShade wrapper CreateSwapChain=0x"<<std::hex<<
        static_cast<std::uint32_t>(result)<<std::dec<<
        " facadeSubstitutions="<<insertion.substitutions.load()<<
        " upper="<<static_cast<bool>(upper)<<
        " sameIdentity="<<sameIdentity(upper.Get(),facade)<<'\n';
    if(!std::holds_alternative<bool>(restored))return 71;
    if(FAILED(result)||!upper||insertion.substitutions.load()!=1)return 72;
    return 0;
}
int code(sl::Result result) { return static_cast<int>(result); }
bool sameIdentity(IUnknown* a,IUnknown* b) {
    ComPtr<IUnknown> left,right;
    return a&&b&&SUCCEEDED(a->QueryInterface(IID_PPV_ARGS(&left)))&&
        SUCCEEDED(b->QueryInterface(IID_PPV_ARGS(&right)))&&
        left.Get()==right.Get();
}
bool acquireForeground(HWND window) {
    ShowWindow(window,SW_SHOW);
    for(unsigned attempt=0;attempt<6;++attempt) {
        SetForegroundWindow(window);
        if(GetForegroundWindow()==window)return true;
        const auto previous=GetForegroundWindow();
        const auto foregroundThread=previous?
            GetWindowThreadProcessId(previous,nullptr):0;
        const auto ownThread=GetCurrentThreadId();
        if(foregroundThread&&foregroundThread!=ownThread&&
           AttachThreadInput(ownThread,foregroundThread,TRUE)) {
            BringWindowToTop(window);
            SetActiveWindow(window);
            SetForegroundWindow(window);
            AttachThreadInput(ownThread,foregroundThread,FALSE);
            if(GetForegroundWindow()==window)return true;
        }
        Sleep(100);
    }
    return GetForegroundWindow()==window;
}
bool readLowerPixel(ID3D12Device* device,ID3D12CommandQueue* queue,
    IDXGISwapChain1* swap,UINT index,std::array<std::uint8_t,4>& pixel) noexcept {
    ComPtr<ID3D12Resource> back;
    if(FAILED(swap->GetBuffer(index,IID_PPV_ARGS(&back))))return false;
    const auto desc=back->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 bytes{};
    device->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&bytes);
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type=D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width=bytes;buffer.Height=1;buffer.DepthOrArraySize=1;
    buffer.MipLevels=1;buffer.SampleDesc.Count=1;
    buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> readback;
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,
        &buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,
        IID_PPV_ARGS(&readback))))return false;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commands;
    if(FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&allocator)))||
       FAILED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,
        allocator.Get(),nullptr,IID_PPV_ARGS(&commands))))return false;
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition={back.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_SOURCE};
    commands->ResourceBarrier(1,&barrier);
    D3D12_TEXTURE_COPY_LOCATION from{};
    from.pResource=back.Get();
    from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION to{};
    to.pResource=readback.Get();
    to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    to.PlacedFootprint=footprint;
    commands->CopyTextureRegion(&to,0,0,0,&from,nullptr);
    barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_PRESENT;
    commands->ResourceBarrier(1,&barrier);
    if(FAILED(commands->Close()))return false;
    ComPtr<ID3D12Fence> done;
    if(FAILED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&done))))return false;
    const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event)return false;
    ID3D12CommandList* lists[]{commands.Get()};
    queue->ExecuteCommandLists(1,lists);
    if(FAILED(queue->Signal(done.Get(),1)))std::terminate();
    const auto armed=done->SetEventOnCompletion(1,event);
    const auto waited=SUCCEEDED(armed)?WaitForSingleObject(event,5000):WAIT_FAILED;
    CloseHandle(event);
    if(waited!=WAIT_OBJECT_0||done->GetCompletedValue()!=1||
       FAILED(device->GetDeviceRemovedReason()))std::terminate();
    void* data{};
    if(FAILED(readback->Map(0,nullptr,&data)))return false;
    const auto* p=static_cast<const std::uint8_t*>(data);
    pixel={p[0],p[1],p[2],p[3]};
    readback->Unmap(0,nullptr);
    return true;
}
template<class T> void attachUpgrade(Microsoft::WRL::ComPtr<T>& proxy,
    T* original,T* upgraded) {
    if(upgraded==original)proxy=original;
    else proxy.Attach(upgraded);
}
template<class T> sl::Result useProxy(T* original,
    Microsoft::WRL::ComPtr<T>& proxy,const char* label) {
    void* native{};
    const auto unwrapped=slGetNativeInterface(original,&native);
    ComPtr<IUnknown> nativeOwner;
    if(unwrapped==sl::Result::eOk&&native)
        nativeOwner.Attach(static_cast<IUnknown*>(native));
    std::cout<<"slGetNativeInterface("<<label<<")="<<code(unwrapped)<<
        " distinct="<<(native&&native!=original)<<'\n';
    if(unwrapped==sl::Result::eOk&&native&&native!=original) {
        proxy=original;
        return sl::Result::eOk;
    }
    auto* upgraded=original;
    const auto result=slUpgradeInterface(reinterpret_cast<void**>(&upgraded));
    std::cout<<"slUpgradeInterface("<<label<<")="<<code(result)<<'\n';
    if(result==sl::Result::eOk)attachUpgrade(proxy,original,upgraded);
    return result;
}
int probeFacade(IDXGIAdapter1* adapter,ID3D12Device* device,
    ID3D12CommandQueue* queue,IDXGISwapChain1* swap,
    const wchar_t* reshadePath=nullptr) {
    ComPtr<ID3D12Device> lowerDevice,queueDevice;
    const auto lowerHr=swap->GetDevice(IID_PPV_ARGS(&lowerDevice));
    const auto queueHr=queue->GetDevice(IID_PPV_ARGS(&queueDevice));
    std::cout<<"Facade D3D12 identities lower=0x"<<std::hex<<
        static_cast<std::uint32_t>(lowerHr)<<" queue=0x"<<
        static_cast<std::uint32_t>(queueHr)<<std::dec<<
        " lowerProxy="<<sameIdentity(lowerDevice.Get(),device)<<
        " queueProxy="<<sameIdentity(queueDevice.Get(),device)<<
        " lowerQueue="<<sameIdentity(lowerDevice.Get(),queueDevice.Get())<<'\n';
    void* native{};
    const auto nativeResult=slGetNativeInterface(device,&native);
    ComPtr<IUnknown> nativeOwner;
    if(nativeResult==sl::Result::eOk&&native)
        nativeOwner.Attach(static_cast<IUnknown*>(native));
    ComPtr<ID3D12Device> verifiedNative;
    if(nativeOwner)nativeOwner.As(&verifiedNative);
    std::cout<<"Facade verified SL native="<<code(nativeResult)<<
        " lowerNative="<<sameIdentity(lowerDevice.Get(),
            verifiedNative.Get())<<'\n';
    if(nativeResult!=sl::Result::eOk||
       !sameIdentity(lowerDevice.Get(),verifiedNative.Get()))return 27;
    ComPtr<ID3D11Device> d11;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level{};
    const auto created=D3D11CreateDevice(adapter,D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,
        &d11,&level,&context);
    std::cout<<"Facade D3D11CreateDevice=0x"<<std::hex<<
        static_cast<std::uint32_t>(created)<<std::dec<<'\n';
    if(FAILED(created))return 20;
    ComPtr<IDXGISwapChain4> lower4;
    if(FAILED(swap->QueryInterface(IID_PPV_ARGS(&lower4))))return 32;
    struct Observation {
        UINT index{};
        std::array<std::uint8_t,4> pixel{};
        bool read{};
    };
    std::array<Observation,3> observations{};
    std::size_t observedCount{};
    ComPtr<IDXGISwapChain4> observedSwap;
    observedSwap.Attach(new rk_test::FgObservedSwap(lower4.Get(),
        [&](UINT physical) noexcept {
            if(observedCount>=observations.size())return E_FAIL;
            auto& observation=observations[observedCount++];
            observation.index=physical;
            observation.read=readLowerPixel(device,queue,swap,physical,
                observation.pixel);
            return observation.read?S_OK:E_FAIL;
        }));
    auto made=rk::FgD3D11SwapFacade::create(d11.Get(),context.Get(),
        device,queue,observedSwap.Get(),verifiedNative.Get());
    if(const auto* error=std::get_if<rk::Error>(&made)) {
        std::cout<<"Facade create error="<<static_cast<int>(error->code)<<
            " "<<error->message<<'\n';
        return 21;
    }
    auto facade=std::move(std::get<ComPtr<IDXGISwapChain4>>(made));
    ComPtr<IDXGISwapChain> reshadeUpper;
    if(reshadePath) {
        const auto wrapped=wrapFacadeWithReshade(reshadePath,d11.Get(),
            facade.Get(),reshadeUpper);
        if(wrapped)return wrapped;
    }
    IDXGISwapChain* gameSwap=reshadeUpper?reshadeUpper.Get():facade.Get();
    ComPtr<ID3D11Device> owner;
    ComPtr<ID3D12Device> hidden;
    const auto gotD11=gameSwap->GetDevice(IID_PPV_ARGS(&owner));
    const auto gotD12=gameSwap->GetDevice(IID_PPV_ARGS(&hidden));
    std::cout<<"Facade GetDevice D3D11=0x"<<std::hex<<
        static_cast<std::uint32_t>(gotD11)<<" D3D12=0x"<<
        static_cast<std::uint32_t>(gotD12)<<std::dec<<
        " original="<<(owner.Get()==d11.Get())<<'\n';
    if(FAILED(gotD11)||gotD12!=E_NOINTERFACE||owner.Get()!=d11.Get())
        return 22;
    const auto index=0u; // Game-facing D3D11 logical back buffer.
    ComPtr<ID3D11Texture2D> buffer;
    const auto gotBuffer=gameSwap->GetBuffer(index,IID_PPV_ARGS(&buffer));
    std::cout<<"Facade GetBuffer=0x"<<std::hex<<
        static_cast<std::uint32_t>(gotBuffer)<<std::dec<<
        " index="<<index<<'\n';
    if(FAILED(gotBuffer))return 23;
    ComPtr<ID3D11RenderTargetView> view;
    const auto madeView=d11->CreateRenderTargetView(buffer.Get(),nullptr,&view);
    if(FAILED(madeView))return 24;
    const std::array<std::array<std::uint8_t,4>,3> colours{{
        {{255,0,0,255}},{{0,255,0,255}},{{0,0,255,255}}
    }};
    for(std::size_t frame=0;frame<colours.size();++frame) {
        const auto& expected=colours[frame];
        const float color[]{expected[0]/255.0f,expected[1]/255.0f,
            expected[2]/255.0f,1.0f};
        context->ClearRenderTargetView(view.Get(),color);
        const auto test=gameSwap->Present(0,DXGI_PRESENT_TEST);
        if(FAILED(test)||observedCount!=frame)return 25;
        const auto present=gameSwap->Present(0,0);
        if(FAILED(present)||observedCount!=frame+1)return 25;
        const auto& got=observations[frame];
        std::cout<<"Facade pre-Present frame="<<frame<<" index="<<
            got.index<<" read="<<got.read<<" rgba="<<
            static_cast<unsigned>(got.pixel[0])<<','<<
            static_cast<unsigned>(got.pixel[1])<<','<<
            static_cast<unsigned>(got.pixel[2])<<','<<
            static_cast<unsigned>(got.pixel[3])<<'\n';
        if(!got.read||got.index>=2||got.pixel!=expected)return 28;
    }
    const sl::ViewportHandle viewport{0u};
    sl::DLSSGState state{};
    const auto stateResult=slDLSSGGetState(viewport,state,nullptr);
    std::cout<<"Facade slDLSSGGetState="<<code(stateResult);
    if(stateResult==sl::Result::eOk)
        std::cout<<" actualPresented="<<
            state.numFramesActuallyPresented<<" maxExtra="<<
            state.numFramesToGenerateMax;
    std::cout<<'\n';
    if(stateResult!=sl::Result::eOk)return 29;
    view.Reset();
    buffer.Reset();
    context->ClearState();
    context->Flush();
    const auto resized=gameSwap->ResizeBuffers(2,128,80,
        DXGI_FORMAT_R8G8B8A8_UNORM,0);
    std::cout<<"Facade ResizeBuffers=0x"<<std::hex<<
        static_cast<std::uint32_t>(resized)<<std::dec<<'\n';
    if(FAILED(resized))return 26;
    HWND window{};
    if(FAILED(facade->GetHwnd(&window))||!window||
       !SetWindowPos(window,nullptr,0,0,144,88,
           SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE))return 30;
    const auto zeroResize=gameSwap->ResizeBuffers(2,0,0,
        DXGI_FORMAT_UNKNOWN,0);
    DXGI_SWAP_CHAIN_DESC after{};
    const auto descResult=facade->GetDesc(&after);
    std::cout<<"Facade zero-size ResizeBuffers=0x"<<std::hex<<
        static_cast<std::uint32_t>(zeroResize)<<std::dec<<
        " extent="<<after.BufferDesc.Width<<'x'<<
        after.BufferDesc.Height<<'\n';
    return SUCCEEDED(zeroResize)&&SUCCEEDED(descResult)&&
        after.BufferDesc.Width==144&&after.BufferDesc.Height==88?0:31;
}
int probeSwap(IDXGIFactory6* factory,IDXGIAdapter1* adapter,
    ID3D12Device* device,bool facadeMode,bool onMode,
    const wchar_t* reshadePath=nullptr) {
    ComPtr<ID3D12Device> proxyDevice;
    const auto deviceUpgrade=useProxy(device,proxyDevice,"device");
    if(deviceUpgrade!=sl::Result::eOk)return 9;
    ComPtr<IDXGIFactory6> proxyFactory;
    const auto factoryUpgrade=useProxy(factory,proxyFactory,"factory");
    if(factoryUpgrade!=sl::Result::eOk)return 10;
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    ComPtr<ID3D12CommandQueue> queue;
    const auto madeQueue=proxyDevice->CreateCommandQueue(&queueDesc,
        IID_PPV_ARGS(&queue));
    std::cout<<"CreateCommandQueue=0x"<<std::hex<<
        static_cast<std::uint32_t>(madeQueue)<<std::dec<<'\n';
    if(FAILED(madeQueue))return 11;
    const auto module=GetModuleHandleW(nullptr);
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc=DefWindowProcW;
    windowClass.hInstance=module;
    windowClass.lpszClassName=L"RazKolbasFgProbeWindow";
    if(onMode&&!RegisterClassW(&windowClass))return 12;
    const auto window=CreateWindowExW(0,onMode?windowClass.lpszClassName:L"STATIC",
        L"RazKolbas FG SL probe",onMode?WS_OVERLAPPEDWINDOW:WS_POPUP,0,0,
        onMode?1280:96,onMode?720:72,nullptr,nullptr,module,nullptr);
    if(!window)return 12;
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width=onMode?1280:96;desc.Height=onMode?720:72;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount=2;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> swap;
    const auto madeSwap=proxyFactory->CreateSwapChainForHwnd(queue.Get(),
        window,&desc,nullptr,nullptr,&swap);
    std::cout<<"CreateSwapChainForHwnd=0x"<<std::hex<<
        static_cast<std::uint32_t>(madeSwap)<<std::dec<<'\n';
    int result=0;
    if(FAILED(madeSwap))result=13;
    else {
        void* nativeSwap{};
        const auto native=slGetNativeInterface(swap.Get(),&nativeSwap);
        ComPtr<IUnknown> nativeSwapOwner;
        if(native==sl::Result::eOk&&nativeSwap)
            nativeSwapOwner.Attach(static_cast<IUnknown*>(nativeSwap));
        std::cout<<"slGetNativeInterface(swap)="<<code(native)<<
            " distinct="<<(nativeSwap&&nativeSwap!=swap.Get())<<'\n';
        if(native!=sl::Result::eOk)result=14;
        const sl::ViewportHandle viewport{0u};
        sl::DLSSGOptions options{};
        options.mode=sl::DLSSGMode::eOff;
        if(!onMode) {
            const auto mode=slDLSSGSetOptions(viewport,options);
            std::cout<<"slDLSSGSetOptions(Off)="<<code(mode)<<'\n';
            if(mode!=sl::Result::eOk&&!result)result=15;
        }
        ComPtr<IDXGISwapChain3> swap3;
        const auto qi=swap.As(&swap3);
        std::cout<<"QueryInterface(IDXGISwapChain3)=0x"<<std::hex<<
            static_cast<std::uint32_t>(qi)<<std::dec<<'\n';
        if(FAILED(qi)&&!result)result=16;
        if(swap3) {
            std::cout<<"GetCurrentBackBufferIndex="<<
                swap3->GetCurrentBackBufferIndex()<<'\n';
        }
        if(onMode) {
            const bool foreground=acquireForeground(window);
            std::cout<<"FG-On foreground="<<foreground<<'\n';
            if(!foreground&&!reshadePath&&!result)result=59;
            if(!foreground&&reshadePath)
                std::cout<<"ReShade FG-On probe continues unfocused; generated presentation is not assumed\n";
            if(!result)result=probeSyntheticOn(proxyDevice.Get(),queue.Get(),
                swap.Get(),adapter,facadeMode,reshadePath);
        } else if(facadeMode) {
            if(!result)result=probeFacade(adapter,proxyDevice.Get(),
                queue.Get(),swap.Get(),reshadePath);
        } else {
        const auto test=swap->Present(0,DXGI_PRESENT_TEST);
        const auto presented=swap->Present(0,0);
        std::cout<<"Present(TEST)=0x"<<std::hex<<
            static_cast<std::uint32_t>(test)<<" Present=0x"<<
            static_cast<std::uint32_t>(presented)<<std::dec<<'\n';
        if(FAILED(test)||FAILED(presented))if(!result)result=17;
        sl::DLSSGState state{};
        const auto stateResult=slDLSSGGetState(viewport,state,nullptr);
        std::cout<<"slDLSSGGetState="<<code(stateResult);
        if(stateResult==sl::Result::eOk)
            std::cout<<" status=0x"<<std::hex<<
                static_cast<std::uint32_t>(state.status)<<std::dec<<
                " actualPresented="<<state.numFramesActuallyPresented<<
                " maxExtra="<<state.numFramesToGenerateMax;
        std::cout<<'\n';
        if(stateResult!=sl::Result::eOk&&!result)result=18;
        const auto resized=swap->ResizeBuffers(2,128,80,
            DXGI_FORMAT_R8G8B8A8_UNORM,0);
        std::cout<<"ResizeBuffers=0x"<<std::hex<<
            static_cast<std::uint32_t>(resized)<<std::dec<<'\n';
        if(FAILED(resized)&&!result)result=19;
        }
    }
    swap.Reset();
    queue.Reset();
    proxyFactory.Reset();
    proxyDevice.Reset();
    DestroyWindow(window);
    if(onMode)UnregisterClassW(windowClass.lpszClassName,module);
    return result;
}
}

int wrapFacadeWithReshadeForProbe(const wchar_t* path,ID3D11Device* device,
    IDXGISwapChain4* facade,ComPtr<IDXGISwapChain>& upper) {
    return wrapFacadeWithReshade(path,device,facade,upper);
}

int wmain(int argc,wchar_t** argv) {
    if((argc!=2&&argc!=3&&argc!=4)||(argc==4&&
       std::wcscmp(argv[2],L"--reshade-facade")&&
       std::wcscmp(argv[2],L"--reshade-facade-on"))||(argc==3&&
       std::wcscmp(argv[2],L"--swap")&&
       std::wcscmp(argv[2],L"--facade")&&
       std::wcscmp(argv[2],L"--on")&&
       std::wcscmp(argv[2],L"--facade-on"))) {
        std::wcerr<<L"Usage: RazKolbasFgStreamlineProbe <absolute SDK bin/x64> [--swap|--facade|--on|--facade-on|--reshade-facade <absolute ReShade dxgi.dll>|--reshade-facade-on <absolute ReShade dxgi.dll>]\n";
        return 1;
    }
    const wchar_t* pluginPaths[]{argv[1]};
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
    preferences.engineVersion="0.1.115";
    preferences.projectId="b3340e44-a57e-4b98-9318-d7150829d110";
    preferences.renderAPI=sl::RenderAPI::eD3D12;
    std::wstring logPath;
    if(argc>=3&&(!std::wcscmp(argv[2],L"--on")||
       !std::wcscmp(argv[2],L"--reshade-facade-on")||
       !std::wcscmp(argv[2],L"--facade-on"))) {
        const auto path=std::filesystem::current_path()/
            "artifacts"/"local"/"fg-on-probe";
        std::filesystem::create_directories(path);
        logPath=path.wstring();
        preferences.logLevel=sl::LogLevel::eVerbose;
        preferences.pathToLogsAndData=logPath.c_str();
    }
    const auto initialized=slInit(preferences);
    std::cout<<"slInit="<<code(initialized)<<'\n';
    if(initialized!=sl::Result::eOk)return 2;

    int exitCode=0;
    // SL shutdown precedes destruction of every DXGI/D3D12 interface.
    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    {
        if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))exitCode=3;
        DXGI_ADAPTER_DESC1 desc{};
        if(!exitCode) {
            for(UINT index=0;factory->EnumAdapterByGpuPreference(index,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(&adapter))!=DXGI_ERROR_NOT_FOUND;++index) {
                if(SUCCEEDED(adapter->GetDesc1(&desc))&&desc.VendorId==0x10de)break;
                adapter.Reset();
            }
            if(!adapter)exitCode=4;
        }
        if(!exitCode) {
            std::cout<<"adapterVendor=0x"<<std::hex<<desc.VendorId
                <<" device=0x"<<desc.DeviceId<<std::dec<<'\n';
            sl::AdapterInfo info{};
            info.deviceLUID=reinterpret_cast<std::uint8_t*>(&desc.AdapterLuid);
            info.deviceLUIDSizeInBytes=sizeof(desc.AdapterLuid);
            const auto support=slIsFeatureSupported(sl::kFeatureDLSS_G,info);
            std::cout<<"slIsFeatureSupported(DLSS-G)="<<code(support)<<'\n';
            sl::FeatureRequirements requirements{};
            const auto queried=slGetFeatureRequirements(sl::kFeatureDLSS_G,
                requirements);
            std::cout<<"slGetFeatureRequirements(DLSS-G)="<<code(queried);
            if(queried==sl::Result::eOk)
                std::cout<<" flags=0x"<<std::hex<<
                    static_cast<std::uint32_t>(requirements.flags)<<std::dec;
            std::cout<<'\n';
            const auto created=D3D12CreateDevice(adapter.Get(),
                D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device));
            std::cout<<"D3D12CreateDevice=0x"<<std::hex<<
                static_cast<std::uint32_t>(created)<<std::dec<<'\n';
            if(FAILED(created))exitCode=5;
            else {
                const auto bound=slSetD3DDevice(device.Get());
                std::cout<<"slSetD3DDevice="<<code(bound)<<'\n';
                if(bound!=sl::Result::eOk)exitCode=6;
                else if(argc>=3)exitCode=probeSwap(factory.Get(),adapter.Get(),
                    device.Get(),!std::wcscmp(argv[2],L"--facade")||
                    !std::wcscmp(argv[2],L"--facade-on")||
                    !std::wcscmp(argv[2],L"--reshade-facade")||
                    !std::wcscmp(argv[2],L"--reshade-facade-on"),
                    !std::wcscmp(argv[2],L"--on")||
                    !std::wcscmp(argv[2],L"--facade-on")||
                    !std::wcscmp(argv[2],L"--reshade-facade-on"),
                    argc==4?argv[3]:nullptr);
            }
            if(support!=sl::Result::eOk||queried!=sl::Result::eOk)
                if(!exitCode)exitCode=7;
        }
    }
    const auto stopped=slShutdown();
    std::cout<<"slShutdown="<<code(stopped)<<'\n';
    if(stopped!=sl::Result::eOk&&exitCode==0)exitCode=8;
    return exitCode;
}
