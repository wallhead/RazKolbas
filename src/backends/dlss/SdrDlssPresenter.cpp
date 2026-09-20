#include "rk/SdrDlssPresenter.hpp"
#include "rk/D3D11StateScope.hpp"
#include "rk/PatchDescriptor.hpp"
#include "rk/SdrDisplayCopy.hpp"
#include <nvsdk_ngx_helpers_d3d.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

namespace rk {
namespace {
namespace fs=std::filesystem;
constexpr std::string_view runtimeHash="c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e";
bool success(NVSDK_NGX_Result result) { return result==NVSDK_NGX_Result_Success; }
fs::path moduleDirectory() {
    HMODULE self{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&moduleDirectory),&self))return {};
    std::array<wchar_t,32768> buffer{};
    const auto length=GetModuleFileNameW(self,buffer.data(),static_cast<DWORD>(buffer.size()));
    return length&&length<buffer.size()?fs::path(buffer.data()).parent_path():fs::path{};
}
Result<bool> exactRuntime(const fs::path& path) {
    if(!fs::is_regular_file(path)||fs::file_size(path)!=58956400)
        return Error{ErrorCode::Unavailable,"Pinned NVIDIA SDR runtime missing or wrong size"};
    std::ifstream file(path,std::ios::binary);
    if(!file)return Error{ErrorCode::Io,"Cannot read NVIDIA SR runtime"};
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fs::file_size(path)));
    if(!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
        return Error{ErrorCode::Io,"Cannot hash NVIDIA SR runtime"};
    if(sha256(bytes)!=runtimeHash)
        return Error{ErrorCode::Conflict,"NVIDIA SR runtime hash differs"};
    return true;
}
Microsoft::WRL::ComPtr<IUnknown> identity(IUnknown* object) {
    Microsoft::WRL::ComPtr<IUnknown> result;
    if(object)object->QueryInterface(IID_PPV_ARGS(&result));
    return result;
}
Result<bool> validateSources(ID3D11Device* device,ID3D11Texture2D* color,
    ID3D11Texture2D* motion,ID3D11Texture2D* depth,UINT width,UINT height) {
    const std::array<ID3D11Texture2D*,3> sources{color,motion,depth};
    const std::array formats{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R16G16_FLOAT,
        DXGI_FORMAT_R24G8_TYPELESS};
    for(std::size_t i=0;i<sources.size();++i) {
        if(!sources[i])return Error{ErrorCode::InvalidInput,"Null SDR source"};
        Microsoft::WRL::ComPtr<ID3D11Device> owner;
        sources[i]->GetDevice(&owner);
        if(!owner||identity(owner.Get()).Get()!=identity(device).Get())
            return Error{ErrorCode::Conflict,"SDR source belongs to another device"};
        D3D11_TEXTURE2D_DESC desc{};sources[i]->GetDesc(&desc);
        if(desc.Format!=formats[i]||desc.Width!=width||desc.Height!=height||
           desc.MipLevels!=1||desc.ArraySize!=1||desc.SampleDesc.Count!=1||
           desc.Usage!=D3D11_USAGE_DEFAULT)
            return Error{ErrorCode::Unsupported,"SDR source format or dimensions changed"};
    }
    return true;
}
}
Result<bool> SdrDlssPresenter::initialize(ID3D11Device* device,
    ID3D11DeviceContext* context,UINT width,UINT height,
    UINT displayWidth,UINT displayHeight,bool reduced) {
    const auto folder=moduleDirectory();
    if(folder.empty())return Error{ErrorCode::Unavailable,"Cannot locate RazKolbas module"};
    const auto runtimeDir=folder/L"RazKolbasRuntime";
    const auto runtimePath=runtimeDir/L"nvngx_dlss.dll";
    runtimeFile_=CreateFileW(runtimePath.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,
        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(runtimeFile_==INVALID_HANDLE_VALUE)
        return Error{ErrorCode::Unavailable,"Pinned NVIDIA SR runtime not installed"};
    if(const auto checked=exactRuntime(runtimePath);const auto error=std::get_if<Error>(&checked))return *error;
    if(const auto existing=GetModuleHandleW(L"nvngx_dlss.dll")) {
        wchar_t name[32768]{};
        const auto length=GetModuleFileNameW(existing,name,32768);
        if(!length||length>=32768)
            return Error{ErrorCode::Conflict,"Cannot identify preloaded NVIDIA SR runtime"};
        if(const auto checked=exactRuntime(name);const auto error=std::get_if<Error>(&checked))
            return Error{ErrorCode::Conflict,"Preloaded NVIDIA SR runtime differs"};
    }
    auto isolated=D3D11StateScope::begin(context);
    if(const auto error=std::get_if<Error>(&isolated))return *error;
    auto scope=std::move(std::get<std::unique_ptr<D3D11StateScope>>(isolated));
    const auto dataPath=fs::temp_directory_path()/L"RazKolbasNgxData";
    fs::create_directories(dataPath);
    const wchar_t* paths[]={runtimeDir.c_str()};
    NVSDK_NGX_FeatureCommonInfo info{};info.PathListInfo={paths,1};
    if(!success(NVSDK_NGX_D3D11_Init_with_ProjectID("b3340e44-a57e-4b98-9318-d7150829d110",
        NVSDK_NGX_ENGINE_TYPE_CUSTOM,reduced?"RazKolbas-SDR-SR-1":"RazKolbas-SDR-DLAA-1",
        dataPath.c_str(),device,&info)))
        return Error{ErrorCode::Unavailable,"NVIDIA NGX SDR init failed"};
    if(!success(NVSDK_NGX_D3D11_GetCapabilityParameters(&parameters_))||!parameters_)
        return Error{ErrorCode::Unavailable,"NVIDIA NGX SDR parameters unavailable"};
    int available{};
    if(!success(parameters_->Get(NVSDK_NGX_Parameter_SuperSampling_Available,&available))||!available)
        return Error{ErrorCode::Unsupported,"NVIDIA DLSS unavailable for SDR session"};
    NVSDK_NGX_DLSS_Create_Params create{};
    create.Feature.InWidth=width;create.Feature.InHeight=height;
    create.Feature.InTargetWidth=displayWidth;
    create.Feature.InTargetHeight=displayHeight;
    create.Feature.InPerfQualityValue=reduced?NVSDK_NGX_PerfQuality_Value_MaxQuality:
        NVSDK_NGX_PerfQuality_Value_DLAA;
    create.InFeatureCreateFlags=NVSDK_NGX_DLSS_Feature_Flags_MVLowRes|
        NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;
    if(!success(NGX_D3D11_CREATE_DLSS_EXT(context,&feature_,parameters_,&create))||!feature_)
        return Error{ErrorCode::Unavailable,"NVIDIA SDR DLAA creation failed"};
    const auto loaded=GetModuleHandleW(L"nvngx_dlss.dll");
    wchar_t loadedName[32768]{};
    const auto length=loaded?GetModuleFileNameW(loaded,loadedName,32768):0;
    if(!length||length>=32768)
        return Error{ErrorCode::Conflict,"Cannot identify loaded NVIDIA SR runtime"};
    if(const auto checked=exactRuntime(loadedName);const auto error=std::get_if<Error>(&checked))return *error;
    device_=device;width_=width;height_=height;
    displayWidth_=displayWidth;displayHeight_=displayHeight;
    reduced_=reduced;initialized_=true;
    return true;
}
Result<bool> SdrDlssPresenter::render(ID3D11Device* device,
    ID3D11DeviceContext* context,ID3D11Texture2D* backbuffer,
    ID3D11Texture2D* motion,ID3D11Texture2D* depth,NgxJitter jitter) {
    return renderFrame(device,context,backbuffer,motion,depth,backbuffer,jitter,false);
}
Result<bool> SdrDlssPresenter::renderSr(ID3D11Device* device,
    ID3D11DeviceContext* context,ID3D11Texture2D* scene,
    ID3D11Texture2D* motion,ID3D11Texture2D* depth,
    ID3D11Texture2D* backbuffer,NgxJitter jitter) {
    return renderFrame(device,context,scene,motion,depth,backbuffer,jitter,true);
}
Result<bool> SdrDlssPresenter::renderFrame(ID3D11Device* device,
    ID3D11DeviceContext* context,ID3D11Texture2D* scene,
    ID3D11Texture2D* motion,ID3D11Texture2D* depth,
    ID3D11Texture2D* backbuffer,NgxJitter jitter,bool reduced) {
    if(!std::isfinite(jitter.x)||!std::isfinite(jitter.y)||
       std::abs(jitter.x)>0.5001f||std::abs(jitter.y)>0.5001f)
        return Error{ErrorCode::InvalidInput,"SDR DLSS jitter is invalid"};
    if(!device||!context||!scene||!backbuffer||!motion||!depth||
       context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)
        return Error{ErrorCode::InvalidInput,"SDR DLSS needs device/context and scene guides"};
    Microsoft::WRL::ComPtr<ID3D11Device> contextDevice;
    context->GetDevice(&contextDevice);
    if(!contextDevice||identity(contextDevice.Get()).Get()!=identity(device).Get()||
       (initialized_&&identity(device_.Get()).Get()!=identity(device).Get()))
        return Error{ErrorCode::Conflict,"SDR DLSS device identity differs"};
    D3D11_TEXTURE2D_DESC back{};backbuffer->GetDesc(&back);
    D3D11_TEXTURE2D_DESC input{};scene->GetDesc(&input);
    Microsoft::WRL::ComPtr<ID3D11Device> backOwner;
    backbuffer->GetDevice(&backOwner);
    if(!backOwner||identity(backOwner.Get()).Get()!=identity(device).Get())
        return Error{ErrorCode::Conflict,"SDR DLSS destination belongs to another device"};
    if(!back.Width||!back.Height||back.Width>8192||back.Height>8192||
       !input.Width||!input.Height||input.Width>back.Width||input.Height>back.Height||
       (reduced?input.Width==back.Width&&input.Height==back.Height:
            input.Width!=back.Width||input.Height!=back.Height)||
       back.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||back.SampleDesc.Count!=1||
       back.MipLevels!=1||back.ArraySize!=1||back.Usage!=D3D11_USAGE_DEFAULT||
       !(back.BindFlags&D3D11_BIND_RENDER_TARGET))
        return Error{ErrorCode::InvalidInput,"SDR DLSS render/display extents or target format differ"};
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> activeView;
    Microsoft::WRL::ComPtr<ID3D11Resource> activeTarget;
    context->OMGetRenderTargets(1,activeView.GetAddressOf(),nullptr);
    if(activeView)activeView->GetResource(&activeTarget);
    if(!activeTarget||identity(activeTarget.Get()).Get()!=identity(backbuffer).Get())
        return Error{ErrorCode::Conflict,"SDR DLSS destination is not active RTV0"};
    if(initialized_&&(input.Width!=width_||input.Height!=height_||
       back.Width!=displayWidth_||back.Height!=displayHeight_||reduced!=reduced_))
        return Error{ErrorCode::Unsupported,"SDR DLSS mode or extent changed"};
    if(const auto valid=validateSources(device,scene,motion,depth,input.Width,input.Height);
       const auto error=std::get_if<Error>(&valid))return *error;
    if(!initialized_) {
        const auto ready=initialize(device,context,input.Width,input.Height,
            back.Width,back.Height,reduced);
        if(const auto error=std::get_if<Error>(&ready))return *error;
    }
    auto& slot=slots_[nextSlot_++%slots_.size()];
    if(slot.inFlight) {
        const auto done=context->GetData(slot.completion.Get(),nullptr,0,
            D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(done==S_FALSE)return false;
        if(FAILED(done)||FAILED(device->GetDeviceRemovedReason()))
            return Error{ErrorCode::DeviceRemoved,"SDR DLSS frame retirement failed"};
        slot.inFlight=false;
    }
    if(!slot.frame) {
        const std::array<ID3D11Texture2D*,3> sources{scene,motion,depth};
        auto prepared=reduced?prepareSdrSrInputsForDisplay(context,sources,
            back.Width,back.Height):prepareSdrSrInputs(context,sources);
        if(const auto error=std::get_if<Error>(&prepared))return *error;
        slot.frame.emplace(std::move(std::get<PreparedSrInputs>(prepared)));
        const D3D11_QUERY_DESC query{D3D11_QUERY_EVENT,0};
        if(FAILED(device->CreateQuery(&query,&slot.completion)))
            return Error{ErrorCode::Unavailable,"SDR DLSS completion query unavailable"};
    } else {
        context->CopyResource(slot.frame->color(),scene);
        context->CopyResource(slot.frame->motion(),motion);
        context->CopyResource(slot.frame->depth(),depth);
    }
    auto isolated=D3D11StateScope::begin(context);
    if(const auto error=std::get_if<Error>(&isolated))return *error;
    auto scope=std::move(std::get<std::unique_ptr<D3D11StateScope>>(isolated));
    NVSDK_NGX_D3D11_DLSS_Eval_Params eval{};
    eval.Feature.pInColor=slot.frame->color();
    eval.Feature.pInOutput=slot.frame->output();
    eval.pInDepth=slot.frame->depth();
    eval.pInMotionVectors=slot.frame->motion();
    eval.InRenderSubrectDimensions={width_,height_};
    eval.InReset=(submittedFrames_==0||resetPending_)?1:0;
    eval.InJitterOffsetX=jitter.x;
    eval.InJitterOffsetY=jitter.y;
    eval.InMVScaleX=static_cast<float>(width_);
    eval.InMVScaleY=static_cast<float>(height_);
    eval.InPreExposure=eval.InExposureScale=1.0f;
    if(!success(NGX_D3D11_EVALUATE_DLSS_EXT(context,feature_,parameters_,&eval)))
        return Error{ErrorCode::Unavailable,"NVIDIA SDR DLSS evaluation failed"};
    scope.reset();
    const auto copied=copySdrDisplayFrame(context,backbuffer,slot.frame->output());
    if(const auto error=std::get_if<Error>(&copied))return *error;
    context->End(slot.completion.Get());
    slot.inFlight=true;
    resetPending_=false;
    ++submittedFrames_;
    return true;
}
Result<bool> SdrDlssPresenter::stop(ID3D11DeviceContext* context) {
    if(!initialized_)return true;
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)
        return Error{ErrorCode::InvalidInput,"SDR DLAA shutdown needs immediate context"};
    Microsoft::WRL::ComPtr<ID3D11Device> owner;
    context->GetDevice(&owner);
    if(!owner||identity(owner.Get()).Get()!=identity(device_.Get()).Get())
        return Error{ErrorCode::Conflict,"SDR DLAA shutdown device differs"};
    for(auto& slot:slots_)if(slot.inFlight) {
        const auto done=context->GetData(slot.completion.Get(),nullptr,0,
            D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(done==S_FALSE)return false;
        if(FAILED(done)||FAILED(device_->GetDeviceRemovedReason()))
            return Error{ErrorCode::DeviceRemoved,"SDR DLAA shutdown fence failed"};
        slot.inFlight=false;
    }
    auto isolated=D3D11StateScope::begin(context);
    if(const auto error=std::get_if<Error>(&isolated))return *error;
    auto scope=std::move(std::get<std::unique_ptr<D3D11StateScope>>(isolated));
    if(!success(NVSDK_NGX_D3D11_ReleaseFeature(feature_)))
        return Error{ErrorCode::Unavailable,"SDR DLAA feature release failed"};
    feature_=nullptr;
    if(!success(NVSDK_NGX_D3D11_DestroyParameters(parameters_)))
        return Error{ErrorCode::Unavailable,"SDR DLAA parameter destruction failed"};
    parameters_=nullptr;
    if(!success(NVSDK_NGX_D3D11_Shutdown1(device_.Get())))
        return Error{ErrorCode::Unavailable,"SDR DLAA NGX shutdown failed"};
    scope.reset();
    for(auto& slot:slots_) {
        slot.frame.reset();slot.completion.Reset();slot.inFlight=false;
    }
    device_.Reset();
    CloseHandle(runtimeFile_);runtimeFile_=INVALID_HANDLE_VALUE;
    width_=height_=displayWidth_=displayHeight_=0;
    reduced_=false;initialized_=false;
    return true;
}
}
