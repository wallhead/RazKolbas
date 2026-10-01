#include "rk/FgPrivateSwapRoute.hpp"
#include "rk/FgGameInputProbe.hpp"
#include "rk/FgReShadeEffectOwner.hpp"
#include "rk/FgD3D11SwapFacade.hpp"
#include "rk/FactoryCreateTrace.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include "rk/PatchDescriptor.hpp"
#include "rk/PointerPatch.hpp"
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <iostream>
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <optional>
#include <variant>
#include <vector>
#include <chrono>
#include <thread>

using Microsoft::WRL::ComPtr;
namespace {
struct Module {HMODULE value{};~Module(){if(value)FreeLibrary(value);}};
struct Window {HWND value{};~Window(){if(value)DestroyWindow(value);}};
struct Callback {
    rk::FgPrivateSwapRoute* route{};
    rk::FactoryCreateFn next{};
    IDXGIFactory* factory{};
    unsigned substitutions{};
    ComPtr<IDXGISwapChain4> facade;
};
Callback* active{};
rk::FactoryCreateFn wrapperNext{};
unsigned wrapperCalls{};
// ReShade 6.8 public add-on event IDs and callback ABI. This probe is
// hash-gated to the installed 6.8 DLL; it never changes effect state.
struct EffectView {std::uint64_t handle;};
struct EffectTrace {
    struct Runtime {
        void* identity{};
        std::uint64_t native{};
        std::uint64_t commandQueueNative{};
        void* window{};
        std::uint32_t api{};
        unsigned epoch{};
        unsigned firstFrame{};
        unsigned lastFrame{};
        unsigned presents{};
        unsigned begins{};
        unsigned techniques{};
        unsigned finishes{};
        std::array<std::uint8_t,240> techniqueFrames{};
        std::vector<std::uint8_t> screenshot;
        std::uint32_t screenshotWidth{};
        std::uint32_t screenshotHeight{};
        bool screenshotAttempted{};
        bool screenshotSucceeded{};
    };
    using RegisterAddon=bool(*)(HMODULE,std::uint32_t);
    using RegisterEvent=void(*)(std::uint32_t,void*);
    using UnregisterAddon=void(*)(HMODULE);
    RegisterEvent registerEvent{};
    RegisterEvent unregisterEvent{};
    UnregisterAddon unregisterAddon{};
    HMODULE addon{};
    std::mutex mutex;
    std::array<Runtime,8> runtimes{};
    unsigned count{};
    unsigned overflow{};
    std::atomic<unsigned> frame{};
    std::atomic<unsigned> epoch{};
    bool registered{};
    static std::atomic<EffectTrace*> active;
    static constexpr std::uint32_t presentEvent=75;
    static constexpr std::uint32_t beginEvent=76;
    static constexpr std::uint32_t finishEvent=77;
    static constexpr std::uint32_t techniqueEvent=83;
    struct Identity {std::uint64_t native{};std::uint64_t queue{};
        void* window{};std::uint32_t api{};};
    static Identity runtimeIdentity(void* runtime) noexcept {
        auto** table=*reinterpret_cast<void***>(runtime);
        auto* device=reinterpret_cast<void*(*)(void*)>(table[3])(runtime);
        if(!device)return {};
        auto** deviceTable=*reinterpret_cast<void***>(device);
        auto* queue=reinterpret_cast<void*(*)(void*)>(table[8])(runtime);
        return {
            reinterpret_cast<std::uint64_t(*)(void*)>(table[0])(runtime),
            queue?reinterpret_cast<std::uint64_t(*)(void*)>(
                (*reinterpret_cast<void***>(queue))[0])(queue):0,
            reinterpret_cast<void*(*)(void*)>(table[4])(runtime),
            reinterpret_cast<std::uint32_t(*)(void*)>(deviceTable[3])(device)};
    }

