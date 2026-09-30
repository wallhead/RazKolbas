#include "rk/FgCameraWriteHooks.hpp"
#include "rk/OwnedRouteProfile.hpp"
#include "rk/PatchDescriptor.hpp"
#include "rk/PointerPatch.hpp"
#include "rk/RendererHook.hpp"
#include "rk/SwapObserver.hpp"
#include <Windows.h>
#include <intrin.h>
#include <wrl/client.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <vector>

namespace rk {
bool isFgCameraWriteBuffer(const D3D11_BUFFER_DESC& desc) noexcept {
    return desc.ByteWidth==720&&desc.BindFlags==D3D11_BIND_CONSTANT_BUFFER&&
        desc.Usage==D3D11_USAGE_DYNAMIC&&desc.CPUAccessFlags==D3D11_CPU_ACCESS_WRITE&&
        desc.MiscFlags==0&&desc.StructureByteStride==0;
}
namespace {
using MapFn=HRESULT(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,
    UINT,D3D11_MAP,UINT,D3D11_MAPPED_SUBRESOURCE*);
using UnmapFn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,ID3D11Resource*,UINT);
struct State {
    explicit State(ID3D11DeviceContext* ctx):context(ctx),capture(
        reinterpret_cast<std::uintptr_t>(ctx)) {}
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    FgCameraWriteCapture capture;
    std::uintptr_t bufferCell{},contextCell{};
    MapFn nextMap{};UnmapFn nextUnmap{};
    PointerPatch mapPatch,unmapPatch;
    std::atomic<bool> armed{false};
    FgCameraWriteObservation facts;
    std::mutex mutex;
};
std::atomic<State*> active{};
std::mutex installMutex;
bool read(std::uintptr_t address,void* out,std::size_t size) noexcept {
    SIZE_T copied{};
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),
        out,size,&copied)&&copied==size;
}
bool select(State& state,ID3D11Resource* resource) {
    std::uintptr_t selected{},ctx{};
    if(!read(state.contextCell,&ctx,sizeof(ctx))||
       ctx!=reinterpret_cast<std::uintptr_t>(state.context.Get())||
       !read(state.bufferCell,&selected,sizeof(selected))) {
        state.capture.selectBuffer(0);state.buffer.Reset();return false;
    }
    if(selected!=reinterpret_cast<std::uintptr_t>(state.buffer.Get())) {
        state.capture.selectBuffer(0);state.buffer.Reset();
    }
    if(!selected||selected!=reinterpret_cast<std::uintptr_t>(resource))return false;
    if(!state.buffer) {
        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
        if(FAILED(resource->QueryInterface(IID_PPV_ARGS(&buffer))))return false;
        D3D11_BUFFER_DESC desc{};buffer->GetDesc(&desc);
        if(!isFgCameraWriteBuffer(desc))return false;
        state.buffer=std::move(buffer);
        state.capture.selectBuffer(selected);
    }
    return true;
}
HRESULT STDMETHODCALLTYPE mapProxy(ID3D11DeviceContext* context,
    ID3D11Resource* resource,UINT subresource,D3D11_MAP kind,UINT flags,
    D3D11_MAPPED_SUBRESOURCE* mapped) noexcept {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    auto* state=active.load(std::memory_order_acquire);
    if(!state||!state->nextMap)std::terminate();
    const auto hr=state->nextMap(context,resource,subresource,kind,flags,mapped);
    const auto savedError=GetLastError();
    if(state->armed.load(std::memory_order_acquire)&&context==state->context.Get()) {
        try {
            std::scoped_lock lock(state->mutex);
            if(select(*state,resource)) {
                const bool valid=SUCCEEDED(hr)&&mapped&&subresource==0&&
                    (kind==D3D11_MAP_WRITE_DISCARD||kind==D3D11_MAP_WRITE_NO_OVERWRITE);
                if(!state->capture.mapped(reinterpret_cast<std::uintptr_t>(context),
                    reinterpret_cast<std::uintptr_t>(resource),GetCurrentThreadId(),caller,
                    valid?mapped->pData:nullptr,720,valid))++state->facts.rejected;
            }
        }catch(...) {state->armed.store(false,std::memory_order_release);}
    }
    SetLastError(savedError);return hr;
}
void STDMETHODCALLTYPE unmapProxy(ID3D11DeviceContext* context,
    ID3D11Resource* resource,UINT subresource) noexcept {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    auto* state=active.load(std::memory_order_acquire);
    if(!state||!state->nextUnmap)std::terminate();
    const auto savedError=GetLastError();
    if(state->armed.load(std::memory_order_acquire)&&context==state->context.Get()) {
        try {
            std::scoped_lock lock(state->mutex);
            if(select(*state,resource)) {
                if(state->capture.beforeUnmap(reinterpret_cast<std::uintptr_t>(context),
                    reinterpret_cast<std::uintptr_t>(resource),
                    subresource==0?GetCurrentThreadId():0,caller)) {
                    ++state->facts.completed;
                    const auto write=state->capture.latest();
                    bool found=false;
                    for(unsigned i=0;i<state->facts.writerCount;++i) {
                        const auto& writer=state->facts.writers[i];
                        found|=writer.mapCaller==write->mapCaller&&
                            writer.unmapCaller==write->unmapCaller;
                    }
                    if(!found) {
                        if(state->facts.writerCount<state->facts.writers.size())
                            state->facts.writers[state->facts.writerCount++]={
                                write->mapCaller,write->unmapCaller};
                        else ++state->facts.writerOverflow;
                    }
                }else ++state->facts.rejected;
            }
        }catch(...) {state->armed.store(false,std::memory_order_release);}
    }
    SetLastError(savedError);
    state->nextUnmap(context,resource,subresource);
}
bool imageOwner(std::uintptr_t address,HMODULE owner,bool executable) {
    MEMORY_BASIC_INFORMATION memory{};
    if(!VirtualQuery(reinterpret_cast<const void*>(address),&memory,sizeof(memory))||
       memory.State!=MEM_COMMIT||memory.Type!=MEM_IMAGE||memory.AllocationBase!=owner||
       (memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    return !executable||(memory.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|
        PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
}
Result<bool> installFgCameraWriteHooks(ID3D11DeviceContext* context,
    std::uintptr_t game,std::string_view gameHash,std::string_view disabled) {
    std::scoped_lock installLock(installMutex);
    if(active.load(std::memory_order_acquire))
        return Error{ErrorCode::Conflict,"Camera write observer already attempted"};
    if(!context||!game||gameHash!=skyrim1170CreationProfile().gameSha256)
        return Error{ErrorCode::Unsupported,"Camera write game/context identity differs"};
    const auto sites=enb505CameraWriteSites();
    for(const auto& site:sites)if(patchDisabled(disabled,site.id))return false;
    try {
        std::uintptr_t table{};
        if(!read(reinterpret_cast<std::uintptr_t>(context),&table,sizeof(table)))
            return Error{ErrorCode::Unsupported,"Camera context table is unreadable"};
        MEMORY_BASIC_INFORMATION tableMemory{};
        if(!VirtualQuery(reinterpret_cast<void*>(table),&tableMemory,sizeof(tableMemory)))
            return Error{ErrorCode::Unsupported,"Camera context table has no image owner"};
        const auto owner=static_cast<HMODULE>(tableMemory.AllocationBase);
        const auto base=reinterpret_cast<std::uintptr_t>(owner);
        if(table!=base+sites.front().tableRva||!imageOwner(table,owner,false))
            return Error{ErrorCode::Unsupported,"Camera context is not exact ENB 0.505"};
        wchar_t path[32768]{};
        const auto length=GetModuleFileNameW(owner,path,32768);
        if(!length||length>=32768)return Error{ErrorCode::Io,"Camera ENB path unavailable"};
        struct FileLock {HANDLE value;~FileLock(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
        FileLock fileLock{CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,
            OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
        if(fileLock.value==INVALID_HANDLE_VALUE)return Error{ErrorCode::Io,"Cannot lock camera ENB file"};
        const auto hash=sha256File(std::filesystem::path(path));
        if(const auto* error=std::get_if<Error>(&hash))return *error;
        IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};
        if(!read(base,&dos,sizeof(dos))||dos.e_magic!=IMAGE_DOS_SIGNATURE||
           dos.e_lfanew<=0||dos.e_lfanew>0x1000||
           !read(base+dos.e_lfanew,&nt,sizeof(nt))||nt.Signature!=IMAGE_NT_SIGNATURE||
           nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||
           nt.OptionalHeader.SizeOfImage!=sites.front().imageSize)
            return Error{ErrorCode::Unsupported,"Camera ENB mapped image differs"};
        // Sparse image: only contract slots/prologues are needed by the pure
        // validator; every copied range is independently checked and owned.
        std::vector<std::uint8_t> image(sites.front().imageSize);
        for(const auto& site:sites) {
            const auto method=base+site.methodRva;
            if(!imageOwner(method,owner,true)||
               !read(base+site.tableRva+site.slot*sizeof(void*),
                   image.data()+site.tableRva+site.slot*sizeof(void*),sizeof(void*))||
               !read(method,image.data()+site.methodRva,site.prologue.size()))
                return Error{ErrorCode::Unsupported,"Camera ENB method ownership differs"};
            const auto checked=validateOwnedRouteSite(image,base,std::get<std::string>(hash),
                static_cast<std::size_t>(std::filesystem::file_size(path)),site.tableRva,site);
            if(const auto* error=std::get_if<Error>(&checked))return *error;
        }
        auto pending=std::make_unique<State>(context);
        pending->bufferCell=game+0x3288788;pending->contextCell=game+0x32887b0;
        std::uintptr_t ignored{};
        if(!read(pending->bufferCell,&ignored,sizeof(ignored))||
           !read(pending->contextCell,&ignored,sizeof(ignored))||
           context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)
            return Error{ErrorCode::Unsupported,"Camera globals or immediate context unavailable"};
        pending->nextMap=reinterpret_cast<MapFn>(base+sites[0].methodRva);
        pending->nextUnmap=reinterpret_cast<UnmapFn>(base+sites[1].methodRva);
        HMODULE self{},pinnedOwner{};
        constexpr DWORD pin=GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
        if(!GetModuleHandleExW(pin,reinterpret_cast<LPCWSTR>(&mapProxy),&self)||
           !GetModuleHandleExW(pin,reinterpret_cast<LPCWSTR>(table),&pinnedOwner))
            return Error{ErrorCode::Unavailable,"Cannot pin camera callback lifetimes"};
        auto* state=pending.release(); // bounded process-lifetime callback lease
        active.store(state,std::memory_order_release);
        auto** slots=reinterpret_cast<void**>(table);
        const auto map=state->mapPatch.apply(slots+sites[0].slot,
            reinterpret_cast<void*>(state->nextMap),reinterpret_cast<void*>(&mapProxy));
        if(const auto* error=std::get_if<Error>(&map))return *error;
        const auto unmap=state->unmapPatch.apply(slots+sites[1].slot,
            reinterpret_cast<void*>(state->nextUnmap),reinterpret_cast<void*>(&unmapProxy));
        if(const auto* error=std::get_if<Error>(&unmap)) {
            const auto rollback=state->mapPatch.restore();
            if(const auto* failure=std::get_if<Error>(&rollback);
               failure&&failure->code!=ErrorCode::Conflict)std::terminate();
            return *error;
        }
        state->armed.store(true,std::memory_order_release);
        return true;
    }catch(const std::exception& e) {return Error{ErrorCode::Unavailable,e.what()};}
}
FgCameraWriteObservation snapshotFgCameraWrites() noexcept {
    auto* state=active.load(std::memory_order_acquire);
    if(!state||!state->armed.load(std::memory_order_acquire))return {};
    try {
        std::scoped_lock lock(state->mutex);
        // Refresh generations even when the selected buffer has not been mapped.
        select(*state,state->buffer.Get());
        auto result=state->facts;result.latest=state->capture.latest();return result;
    }catch(...) {state->armed.store(false,std::memory_order_release);return {};}
}
}
