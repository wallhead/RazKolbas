#include "rk/FgCameraWriteCapture.hpp"
#include <cstring>
#include <limits>

namespace rk {
bool canCompareFgCameraWriteHistory(const FgCameraWrite& prior,const FgCameraWrite& next) noexcept {
    return prior.revision&&next.revision>prior.revision&&prior.buffer&&
        prior.buffer==next.buffer&&prior.generation&&prior.generation==next.generation;
}
bool sameFgCameraWrite(const FgCameraWrite& a,const FgCameraWrite& b) noexcept {
    return a.revision&&a.revision==b.revision&&a.buffer&&a.buffer==b.buffer&&
        a.generation&&a.generation==b.generation&&a.thread==b.thread&&a.bytes==b.bytes;
}
void FgCameraWriteCapture::selectBuffer(std::uint64_t buffer) noexcept {
    if(buffer_==buffer)return;
    if(generation_==std::numeric_limits<std::uint64_t>::max())buffer=0;
    else ++generation_;
    buffer_=buffer;mapped_=nullptr;latest_.reset();
}
bool FgCameraWriteCapture::mapped(std::uint64_t context,std::uint64_t buffer,
    std::uint64_t thread,std::uint64_t caller,const void* bytes,
    std::size_t size,bool succeeded) noexcept {
    if(!context_||context!=context_||!buffer_||buffer!=buffer_)return false;
    const bool valid=!mapped_&&succeeded&&thread&&caller&&bytes&&size==720;
    latest_.reset();mapped_=nullptr;
    if(!valid)return false;
    mapped_=bytes;thread_=thread;caller_=caller;
    return true;
}
bool FgCameraWriteCapture::beforeUnmap(std::uint64_t context,
    std::uint64_t buffer,std::uint64_t thread,std::uint64_t caller) noexcept {
    if(context!=context_||!buffer_||buffer!=buffer_)return false;
    const auto* bytes=mapped_;mapped_=nullptr;
    if(!bytes||thread!=thread_||!caller||
       revision_==std::numeric_limits<std::uint64_t>::max()) {
        latest_.reset();return false;
    }
    FgCameraWrite write{};
    std::memcpy(write.bytes.data(),bytes,write.bytes.size());
    write.revision=++revision_;write.buffer=buffer;write.generation=generation_;write.thread=thread;
    write.mapCaller=caller_;write.unmapCaller=caller;
    latest_=write;
    return true;
}
}