    void record(void* runtime,std::uint32_t event) noexcept {
        if(!runtime)return;
        std::unique_lock lock(mutex);
        const auto currentEpoch=epoch.load(std::memory_order_relaxed);
        const auto currentFrame=frame.load(std::memory_order_relaxed);
        Runtime* found{};
        for(unsigned index=0;index<count;++index)
            if(runtimes[index].identity==runtime&&
               runtimes[index].epoch==currentEpoch) {found=&runtimes[index];break;}
        if(!found) {
            if(count==runtimes.size()) {++overflow;return;}
            found=&runtimes[count++];
            found->identity=runtime;
            const auto identity=runtimeIdentity(runtime);
            found->native=identity.native;
            found->commandQueueNative=identity.queue;
            found->window=identity.window;
            found->api=identity.api;
            found->epoch=currentEpoch;
            found->firstFrame=currentFrame;
        }
        found->lastFrame=currentFrame;
        if(event==presentEvent)++found->presents;
        else if(event==beginEvent)++found->begins;
        else if(event==finishEvent)++found->finishes;
        else if(event==techniqueEvent) {
            ++found->techniques;
            if(currentFrame<found->techniqueFrames.size())
                found->techniqueFrames[currentFrame]=1;
        }
        if(event==presentEvent&&found->api==0xc000&&
           (currentFrame==100||currentFrame==220)&&
           !found->screenshotAttempted) {
            found->screenshotAttempted=true;
            lock.unlock();
            std::uint32_t width{},height{};
            auto** table=*reinterpret_cast<void***>(runtime);
            reinterpret_cast<void(*)(void*,std::uint32_t*,std::uint32_t*)>(
                table[11])(runtime,&width,&height);
            bool succeeded=false;
            std::vector<std::uint8_t> pixels;
            try {
                if(width&&height&&width<=8192&&height<=8192) {
                    pixels.resize(static_cast<std::size_t>(width)*height*4);
                    succeeded=reinterpret_cast<bool(*)(void*,void*)>(
                        table[10])(runtime,pixels.data());
                }
            } catch(...) {succeeded=false;}
            lock.lock();
            found->screenshotSucceeded=succeeded;
            found->screenshotWidth=width;
            found->screenshotHeight=height;
            if(succeeded)found->screenshot=std::move(pixels);
        }
    }
    static void presented(void* runtime) noexcept {
        if(auto* trace=active.load(std::memory_order_acquire))
            trace->record(runtime,presentEvent);
    }
    static void began(void* runtime,void*,EffectView,EffectView) noexcept {
        if(auto* trace=active.load(std::memory_order_acquire))
            trace->record(runtime,beginEvent);
    }
    static void finished(void* runtime,void*,EffectView,EffectView) noexcept {
        if(auto* trace=active.load(std::memory_order_acquire))
            trace->record(runtime,finishEvent);
    }
    static void technique(void* runtime,EffectView,void*,EffectView,EffectView) noexcept {
        if(auto* trace=active.load(std::memory_order_acquire))
            trace->record(runtime,techniqueEvent);
    }
    bool start(HMODULE module) noexcept {
        if(active.load(std::memory_order_acquire)||!module)return false;
        auto* add=reinterpret_cast<RegisterAddon>(GetProcAddress(module,"ReShadeRegisterAddon"));
        registerEvent=reinterpret_cast<RegisterEvent>(GetProcAddress(module,"ReShadeRegisterEvent"));
        unregisterEvent=reinterpret_cast<RegisterEvent>(GetProcAddress(module,"ReShadeUnregisterEvent"));
        unregisterAddon=reinterpret_cast<UnregisterAddon>(GetProcAddress(module,"ReShadeUnregisterAddon"));
        addon=GetModuleHandleW(nullptr);
        if(!add||!registerEvent||!unregisterEvent||!unregisterAddon||
           !addon||!add(addon,14))return false;
        active.store(this,std::memory_order_release);
        registerEvent(presentEvent,reinterpret_cast<void*>(&presented));
        registerEvent(beginEvent,reinterpret_cast<void*>(&began));
        registerEvent(finishEvent,reinterpret_cast<void*>(&finished));
        registerEvent(techniqueEvent,reinterpret_cast<void*>(&technique));
        registered=true;
        return true;
    }
    void nextFrame(unsigned number) noexcept {frame.store(number,std::memory_order_relaxed);}
    void nextEpoch() noexcept {epoch.fetch_add(1,std::memory_order_relaxed);}
    void report() {
        std::lock_guard lock(mutex);
        std::cout<<"Effect trace registered="<<registered<<
            " runtimes="<<count<<" overflow="<<overflow<<'\n';
        for(unsigned index=0;index<count;++index) {
            const auto& item=runtimes[index];
            std::cout<<"Effect trace runtime="<<item.identity<<
                " native=0x"<<std::hex<<item.native<<
                " queueNative=0x"<<item.commandQueueNative<<
                " window="<<item.window<<
                " api=0x"<<item.api<<std::dec<<
                " epoch="<<item.epoch<<
                " firstFrame="<<item.firstFrame<<
                " lastFrame="<<item.lastFrame<<
                " presents="<<item.presents<<
                " begins="<<item.begins<<
                " techniques="<<item.techniques<<
                " finishes="<<item.finishes<<'\n';
            if(item.api==0xc000&&item.screenshotAttempted) {
                std::uint64_t hash=1469598103934665603ull;
                for(const auto byte:item.screenshot)
                    hash=(hash^byte)*1099511628211ull;
                std::cout<<"Effect screenshot epoch="<<item.epoch<<
                    " frame="<<(item.epoch?220:100)<<
                    " succeeded="<<item.screenshotSucceeded<<
                    " size="<<item.screenshotWidth<<'x'<<item.screenshotHeight<<
                    " hash="<<std::hex<<hash<<std::dec;
                if(item.screenshotSucceeded&&item.screenshotWidth>=4&&
                   item.screenshotHeight>=4) {
                    for(const auto [x,y]:std::array<std::pair<unsigned,unsigned>,4>{{
                        {item.screenshotWidth/4,item.screenshotHeight/4},
                        {item.screenshotWidth*3/4,item.screenshotHeight/4},
                        {item.screenshotWidth/4,item.screenshotHeight*3/4},
                        {item.screenshotWidth*3/4,item.screenshotHeight*3/4}}}) {
                        const auto offset=(static_cast<std::size_t>(y)*
                            item.screenshotWidth+x)*4;
                        std::cout<<" sample=";
                        for(unsigned channel=0;channel<4;++channel)
                            std::cout<<(channel?",":"")<<
                                static_cast<unsigned>(item.screenshot[offset+channel]);
                    }
                }
                std::cout<<'\n';
            }
        }
        for(unsigned observedEpoch=0;observedEpoch<=epoch.load(std::memory_order_relaxed);
            ++observedEpoch) {
            unsigned simultaneousFrames{};
            for(unsigned frameNumber=0;frameNumber<240;++frameNumber) {
                unsigned executingRuntimes{};
                for(unsigned index=0;index<count;++index)
                    if(runtimes[index].epoch==observedEpoch&&
                       runtimes[index].techniqueFrames[frameNumber])
                        ++executingRuntimes;
                if(executingRuntimes>=2)++simultaneousFrames;
            }
            std::cout<<"Effect trace coexecuted epoch="<<observedEpoch<<
                " frames="<<simultaneousFrames<<'\n';
        }
    }
    void stop() noexcept {
        if(!registered)return;
        unregisterEvent(techniqueEvent,reinterpret_cast<void*>(&technique));
        unregisterEvent(finishEvent,reinterpret_cast<void*>(&finished));
        unregisterEvent(beginEvent,reinterpret_cast<void*>(&began));
        unregisterEvent(presentEvent,reinterpret_cast<void*>(&presented));
        active.store(nullptr,std::memory_order_release);
        unregisterAddon(addon);
        registered=false;
    }
    ~EffectTrace() {stop();}
};
std::atomic<EffectTrace*> EffectTrace::active{};
bool sameIdentity(IUnknown* first,IUnknown* second) noexcept {
    ComPtr<IUnknown> a,b;
    return first&&second&&SUCCEEDED(first->QueryInterface(IID_PPV_ARGS(&a)))&&
        SUCCEEDED(second->QueryInterface(IID_PPV_ARGS(&b)))&&a.Get()==b.Get();
}
bool exerciseOwnerCopies(ID3D11Device* device,ID3D11DeviceContext* context,
    const rk::FgPresentationInputOwner& owner) {
    const rk::Extent size{owner.description.BufferDesc.Width,
        owner.description.BufferDesc.Height};
    rk::FgWorldGuideFrame world{};
    world.frame.generation=1;world.frame.resetEpoch=1;
    world.frame.render=size;world.frame.display=size;
    world.frame.worldActive=true;
    rk::FgUiPlaneFrame ui{};
    ui.generation=1;ui.resetEpoch=1;ui.display=size;
    ui.uiRegion={0,0,static_cast<LONG>(size.width),static_cast<LONG>(size.height)};
    const auto make=[&](DXGI_FORMAT format,ComPtr<ID3D11Texture2D>& texture) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=size.width;desc.Height=size.height;
        desc.MipLevels=1;desc.ArraySize=1;desc.SampleDesc.Count=1;
        desc.Format=format;desc.Usage=D3D11_USAGE_DEFAULT;
        desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        return SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&texture));
    };
    if(!make(DXGI_FORMAT_R32_FLOAT,world.depth)||
       !make(DXGI_FORMAT_R16G16_FLOAT,world.motion)||
       !make(DXGI_FORMAT_R8G8B8A8_UNORM,world.hudless)||
       !make(DXGI_FORMAT_R8G8B8A8_UNORM,ui.hudless)||
       !make(DXGI_FORMAT_R8G8B8A8_UNORM,ui.uiColorAlpha)||
       !make(DXGI_FORMAT_R8G8B8A8_UNORM,ui.finalColor))return false;
    std::unique_ptr<rk::FgGameInputProbe> probe;
    for(unsigned n=1;n<=8;++n) {
        world.frame.source=n;world.frame.presentToken=n;
        ui.source=n;ui.presentToken=n;
        const rk::FgResourceStamp stamp{n,1,size,true,1};
        ui.hudlessStamp=stamp;ui.uiStamp=stamp;ui.finalStamp=stamp;
        auto pair=rk::pairFgGameInputs(world,ui);
        if(const auto* error=std::get_if<rk::Error>(&pair)) {
            std::cout<<"Game route owner pair failed: "<<error->message<<'\n';
            return false;
        }
        const auto& candidate=std::get<rk::FgGameInputCandidate>(pair);
        if(!probe) {
            auto result=rk::FgGameInputProbe::beginOnOwner(device,context,
                owner.nativeDevice.Get(),owner.nativeQueue.Get(),owner.runtime,
                candidate);
            if(const auto* error=std::get_if<rk::Error>(&result)) {
                std::cout<<"Game route owner copy failed: "<<error->message<<'\n';
                return false;
            }
            probe=std::move(std::get<std::unique_ptr<rk::FgGameInputProbe>>(result));
        } else if(std::holds_alternative<rk::Error>(probe->enqueue(context,candidate)))
            return false;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        auto state=probe->poll();
        while(state==rk::FgGameCopyState::Pending&&
              std::chrono::steady_clock::now()<deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            state=probe->poll();
        }
        if(state!=rk::FgGameCopyState::Complete)return false;
        const auto ticket=probe->copyTicket();
        std::cout<<"Game route exact-owner copy="<<n<<" producer="<<
            ticket.producer<<" copy="<<ticket.copy<<" complete=1\n";
    }
    const bool clean=probe->close();
    std::cout<<"Game route exact-owner copies=8 clean="<<clean<<'\n';
    return clean;
}
std::optional<std::array<unsigned char,4>> firstPixel(
    ID3D11Device* device,ID3D11DeviceContext* context,
    ID3D11Texture2D* source) {
    if(!device||!context||!source)return std::nullopt;
    D3D11_TEXTURE2D_DESC desc{};
    source->GetDesc(&desc);
    if(!desc.Width||!desc.Height||desc.SampleDesc.Count!=1||
       desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM)return std::nullopt;
    desc.Usage=D3D11_USAGE_STAGING;
    desc.BindFlags=0;
    desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    desc.MiscFlags=0;
    ComPtr<ID3D11Texture2D> staging;
    if(FAILED(device->CreateTexture2D(&desc,nullptr,&staging)))return std::nullopt;
    context->CopyResource(staging.Get(),source);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))
        return std::nullopt;
    const auto* data=static_cast<const unsigned char*>(mapped.pData);
    std::array<unsigned char,4> pixel{data[0],data[1],data[2],data[3]};
    context->Unmap(staging.Get(),0);
    return pixel;
}
void reportPixel(const char* stage,
    const std::optional<std::array<unsigned char,4>>& pixel) {
    std::cout<<stage<<" pixel=";
    if(!pixel)std::cout<<"unavailable";
    else for(const auto channel:*pixel)std::cout<<static_cast<unsigned>(channel)<<',';
    std::cout<<'\n';
}
using ColourSamples=std::array<std::array<std::uint8_t,4>,4>;
struct ColourReadback {
    ColourSamples samples{};
    std::vector<std::uint8_t> pixels;
};
constexpr ColourSamples sourcePattern{{
    {{100,120,140,255}},{{150,130,100,255}},
    {{80,100,70,255}},{{180,170,160,255}}}};
constexpr std::array<const char*,4> sampleNames{"TL","TR","BL","BR"};
ColourSamples samplePixels(const std::uint8_t* data,UINT rowPitch,
    UINT width,UINT height) noexcept {
    const std::array<std::pair<UINT,UINT>,4> positions{{
        {width/4,height/4},{width*3/4,height/4},
        {width/4,height*3/4},{width*3/4,height*3/4}}};
    ColourSamples result{};
    for(unsigned index=0;index<4;++index) {
        const auto [x,y]=positions[index];
        const auto* pixel=data+y*rowPitch+x*4;
        std::copy_n(pixel,4,result[index].begin());
    }
    return result;
}
ColourReadback readColour(const std::uint8_t* data,UINT rowPitch,
    UINT width,UINT height) {
    ColourReadback result;
    result.samples=samplePixels(data,rowPitch,width,height);
    result.pixels.resize(static_cast<std::size_t>(width)*height*4);
    for(UINT y=0;y<height;++y)
        std::copy_n(data+static_cast<std::size_t>(y)*rowPitch,width*4,
            result.pixels.begin()+static_cast<std::size_t>(y)*width*4);
    return result;
}
std::uint64_t colourHash(const std::vector<std::uint8_t>& pixels) noexcept {
    std::uint64_t value=14695981039346656037ull;
    for(const auto byte:pixels) {
        value^=byte;
        value*=1099511628211ull;
    }
    return value;
}
void uploadPattern(ID3D11DeviceContext* context,ID3D11Texture2D* target) {
    D3D11_TEXTURE2D_DESC desc{};
    target->GetDesc(&desc);
    std::vector<std::uint8_t> data(
        static_cast<std::size_t>(desc.Width)*desc.Height*4);
    for(UINT y=0;y<desc.Height;++y)
        for(UINT x=0;x<desc.Width;++x) {
            const auto quadrant=(y>=desc.Height/2?2u:0u)+
                (x>=desc.Width/2?1u:0u);
            std::copy(sourcePattern[quadrant].begin(),
                sourcePattern[quadrant].end(),
                data.begin()+(static_cast<std::size_t>(y)*desc.Width+x)*4);
        }
    context->UpdateSubresource(target,0,nullptr,data.data(),desc.Width*4,0);
}
std::optional<ColourReadback> sampleD3D11(ID3D11Device* device,
    ID3D11DeviceContext* context,ID3D11Texture2D* source) {
    if(!device||!context||!source)return std::nullopt;
    D3D11_TEXTURE2D_DESC desc{};
    source->GetDesc(&desc);
    if(!desc.Width||!desc.Height||desc.SampleDesc.Count!=1||
       desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM)return std::nullopt;
    desc.Usage=D3D11_USAGE_STAGING;
    desc.BindFlags=0;
    desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    desc.MiscFlags=0;
    ComPtr<ID3D11Texture2D> staging;
    if(FAILED(device->CreateTexture2D(&desc,nullptr,&staging)))return std::nullopt;
    context->CopyResource(staging.Get(),source);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))
        return std::nullopt;
    const auto result=readColour(
        static_cast<const std::uint8_t*>(mapped.pData),mapped.RowPitch,
        desc.Width,desc.Height);
    context->Unmap(staging.Get(),0);
    return result;
}
std::optional<ColourReadback> sampleD3D12(IDXGISwapChain4* swap,
    ID3D12CommandQueue* queue,UINT index) {
    if(!swap||!queue)return std::nullopt;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12Resource> back;
    if(FAILED(queue->GetDevice(IID_PPV_ARGS(&device)))||
       FAILED(swap->GetBuffer(index,IID_PPV_ARGS(&back))))return std::nullopt;
    const auto desc=back->GetDesc();
    if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||
       desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||!desc.Width||!desc.Height||
       desc.SampleDesc.Count!=1)return std::nullopt;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 bytes{};
    device->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&bytes);
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width=bytes;buffer.Height=1;buffer.DepthOrArraySize=1;
    buffer.MipLevels=1;buffer.SampleDesc.Count=1;
    buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> readback;
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,
        &buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,
        IID_PPV_ARGS(&readback))))return std::nullopt;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commands;
    ComPtr<ID3D12Fence> fence;
    if(FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&allocator)))||
       FAILED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,
        allocator.Get(),nullptr,IID_PPV_ARGS(&commands)))||
       FAILED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&fence))))return std::nullopt;
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
    if(FAILED(commands->Close()))return std::nullopt;
    const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event)return std::nullopt;
    ID3D12CommandList* lists[]{commands.Get()};
    queue->ExecuteCommandLists(1,lists);
    if(FAILED(queue->Signal(fence.Get(),1)))std::terminate();
    const auto armed=fence->SetEventOnCompletion(1,event);
    const auto waited=SUCCEEDED(armed)?WaitForSingleObject(event,5000):WAIT_FAILED;
    CloseHandle(event);
    if(waited!=WAIT_OBJECT_0||fence->GetCompletedValue()!=1||
       FAILED(device->GetDeviceRemovedReason()))std::terminate();
    void* mapped{};
    if(FAILED(readback->Map(0,nullptr,&mapped)))return std::nullopt;
    const auto result=readColour(static_cast<const std::uint8_t*>(mapped),
        footprint.Footprint.RowPitch,static_cast<UINT>(desc.Width),
        desc.Height);
    readback->Unmap(0,nullptr);
    return result;
}
void reportColour(unsigned epoch,unsigned frame,const ColourSamples& source,
    const ColourSamples& d3d11,const ColourSamples& d3d12) {
    const auto print=[](const std::array<std::uint8_t,4>& pixel) {
        for(unsigned channel=0;channel<4;++channel) {
            if(channel)std::cout<<',';
            std::cout<<static_cast<unsigned>(pixel[channel]);
        }
    };
    for(unsigned index=0;index<4;++index) {
        std::cout<<"Colour trace epoch="<<epoch<<" frame="<<frame<<
            " sample="<<sampleNames[index]<<" source=";
        print(source[index]);std::cout<<" d3d11=";
        print(d3d11[index]);std::cout<<" d3d12=";
        print(d3d12[index]);std::cout<<'\n';
    }
}
void reportColourStage(unsigned epoch,unsigned frame,
    const ColourReadback& source,const ColourReadback& d3d11,
    const ColourReadback& d3d12) {
    std::cout<<"Colour stage epoch="<<epoch<<" frame="<<frame<<
        " sourceHash="<<std::hex<<colourHash(source.pixels)<<
        " d3d11Hash="<<colourHash(d3d11.pixels)<<
        " d3d12Hash="<<colourHash(d3d12.pixels)<<std::dec<<
        " fullEqual="<<(d3d11.pixels==d3d12.pixels)<<'\n';
}
HRESULT WINAPI wrapFactory(IDXGIFactory* factory,IUnknown* device,
    DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** output) noexcept {
    ++wrapperCalls;
    return wrapperNext?wrapperNext(factory,device,desc,output):E_UNEXPECTED;
}
HRESULT WINAPI replace(IDXGIFactory* factory,IUnknown* device,
    DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** output) noexcept {
    auto* state=active;
    if(!state||!state->next)return E_UNEXPECTED;
    if(factory!=state->factory||!desc||!output)
        return state->next(factory,device,desc,output);
    ComPtr<ID3D11Device> d11;
    if(!device||FAILED(device->QueryInterface(IID_PPV_ARGS(&d11))))
        return state->next(factory,device,desc,output);
    auto made=state->route->createFacade(state->next,factory,d11.Get(),
        *desc,true);
    if(auto* facade=std::get_if<ComPtr<IDXGISwapChain4>>(&made)) {
        state->facade=*facade;
        *output=facade->Detach();
        ++state->substitutions;
        return S_OK;
    }
    state->route->abandon();
    return state->next(factory,device,desc,output);
}
int run(const wchar_t* runtimeDirectory,const wchar_t* reshadePath,
    const wchar_t* enbPath,bool failAfterSrv,bool effectTrace,
    bool colourTrace,bool suppressD3D12) {
    const auto hash=rk::sha256File(reshadePath);
    if(!std::holds_alternative<std::string>(hash)||
       std::get<std::string>(hash)!=rk::reshade680FactoryCreateSite().moduleSha256)
        return 13;
    Module reshade{LoadLibraryW(reshadePath)};
    if(!reshade.value)return 14;
    std::optional<EffectTrace> trace;
    if(effectTrace) {
        trace.emplace();
        if(!trace->start(reshade.value))return 44;
    }
    Module enb;
    if(enbPath) {
        const auto enbHash=rk::sha256File(enbPath);
        if(!std::holds_alternative<std::string>(enbHash)||
           std::get<std::string>(enbHash)!=
               "35ff1543c8aaa5435a9002dc58d5459c29557ce8e5e5f91b25dfe4645be7bae3")return 35;
        enb.value=LoadLibraryW(enbPath);
        if(!enb.value)return 36;
        std::cout<<"Exact ENB 0.505 preloaded for the outer Present probe\n";
    }
    ComPtr<IDXGIFactory1> wrapper;
    using CreateFactory=HRESULT(WINAPI*)(REFIID,void**);
    auto* create=reinterpret_cast<CreateFactory>(
        GetProcAddress(reshade.value,"CreateDXGIFactory1"));
    if(!create||FAILED(create(IID_PPV_ARGS(&wrapper))))return 18;
    ComPtr<IDXGIAdapter1> adapter;
    for(UINT index=0;;++index) {
        ComPtr<IDXGIAdapter1> candidate;
        const auto hr=wrapper->EnumAdapters1(index,&candidate);
        if(hr==DXGI_ERROR_NOT_FOUND)break;
        if(FAILED(hr))return 11;
        DXGI_ADAPTER_DESC1 desc{};
        if(SUCCEEDED(candidate->GetDesc1(&desc))&&
           desc.VendorId==0x10de) {adapter=candidate;break;}
    }
    if(!adapter)return 12;
    Window window{CreateWindowExW(0,L"STATIC",L"RazKolbas game FG route",
        WS_POPUP,0,0,160,96,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)};
    if(!window.value)return 16;
    DXGI_SWAP_CHAIN_DESC game{};
    game.BufferDesc.Width=160;game.BufferDesc.Height=96;
    game.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    game.SampleDesc.Count=1;game.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT|DXGI_USAGE_SHADER_INPUT;
    game.BufferCount=3;game.OutputWindow=window.value;game.Windowed=TRUE;
    game.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    game.Flags=DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    std::unique_ptr<rk::FgReShadeEffectOwner> effectOwner;
    if(suppressD3D12) {
        auto armed=rk::FgReShadeEffectOwner::arm(adapter.Get(),game,
            trace.has_value());
        if(const auto* error=std::get_if<rk::Error>(&armed)) {
            std::cerr<<"effect owner: "<<error->message<<'\n';return 48;
        }
        effectOwner=std::move(std::get<
            std::unique_ptr<rk::FgReShadeEffectOwner>>(armed));
    }
    auto prepared=rk::FgPrivateSwapRoute::prepare(adapter.Get(),game,
        runtimeDirectory);
    if(const auto* error=std::get_if<rk::Error>(&prepared)) {
        std::cerr<<"game-route preparation: "<<error->message<<'\n';
        return 17;
    }
    auto route=std::move(std::get<std::unique_ptr<rk::FgPrivateSwapRoute>>(
        prepared));
    if(!std::holds_alternative<rk::Error>(route->acquireInputOwner()))return 53;
    if(effectOwner) {
        if(!effectOwner->initialSuppressed())return 49;
        effectOwner->commit();
    }
    // Reproduce the live chain: a downstream owner enables tearing after
    // the early private lower was prepared, before native game creation.
    game.Flags|=DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    ComPtr<ID3D11Device> d11;
    ComPtr<ID3D11DeviceContext> context;
    if(!enb.value&&FAILED(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,
        &d11,nullptr,&context)))return 15;
    const auto& reshadeSite=rk::reshade680FactoryCreateSite();
    auto* wrapperMethod=reinterpret_cast<rk::FactoryCreateFn>(
        (*reinterpret_cast<void***>(wrapper.Get()))[10]);
    if(!rk::isReshadeFactoryDelegateSite(wrapper.Get(),
        reinterpret_cast<std::uintptr_t>(reshade.value),
        reshadeSite.moduleSha256,wrapperMethod,reshadeSite))return 29;
    const auto facts=rk::inspectReshadeFactoryDelegate(wrapper.Get());
    const auto& site=rk::win11DxgiFactoryCreateSite();
    HMODULE owner{};
    if(!facts.methodExecutable||!facts.delegate||
       !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
           reinterpret_cast<LPCWSTR>(facts.vtable),&owner))return 19;
    Module ownerRef{owner};
    wchar_t nativePath[32768]{};
    if(!GetModuleFileNameW(owner,nativePath,32768))return 20;
    const auto nativeHash=rk::sha256File(nativePath);
    if(!std::holds_alternative<std::string>(nativeHash)||
       std::get<std::string>(nativeHash)!=site.moduleSha256||
       facts.vtable!=reinterpret_cast<std::uintptr_t>(owner)+site.tableRva||
       facts.createMethod!=reinterpret_cast<std::uintptr_t>(owner)+site.methodRva)
        return 20;
    std::vector<std::uint8_t> nativeImage(site.imageSize);
    const auto nativeBase=reinterpret_cast<std::uintptr_t>(owner);
    const auto codeFacts=rk::inspectFactoryMethodCode(facts.createMethod);
    std::cout<<"Native factory code: protection=0x"<<std::hex<<
        codeFacts.protection<<"; entry=";
    for(const auto byte:std::span(codeFacts.bytes).first(16))
        std::cout<<static_cast<unsigned>(byte)<<' ';
    std::cout<<std::dec<<'\n';
    if(!codeFacts.readable)return 31;
    std::memcpy(nativeImage.data()+site.tableRva+site.slot*sizeof(void*),
        reinterpret_cast<const void*>(facts.vtable+site.slot*sizeof(void*)),
        sizeof(void*));
    std::memcpy(nativeImage.data()+site.methodRva,
        codeFacts.bytes.data(),site.prologue.size());
    auto nativeValidated=rk::validateOwnedRouteSite(nativeImage,nativeBase,
        std::get<std::string>(nativeHash),std::filesystem::file_size(nativePath),
        site.tableRva,site);
    if(std::holds_alternative<rk::Error>(nativeValidated)) {
        const auto compatible=rk::inspectAndPinSteamFactoryInline(facts.createMethod);
        if(std::holds_alternative<bool>(compatible)&&std::get<bool>(compatible)) {
            std::copy_n(site.prologue.begin(),5,nativeImage.begin()+site.methodRva);
            nativeValidated=rk::validateOwnedRouteSite(nativeImage,nativeBase,
                std::get<std::string>(nativeHash),std::filesystem::file_size(nativePath),
                site.tableRva,site);
            std::cout<<"Exact Steam native factory chain accepted; inline hook preserved\n";
        } else if(const auto* error=std::get_if<rk::Error>(&compatible)) {
            std::cerr<<"Steam compatibility: "<<error->message<<'\n';
        }
    }
    if(const auto* error=std::get_if<rk::Error>(&nativeValidated)) {
        std::cerr<<"production native validation: "<<error->message<<"; live16=";
        for(const auto byte:std::span(nativeImage).subspan(site.methodRva,16))
            std::cerr<<std::hex<<static_cast<unsigned>(byte)<<' ';
        std::cerr<<std::dec<<'\n';
        return 31;
    }
    Callback callback{route.get(),
        reinterpret_cast<rk::FactoryCreateFn>(facts.createMethod),
        reinterpret_cast<IDXGIFactory*>(facts.delegate),0};
    rk::PointerPatch patch;
    auto** table=reinterpret_cast<void**>(facts.vtable);
    active=&callback;
    const auto installed=patch.apply(table+site.slot,
        reinterpret_cast<void*>(facts.createMethod),
        reinterpret_cast<void*>(&replace));
    if(!std::holds_alternative<bool>(installed)) {active=nullptr;return 21;}
    rk::PointerPatch wrapperPatch;
    auto** wrapperTable=*reinterpret_cast<void***>(wrapper.Get());
    wrapperNext=wrapperMethod;
    const auto wrapperInstalled=wrapperPatch.apply(
        wrapperTable+reshadeSite.slot,
        reinterpret_cast<void*>(wrapperMethod),
        reinterpret_cast<void*>(&wrapFactory));
    if(!std::holds_alternative<bool>(wrapperInstalled)) {
        patch.restore();active=nullptr;return 30;
    }
    ComPtr<IDXGISwapChain> upper;
    // Early probe failures need the same ownership order as successful exit.
    // A returned ENB/ReShade object must be released before slShutdown.
    struct OrderedExit {
        ComPtr<IDXGISwapChain>& upper;
        Callback& callback;
        ComPtr<IDXGIFactory1>& wrapper;
        std::unique_ptr<rk::FgPrivateSwapRoute>& route;
        ComPtr<ID3D11DeviceContext>& context;
        ComPtr<ID3D11Device>& d11;
        ComPtr<IDXGIAdapter1>& adapter;
        rk::PointerPatch& wrapperPatch;
        rk::PointerPatch& nativePatch;
        ~OrderedExit() {
            if(std::holds_alternative<rk::Error>(wrapperPatch.restore())||
               std::holds_alternative<rk::Error>(nativePatch.restore()))
                std::terminate();
            active=nullptr;wrapperNext=nullptr;
            if(context) {context->ClearState();context->Flush();}
            upper.Reset();callback.facade.Reset();wrapper.Reset();route.reset();
            context.Reset();d11.Reset();adapter.Reset();
        }
    } orderedExit{upper,callback,wrapper,route,context,d11,adapter,wrapperPatch,patch};
    HRESULT created{};
    if(enb.value) {
        auto* createDevice=reinterpret_cast<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>(
            GetProcAddress(enb.value,"D3D11CreateDeviceAndSwapChain"));
        if(!createDevice)return 37;
        created=createDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,
            &game,&upper,&d11,nullptr,&context);
        std::cout<<"ENB device/swap creation=0x"<<std::hex<<
            static_cast<unsigned>(created)<<std::dec<<'\n';
    } else created=wrapper->CreateSwapChain(d11.Get(),&game,&upper);
    const auto wrapperRestored=wrapperPatch.restore();
    const auto restored=patch.restore();
    active=nullptr;
    if(!std::holds_alternative<bool>(restored)||
       !std::holds_alternative<bool>(wrapperRestored)||FAILED(created)||!upper||
       callback.substitutions!=1||wrapperCalls!=1||!route->issued())return 22;
    auto ownerResult=route->acquireInputOwner();
    if(const auto* error=std::get_if<rk::Error>(&ownerResult)) {
        std::cout<<"Game route input owner unavailable: "<<error->message<<'\n';
        ComPtr<IDXGISwapChain4> proxy;
        ComPtr<ID3D12CommandQueue> queue;
        const auto nativeResult=route->inspectNativeLowerForProbe();
        if(route->inspectLowerForProbe(proxy,queue)&&
           std::holds_alternative<ComPtr<IDXGISwapChain4>>(nativeResult)) {
            const auto& native=std::get<ComPtr<IDXGISwapChain4>>(nativeResult);
            for(auto* current:{proxy.Get(),native.Get()}) {
                DXGI_SWAP_CHAIN_DESC desc{};
                const auto hr=current->GetDesc(&desc);
                std::cout<<"Owner descriptor proxy="<<(current==proxy.Get())<<
                    " hr="<<static_cast<unsigned>(hr)<<
                    " width="<<desc.BufferDesc.Width<<" height="<<desc.BufferDesc.Height<<
                    " format="<<desc.BufferDesc.Format<<" buffers="<<desc.BufferCount<<
                    " effect="<<desc.SwapEffect<<" window="<<desc.OutputWindow<<'\n';
            }
        }
        return 51;
    }
    auto inputOwner=std::get<rk::FgPresentationInputOwner>(std::move(ownerResult));
    ComPtr<ID3D12Device> ownerQueueDevice,ownerLowerDevice;
    if(!inputOwner.runtime||!inputOwner.nativeDevice||!inputOwner.nativeQueue||
       !inputOwner.lower||!inputOwner.nativeLower||
       FAILED(inputOwner.nativeQueue->GetDevice(IID_PPV_ARGS(&ownerQueueDevice)))||
       FAILED(inputOwner.nativeLower->GetDevice(IID_PPV_ARGS(&ownerLowerDevice)))||
       !sameIdentity(ownerQueueDevice.Get(),inputOwner.nativeDevice.Get())||
       !sameIdentity(ownerLowerDevice.Get(),inputOwner.nativeDevice.Get()))return 52;
    std::cout<<"Game route input owner exactDeviceQueue=1 runtimeRetained=1\n";
    std::cout<<"Game route owner buffer domains proxy="<<inputOwner.description.BufferCount<<
        " native="<<inputOwner.nativeDescription.BufferCount<<'\n';
    if(inputOwner.description.BufferCount!=3||inputOwner.nativeDescription.BufferCount!=2)
        return 55;
    if(!exerciseOwnerCopies(d11.Get(),context.Get(),inputOwner))return 54;
    ComPtr<IDXGISwapChain4> lowerForProbe;
    ComPtr<IDXGISwapChain4> nativeLowerForProbe;
    ComPtr<ID3D12CommandQueue> nativeQueueForProbe;
    ComPtr<ID3D12CommandQueue> reshadeQueueForProbe;
    ComPtr<IDXGISwapChain3> indexedLowerForProbe;
    ComPtr<ID3D12CommandQueue> lowerQueueForProbe;
    if(colourTrace&&
       (!route->inspectLowerForProbe(lowerForProbe,lowerQueueForProbe)||
        FAILED(lowerForProbe.As(&indexedLowerForProbe))))return 45;
    if(colourTrace)std::cout<<"Colour lower swap=0x"<<std::hex<<
        reinterpret_cast<std::uintptr_t>(lowerForProbe.Get())<<
        " queue=0x"<<reinterpret_cast<std::uintptr_t>(lowerQueueForProbe.Get())<<
        std::dec<<'\n';
    if(colourTrace) {
        auto native=route->inspectNativeLowerForProbe();
        if(auto* swap=std::get_if<ComPtr<IDXGISwapChain4>>(&native)) {
            nativeLowerForProbe=*swap;
            std::cout<<"Colour native lower swap=0x"<<std::hex<<
                reinterpret_cast<std::uintptr_t>(nativeLowerForProbe.Get())<<
                std::dec<<'\n';
        } else std::cout<<"Colour native lower unavailable: "<<
            std::get<rk::Error>(native).message<<'\n';
        auto queue=route->inspectNativeQueueForProbe();
        if(auto* nativeQueue=std::get_if<ComPtr<ID3D12CommandQueue>>(&queue)) {
            nativeQueueForProbe=*nativeQueue;
            std::cout<<"Colour native queue=0x"<<std::hex<<
                reinterpret_cast<std::uintptr_t>(nativeQueueForProbe.Get())<<
                std::dec<<'\n';
        } else std::cout<<"Colour native queue unavailable: "<<
            std::get<rk::Error>(queue).message<<'\n';
        if(trace&&nativeQueueForProbe) {
            std::lock_guard lock(trace->mutex);
            for(unsigned index=0;index<trace->count;++index) {
                const auto& observed=trace->runtimes[index];
                if(observed.api!=0xc000||!observed.commandQueueNative)continue;
                auto* runtimeQueue=reinterpret_cast<IUnknown*>(
                    observed.commandQueueNative);
                std::cout<<"Colour lower queue identity="<<
                    sameIdentity(runtimeQueue,nativeQueueForProbe.Get())<<'\n';
                auto* runtimeSwap=reinterpret_cast<IUnknown*>(observed.native);
                std::cout<<"Colour ReShade swap matches proxy/native="<<
                    sameIdentity(runtimeSwap,lowerForProbe.Get())<<'/'<<
                    sameIdentity(runtimeSwap,nativeLowerForProbe.Get())<<'\n';
                ComPtr<IDXGISwapChain> reshadeSwap;
                if(SUCCEEDED(runtimeSwap->QueryInterface(
                    IID_PPV_ARGS(&reshadeSwap)))) {
                    DXGI_SWAP_CHAIN_DESC runtimeDesc{};
                    ComPtr<ID3D12Device> runtimeSwapDevice;
                    const auto description=reshadeSwap->GetDesc(&runtimeDesc);
                    const auto device=reshadeSwap->GetDevice(
                        IID_PPV_ARGS(&runtimeSwapDevice));
                    std::cout<<"Colour ReShade native swap desc/device=0x"<<
                        std::hex<<static_cast<unsigned>(description)<<"/0x"<<
                        static_cast<unsigned>(device)<<std::dec;
                    if(SUCCEEDED(description))std::cout<<" size="<<
                        runtimeDesc.BufferDesc.Width<<'x'<<runtimeDesc.BufferDesc.Height<<
                        " buffers="<<runtimeDesc.BufferCount;
                    std::cout<<'\n';
                } else std::cout<<"Colour ReShade native is not a DXGI swap\n";
                if(SUCCEEDED(runtimeQueue->QueryInterface(
                    IID_PPV_ARGS(&reshadeQueueForProbe)))) {
                    ComPtr<ID3D12Device> reshadeDevice,lowerDevice,
                        nativeLowerDevice,routeQueueDevice;
                    ComPtr<ID3D12Resource> lowerBuffer;
                    ComPtr<ID3D12Resource> nativeLowerBuffer;
                    const bool deviceMatch=SUCCEEDED(
                        reshadeQueueForProbe->GetDevice(IID_PPV_ARGS(&reshadeDevice)))&&
                        SUCCEEDED(lowerForProbe->GetBuffer(0,IID_PPV_ARGS(&lowerBuffer)))&&
                        SUCCEEDED(lowerBuffer->GetDevice(IID_PPV_ARGS(&lowerDevice)))&&
                        sameIdentity(reshadeDevice.Get(),lowerDevice.Get());
                    std::cout<<"Colour ReShade queue device matches lower="<<
                        deviceMatch<<'\n';
                    if(nativeLowerForProbe&&SUCCEEDED(nativeLowerForProbe->GetBuffer(
                        0,IID_PPV_ARGS(&nativeLowerBuffer)))&&
                       nativeLowerBuffer)
                        nativeLowerBuffer->GetDevice(IID_PPV_ARGS(&nativeLowerDevice));
                    if(nativeQueueForProbe)nativeQueueForProbe->GetDevice(
                        IID_PPV_ARGS(&routeQueueDevice));
                    std::cout<<"Colour device identities reshade/lower/native/route="<<
                        sameIdentity(reshadeDevice.Get(),lowerDevice.Get())<<'/'<<
                        sameIdentity(reshadeDevice.Get(),nativeLowerDevice.Get())<<'/'<<
                        sameIdentity(reshadeDevice.Get(),routeQueueDevice.Get())<<'/'<<
                        sameIdentity(lowerDevice.Get(),nativeLowerDevice.Get())<<'/'<<
                        sameIdentity(lowerDevice.Get(),routeQueueDevice.Get())<<'\n';
                    if(reshadeDevice&&lowerDevice&&nativeLowerDevice)
                        std::cout<<"Colour device LUID reshade/lower/native="<<
                            reshadeDevice->GetAdapterLuid().HighPart<<':'<<
                            reshadeDevice->GetAdapterLuid().LowPart<<'/'<<
                            lowerDevice->GetAdapterLuid().HighPart<<':'<<
                            lowerDevice->GetAdapterLuid().LowPart<<'/'<<
                            nativeLowerDevice->GetAdapterLuid().HighPart<<':'<<
                            nativeLowerDevice->GetAdapterLuid().LowPart<<'\n';
                } else std::cout<<"Colour ReShade native queue QI failed\n";
                break;
            }
        }
    }
    const auto& diagnostics=static_cast<rk::FgD3D11SwapFacade*>(callback.facade.Get())->diagnostics();
    const auto report=[&] {
        std::cout<<"Bridge copy="<<diagnostics.copyPhase()<<" hr=0x"<<std::hex<<
            static_cast<unsigned>(diagnostics.copyResult())<<std::dec<<
            "; present="<<diagnostics.presentPhase()<<
            "; detail="<<diagnostics.copyDetail()<<
            "; first-failure="<<diagnostics.firstCopyFailurePhase()<<"/0x"<<
            std::hex<<static_cast<unsigned>(diagnostics.firstCopyFailure())<<std::dec<<'\n';
    };
    report();
    const auto directTest=callback.facade->Present(0,DXGI_PRESENT_TEST);
    const auto upperTest=upper->Present(0,DXGI_PRESENT_TEST);
    std::cout<<"direct/upper TEST=0x"<<std::hex<<
        static_cast<unsigned>(directTest)<<"/0x"<<
        static_cast<unsigned>(upperTest)<<std::dec<<'\n';
    ComPtr<ID3D11Texture2D> colour;
    ComPtr<ID3D11RenderTargetView> view;
    if(FAILED(upper->GetBuffer(0,IID_PPV_ARGS(&colour)))||
       FAILED(d11->CreateRenderTargetView(colour.Get(),nullptr,&view)))return 23;
    ComPtr<ID3D11Texture2D> innerColour;
    if(FAILED(callback.facade->GetBuffer(0,IID_PPV_ARGS(&innerColour))))return 43;
    D3D11_TEXTURE2D_DESC innerColourDesc{};
    innerColour->GetDesc(&innerColourDesc);
    ComPtr<ID3D11ShaderResourceView> sampleView;
    D3D11_TEXTURE2D_DESC colourDesc{};
    colour->GetDesc(&colourDesc);
    std::cout<<"Colour handoff upper/inner sameIdentity="<<
        sameIdentity(colour.Get(),innerColour.Get())<<
        "; upper format="<<static_cast<unsigned>(colourDesc.Format)<<
        " bind=0x"<<std::hex<<colourDesc.BindFlags<<
        " misc=0x"<<colourDesc.MiscFlags<<
        "; inner format="<<std::dec<<static_cast<unsigned>(innerColourDesc.Format)<<
        " bind=0x"<<std::hex<<innerColourDesc.BindFlags<<
        " misc=0x"<<innerColourDesc.MiscFlags<<std::dec<<'\n';
    const auto sampled=d11->CreateShaderResourceView(colour.Get(),nullptr,&sampleView);
    std::cout<<"Game shader-input contract: bind=0x"<<std::hex<<colourDesc.BindFlags<<
        "; SRV=0x"<<static_cast<unsigned>(sampled)<<std::dec<<'\n';
    if(FAILED(sampled))return 41;
    sampleView.Reset();
    if(failAfterSrv) {
        std::cout<<"Intentional early failure after SRV; ordered cleanup required\n";
        return 42;
    }
    const float red[4]{1.f,0.f,0.f,1.f};
    context->ClearRenderTargetView(view.Get(),red);
    reportPixel("Colour upper before Present",firstPixel(d11.Get(),context.Get(),colour.Get()));
    reportPixel("Colour inner before Present",firstPixel(d11.Get(),context.Get(),innerColour.Get()));
    DXGI_SWAP_CHAIN_DESC current{};
    if(FAILED(upper->GetDesc(&current)))return 34;
    std::cout<<"Game requested/lower flags=0x"<<std::hex<<game.Flags<<"/0x"<<current.Flags<<std::dec<<'\n';
    if(trace)trace->nextFrame(0);
    const auto presented=upper->Present(0,DXGI_PRESENT_ALLOW_TEARING);
    std::cout<<"Game route first Present=0x"<<std::hex<<
        static_cast<unsigned>(presented)<<std::dec<<'\n';
    report();
    if(FAILED(presented))return 24;
    if(trace)Sleep(30);
    reportPixel("Colour upper after Present",firstPixel(d11.Get(),context.Get(),colour.Get()));
    reportPixel("Colour inner after Present",firstPixel(d11.Get(),context.Get(),innerColour.Get()));
    innerColour.Reset();
    for(unsigned frame=1;frame<120;++frame) {
        if(trace)trace->nextFrame(frame);
        const float next[4]{frame%2?1.f:0.f,frame%3?0.f:1.f,0.f,1.f};
        if(colourTrace)uploadPattern(context.Get(),colour.Get());
        else context->ClearRenderTargetView(view.Get(),next);
        void* reenabledRuntime{};
        if(effectOwner&&trace&&frame==100) {
            std::lock_guard lock(trace->mutex);
            for(unsigned index=0;index<trace->count;++index) {
                if(trace->runtimes[index].api!=0xc000)continue;
                reenabledRuntime=trace->runtimes[index].identity;
                auto** runtimeTable=*reinterpret_cast<void***>(reenabledRuntime);
                reinterpret_cast<void(*)(void*,bool)>(runtimeTable[61])(
                    reenabledRuntime,true);
                break;
            }
            if(!reenabledRuntime)return 50;
        }
        const bool capture=colourTrace&&frame==100;
        const auto source=capture?sampleD3D11(d11.Get(),context.Get(),colour.Get()):
            std::optional<ColourReadback>{};
        if(capture&&(!source||source->samples!=sourcePattern))return 47;
        const auto lowerIndex=capture?
            indexedLowerForProbe->GetCurrentBackBufferIndex():0;
        if(FAILED(upper->Present(0,DXGI_PRESENT_ALLOW_TEARING)))return 38;
        if(reenabledRuntime) {
            auto** runtimeTable=*reinterpret_cast<void***>(reenabledRuntime);
            const bool reasserted=!reinterpret_cast<bool(*)(void*)>(
                runtimeTable[60])(reenabledRuntime)&&effectOwner->reassertions()>0;
            std::cout<<"Effect suppression direct-toggle reasserted="<<
                reasserted<<'\n';
            if(!reasserted)return 50;
        }
        if(capture) {
            const auto afterD11=sampleD3D11(d11.Get(),context.Get(),colour.Get());
            const auto afterD12=sampleD3D12(lowerForProbe.Get(),
                lowerQueueForProbe.Get(),lowerIndex);
            if(!afterD11||!afterD12)return 46;
            reportColour(0,frame,source->samples,
                afterD11->samples,afterD12->samples);
            reportColourStage(0,frame,*source,*afterD11,*afterD12);
        }
        if(trace)Sleep(30);
    }
    view.Reset();colour.Reset();context->ClearState();context->Flush();
    if(trace)trace->nextEpoch();
    if(FAILED(upper->ResizeBuffers(3,192,108,
        DXGI_FORMAT_R8G8B8A8_UNORM,current.Flags)))return 25;
    DXGI_SWAP_CHAIN_DESC after{};
    if(FAILED(upper->GetDesc(&after))||after.BufferDesc.Width!=192||
       after.BufferDesc.Height!=108)return 26;
    ownerResult=route->acquireInputOwner();
    if(const auto* error=std::get_if<rk::Error>(&ownerResult)) {
        std::cout<<"Game route resized input owner failed: "<<error->message<<'\n';
        return 56;
    }
    inputOwner=std::get<rk::FgPresentationInputOwner>(std::move(ownerResult));
    if(inputOwner.description.BufferDesc.Width!=192||
       inputOwner.description.BufferDesc.Height!=108||
       inputOwner.nativeDescription.BufferDesc.Width!=192||
       inputOwner.nativeDescription.BufferDesc.Height!=108||
       inputOwner.description.BufferCount!=3||inputOwner.nativeDescription.BufferCount!=2)
        return 57;
    std::cout<<"Game route resized input owner=1 proxyBuffers=3 nativeBuffers=2\n";
    if(FAILED(upper->GetBuffer(0,IID_PPV_ARGS(&colour)))||
       FAILED(d11->CreateRenderTargetView(colour.Get(),nullptr,&view)))return 39;
    for(unsigned frame=0;frame<120;++frame) {
        if(trace)trace->nextFrame(120+frame);
        if(colourTrace)uploadPattern(context.Get(),colour.Get());
        else context->ClearRenderTargetView(view.Get(),red);
        const bool capture=colourTrace&&frame==100;
        const auto source=capture?sampleD3D11(d11.Get(),context.Get(),colour.Get()):
            std::optional<ColourReadback>{};
        if(capture&&(!source||source->samples!=sourcePattern))return 47;
        const auto lowerIndex=capture?
            indexedLowerForProbe->GetCurrentBackBufferIndex():0;
        if(FAILED(upper->Present(0,DXGI_PRESENT_ALLOW_TEARING)))return 40;
        if(capture) {
            const auto afterD11=sampleD3D11(d11.Get(),context.Get(),colour.Get());
            const auto afterD12=sampleD3D12(lowerForProbe.Get(),
                lowerQueueForProbe.Get(),lowerIndex);
            if(!afterD11||!afterD12)return 46;
            reportColour(1,120+frame,source->samples,
                afterD11->samples,afterD12->samples);
            reportColourStage(1,120+frame,*source,*afterD11,*afterD12);
        }
        if(trace)Sleep(30);
    }
    view.Reset();colour.Reset();context->ClearState();context->Flush();
    upper.Reset();context->ClearState();context->Flush();
    std::cout<<"Game route FG-Off: substitution=1, Present=240 frames ok, resize=192x108\n";
    callback.facade.Reset();
    std::cout<<"facade released\n";
    wrapper.Reset();
    std::cout<<"wrapper released\n";
    indexedLowerForProbe.Reset();lowerForProbe.Reset();
    nativeLowerForProbe.Reset();lowerQueueForProbe.Reset();
    nativeQueueForProbe.Reset();
    reshadeQueueForProbe.Reset();
    inputOwner={};
    ownerQueueDevice.Reset();ownerLowerDevice.Reset();
    route.reset();
    std::cout<<"route shutdown\n";
    if(effectOwner) {
        std::cout<<"Effect suppression D3D12 init="<<
            effectOwner->suppressedInits()<<" failures=0 reassertions="<<
            effectOwner->reassertions()<<'\n';
        effectOwner.reset();
    }
    context.Reset();d11.Reset();
    std::cout<<"D3D11 released\n";
    adapter.Reset();
    std::cout<<"adapter/factory released\n";
    if(trace) {trace->report();trace->stop();}
    FreeLibrary(reshade.value);reshade.value=nullptr;
    std::cout<<"ReShade released\n";
    return 0;
}
}
int wmain(int argc,wchar_t** argv) {
    std::cout.setf(std::ios::unitbuf);
    if(argc<3||argc>6)return 1;
    const bool failAfterSrv=argc==6&&
        (std::wcscmp(argv[5],L"fail-after-srv")==0||
         std::wcscmp(argv[5],L"single-owner-early-fail")==0);
    const bool suppressD3D12=argc==6&&
        (std::wcscmp(argv[5],L"colour-single-d3d11")==0||
         std::wcscmp(argv[5],L"single-owner-registration")==0||
         std::wcscmp(argv[5],L"single-owner-early-fail")==0);
    const bool colourTrace=argc==6&&
        (std::wcscmp(argv[5],L"colour-single-d3d11")==0||
         std::wcscmp(argv[5],L"colour-trace")==0);
    const bool effectTrace=colourTrace||
        (argc==6&&std::wcscmp(argv[5],L"effect-trace")==0);
    if(argc==6&&!failAfterSrv&&!effectTrace&&!suppressD3D12)return 1;
    if(argc>=4&&std::wcscmp(argv[3],L"-")!=0) {
        const auto hash=rk::sha256File(argv[3]);
        const auto& profile=rk::steamFactoryInlineProfile();
        if(!std::holds_alternative<std::string>(hash)||
           std::get<std::string>(hash)!=profile.moduleHash||
           std::filesystem::file_size(argv[3])!=profile.fileSize)return 32;
        // Keep this explicit research preload resident until process exit.
        // Steam may install callbacks even if a later probe stage rejects.
        if(!LoadLibraryW(argv[3]))return 33;
        std::cout<<"Exact Steam overlay preloaded for the factory-chain probe\n";
    } else std::cout<<"No Steam overlay preload; pristine native factory required\n";
    try {return run(argv[1],argv[2],argc>=5?argv[4]:nullptr,
        failAfterSrv,effectTrace,colourTrace,suppressD3D12);}
    catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 2;
    }
}
