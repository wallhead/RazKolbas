#include "rk/SdrDlssPresenter.hpp"
#include "rk/D3D11StateScope.hpp"
#include "rk/PatchDescriptor.hpp"
#include "rk/SdrDisplayCopy.hpp"
#include <nvsdk_ngx_helpers_d3d.h>
#include <nvsdk_ngx_helpers.h>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

namespace rk {
namespace {
namespace fs=std::filesystem;
using Microsoft::WRL::ComPtr;
constexpr std::string_view runtimeHash="c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e";
bool success(NVSDK_NGX_Result result) { return result==NVSDK_NGX_Result_Success; }
Result<bool> waitForGpuEvent(ID3D11Device* device,ID3D11DeviceContext* context,
    ID3D11Query* event) {
    if(!device||!context||!event)
        return Error{ErrorCode::InvalidInput,"GPU completion wait is missing its device, context or event"};
    context->Flush();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    for(;;) {
        BOOL complete=FALSE;
        const auto status=context->GetData(event,&complete,sizeof(complete),
            D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(status==S_OK&&complete)return true;
        if(FAILED(status)||FAILED(device->GetDeviceRemovedReason()))
            return Error{ErrorCode::DeviceRemoved,"Prepared SR GPU completion failed"};
        if(std::chrono::steady_clock::now()>=deadline)
            return Error{ErrorCode::Unavailable,"Prepared SR GPU completion timed out"};
        std::this_thread::yield();
    }
}
std::optional<NVSDK_NGX_PerfQuality_Value> ngxQuality(UpscaleQuality quality) noexcept {
    switch(quality) {
    case UpscaleQuality::NativeAA:return NVSDK_NGX_PerfQuality_Value_DLAA;
    case UpscaleQuality::Quality:return NVSDK_NGX_PerfQuality_Value_MaxQuality;
    case UpscaleQuality::Balanced:return NVSDK_NGX_PerfQuality_Value_Balanced;
    case UpscaleQuality::Performance:return NVSDK_NGX_PerfQuality_Value_MaxPerf;
    case UpscaleQuality::UltraPerformance:return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
    }
    return std::nullopt;
}
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
Result<bool> validatePrepared(ID3D11Device* device,const PreparedSrInputs& frame,
    SrFrameMetadata metadata,NgxJitter jitter) {
    if(!device||!metadata.frameId||!metadata.generation||
       !std::isfinite(jitter.x)||!std::isfinite(jitter.y)||
       std::abs(jitter.x)>0.5001f||std::abs(jitter.y)>0.5001f)
        return Error{ErrorCode::InvalidInput,"Prepared SR frame identity or jitter is invalid"};
    const std::array<ID3D11Texture2D*,4> textures{
        frame.color(),frame.motion(),frame.depth(),frame.output()};
    std::array<D3D11_TEXTURE2D_DESC,4> desc{};
    std::array<ComPtr<IUnknown>,4> identities{};
    for(std::size_t i=0;i<textures.size();++i) {
        if(!textures[i])return Error{ErrorCode::InvalidInput,"Prepared SR texture is missing"};
        ComPtr<ID3D11Device> owner;
        textures[i]->GetDevice(&owner);
        if(!owner||identity(owner.Get()).Get()!=identity(device).Get())
            return Error{ErrorCode::Conflict,"Prepared SR texture device differs"};
        identities[i]=identity(textures[i]);
        textures[i]->GetDesc(&desc[i]);
        if(!desc[i].Width||!desc[i].Height||desc[i].Width>8192||
           desc[i].Height>8192||desc[i].MipLevels!=1||
           desc[i].ArraySize!=1||desc[i].SampleDesc.Count!=1||
           desc[i].Usage!=D3D11_USAGE_DEFAULT)
            return Error{ErrorCode::Unsupported,"Prepared SR texture geometry differs"};
        for(std::size_t j=0;j<i;++j)if(identities[i]==identities[j])
            return Error{ErrorCode::Conflict,"Prepared SR textures alias"};
    }
    if(desc[0].Format!=DXGI_FORMAT_R8G8B8A8_UNORM||
       desc[1].Format!=DXGI_FORMAT_R16G16_FLOAT||
       (desc[2].Format!=DXGI_FORMAT_R24G8_TYPELESS&&
        desc[2].Format!=DXGI_FORMAT_R32_FLOAT)||
       desc[3].Format!=DXGI_FORMAT_R8G8B8A8_UNORM||
       !(desc[0].BindFlags&D3D11_BIND_SHADER_RESOURCE)||
       !(desc[1].BindFlags&D3D11_BIND_SHADER_RESOURCE)||
       !(desc[2].BindFlags&D3D11_BIND_SHADER_RESOURCE)||
       !(desc[3].BindFlags&D3D11_BIND_UNORDERED_ACCESS))
        return Error{ErrorCode::Unsupported,"Prepared SR texture formats or binds differ"};
    if(desc[0].Width!=frame.width()||desc[0].Height!=frame.height()||
       desc[1].Width!=frame.width()||desc[1].Height!=frame.height()||
       desc[2].Width!=frame.width()||desc[2].Height!=frame.height()||
       desc[3].Width!=frame.outputWidth()||
       desc[3].Height!=frame.outputHeight()||
       frame.outputWidth()<frame.width()||frame.outputHeight()<frame.height()||
       (frame.outputWidth()==frame.width()&&frame.outputHeight()==frame.height())||
       frame.sourceRegion().width!=frame.width()||
       frame.sourceRegion().height!=frame.height())
        return Error{ErrorCode::InvalidInput,"Prepared SR extents or source region differ"};
    return true;
}
}
Result<bool> SdrDlssPresenter::configureQuality(UpscaleQuality quality) noexcept {
    if(ngxStartAttempted_||initialized_||preparedGeneration_)
        return Error{ErrorCode::Conflict,"Cannot change NVIDIA quality during an active feature"};
    switch(quality) {
    case UpscaleQuality::NativeAA:
    case UpscaleQuality::Quality:
    case UpscaleQuality::Balanced:
    case UpscaleQuality::Performance:
    case UpscaleQuality::UltraPerformance:break;
    default:return Error{ErrorCode::InvalidInput,"Unknown NVIDIA quality value"};
    }
    quality_=quality;
    return true;
}

Result<bool> SdrDlssPresenter::beginSession(ID3D11Device* device,
    ID3D11DeviceContext* context,bool reduced) {
    if(!device||!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)
        return Error{ErrorCode::InvalidInput,"NVIDIA session needs a D3D11 device and immediate context"};
    ComPtr<ID3D11Device> owner;context->GetDevice(&owner);
    if(!owner||identity(owner.Get()).Get()!=identity(device).Get())
        return Error{ErrorCode::Conflict,"NVIDIA session context device differs"};
    if(ngxStartAttempted_)
        return Error{ErrorCode::Conflict,"NVIDIA SR initialization already attempted; retire before retry"};
    ngxStartAttempted_=true;
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
    ngxInitSucceeded_=true;
    device_=device;
    if(!success(NVSDK_NGX_D3D11_GetCapabilityParameters(&parameters_))||!parameters_)
        return Error{ErrorCode::Unavailable,"NVIDIA NGX SDR parameters unavailable"};
    int available{};
    if(!success(parameters_->Get(NVSDK_NGX_Parameter_SuperSampling_Available,&available))||!available)
        return Error{ErrorCode::Unsupported,"NVIDIA DLSS unavailable for SDR session"};
    return true;
}
Result<Extent> SdrDlssPresenter::prepareReducedPlan(ID3D11Device* device,
    ID3D11DeviceContext* context,Extent display) {
    if(!display.valid()||display.width>8192||display.height>8192||
       quality_==UpscaleQuality::NativeAA||initialized_||preparedPlan_)
        return Error{ErrorCode::InvalidInput,"Invalid reduced NGX plan request"};
    if(const auto begun=beginSession(device,context,true);
       const auto error=std::get_if<Error>(&begun))return *error;
    const auto quality=ngxQuality(quality_);
    if(!quality)return Error{ErrorCode::InvalidInput,"Unknown NVIDIA quality"};
    UINT width{},height{},maximumWidth{},maximumHeight{},minimumWidth{},minimumHeight{};
    float sharpness{};
    if(!success(NGX_DLSS_GET_OPTIMAL_SETTINGS(parameters_,display.width,display.height,
        *quality,&width,&height,&maximumWidth,&maximumHeight,
        &minimumWidth,&minimumHeight,&sharpness)))
        return Error{ErrorCode::Unavailable,"NVIDIA optimal render size query failed"};
    if(!width||!height||width>display.width||height>display.height||
       (width==display.width&&height==display.height)||
       !minimumWidth||!minimumHeight||minimumWidth>width||minimumHeight>height||
       width>maximumWidth||height>maximumHeight)
        return Error{ErrorCode::Unsupported,"NVIDIA optimal render size is invalid"};
    preparedPlan_=RenderSizePlan{display,{width,height},true};
    return Extent{width,height};
}
Result<bool> SdrDlssPresenter::createReducedFeature(ID3D11Device* device,
    ID3D11DeviceContext* context) {
    if(!preparedPlan_)
        return Error{ErrorCode::Conflict,"Reduced NGX plan must be prepared before feature creation"};
    if(initialized_||preparedGeneration_)
        return Error{ErrorCode::Conflict,"Reduced NGX feature is already active"};
    return initialize(device,context,preparedPlan_->render.width,
        preparedPlan_->render.height,preparedPlan_->display.width,
        preparedPlan_->display.height,true);
}
Result<bool> SdrDlssPresenter::initialize(ID3D11Device* device,
    ID3D11DeviceContext* context,UINT width,UINT height,
    UINT displayWidth,UINT displayHeight,bool reduced) {
    if(reduced&&quality_==UpscaleQuality::NativeAA)
        return Error{ErrorCode::InvalidInput,"NativeAA cannot create a reduced DLSS feature"};
    if(preparedPlan_&&(!reduced||width!=preparedPlan_->render.width||
       height!=preparedPlan_->render.height||
       displayWidth!=preparedPlan_->display.width||
       displayHeight!=preparedPlan_->display.height))
        return Error{ErrorCode::Conflict,"NVIDIA feature extents differ from prepared render plan"};
    if(!ngxStartAttempted_) {
        if(const auto begun=beginSession(device,context,reduced);
           const auto error=std::get_if<Error>(&begun))return *error;
    } else if(!ngxInitSucceeded_||!parameters_||!device_||
              identity(device_.Get()).Get()!=identity(device).Get())
        return Error{ErrorCode::Conflict,"Prepared NVIDIA session device differs or is unavailable"};
    auto isolated=D3D11StateScope::begin(context);
    if(const auto error=std::get_if<Error>(&isolated))return *error;
    auto scope=std::move(std::get<std::unique_ptr<D3D11StateScope>>(isolated));
    NVSDK_NGX_DLSS_Create_Params create{};
    create.Feature.InWidth=width;create.Feature.InHeight=height;
    create.Feature.InTargetWidth=displayWidth;
    create.Feature.InTargetHeight=displayHeight;
    const auto quality=ngxQuality(reduced?quality_:UpscaleQuality::NativeAA);
    if(!quality)return Error{ErrorCode::InvalidInput,"Unknown NVIDIA quality"};
    create.Feature.InPerfQualityValue=*quality;
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
    width_=width;height_=height;
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
std::size_t SdrDlssPresenter::retainedPreparedFrames() const noexcept {
    std::size_t count=retiredPrepared_.size()+unfencedPrepared_.size();
    for(const auto& slot:preparedSlots_)count+=slot.frame.has_value();
    return count;
}
void SdrDlssPresenter::retainUnsubmitted(ID3D11DeviceContext* context,
    PreparedSrInputs frame) {
    ComPtr<ID3D11Device> device;
    if(context)context->GetDevice(&device);
    ComPtr<ID3D11Query> completion;
    const D3D11_QUERY_DESC query{D3D11_QUERY_EVENT,0};
    if(context&&context->GetType()==D3D11_DEVICE_CONTEXT_IMMEDIATE&&
       device&&SUCCEEDED(device->CreateQuery(&query,&completion))) {
        context->End(completion.Get());
        retiredPrepared_.push_back({std::move(frame),std::move(completion)});
    } else unfencedPrepared_.push_back(std::move(frame));
}
Result<bool> SdrDlssPresenter::retirePrepared(ID3D11DeviceContext* context) {
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)
        return Error{ErrorCode::InvalidInput,"Prepared SR retirement needs an immediate context"};
    bool pending=false;
    for(auto& slot:preparedSlots_)if(slot.inFlight) {
        const auto done=context->GetData(slot.completion.Get(),nullptr,0,
            D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(done==S_FALSE)pending=true;
        else if(FAILED(done))return Error{ErrorCode::DeviceRemoved,"Prepared SR slot retirement failed"};
        else {
            slot.inFlight=slot.evaluated=slot.published=false;
        }
    }
    for(auto it=retiredPrepared_.begin();it!=retiredPrepared_.end();) {
        const auto done=context->GetData(it->completion.Get(),nullptr,0,
            D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(done==S_FALSE){pending=true;++it;}
        else if(FAILED(done))return Error{ErrorCode::DeviceRemoved,"Prepared SR discarded frame retirement failed"};
        else it=retiredPrepared_.erase(it);
    }
    if(!unfencedPrepared_.empty())
        return Error{ErrorCode::Unavailable,"Prepared SR work has no completion fence; resources retained"};
    return !pending;
}
Result<bool> SdrDlssPresenter::submitPreparedNgx(ID3D11DeviceContext* context,
    const PreparedSrInputs& prepared,NgxJitter jitter,bool reset) {
    if(!initialized_||!feature_||!parameters_)
        return Error{ErrorCode::Unavailable,"Prepared SR NGX feature is unavailable"};
    NVSDK_NGX_D3D11_DLSS_Eval_Params eval{};
    eval.Feature.pInColor=prepared.color();
    eval.Feature.pInOutput=prepared.output();
    eval.pInDepth=prepared.depth();
    eval.pInMotionVectors=prepared.motion();
    eval.InRenderSubrectDimensions={prepared.width(),prepared.height()};
    eval.InReset=reset?1:0;
    eval.InJitterOffsetX=jitter.x;
    eval.InJitterOffsetY=jitter.y;
    eval.InMVScaleX=static_cast<float>(prepared.width());
    eval.InMVScaleY=static_cast<float>(prepared.height());
    eval.InPreExposure=eval.InExposureScale=1.0f;
    if(!success(NGX_D3D11_EVALUATE_DLSS_EXT(context,feature_,parameters_,&eval)))
        return Error{ErrorCode::Unavailable,"Prepared NVIDIA SDR SR evaluation failed"};
    return true;
}
Result<std::optional<SrEvaluationToken>> SdrDlssPresenter::evaluatePrepared(
    ID3D11Device* device,ID3D11DeviceContext* context,
    PreparedSrInputs prepared,SrFrameMetadata metadata,NgxJitter jitter) {
    const auto reject=[&](Error error)->Result<std::optional<SrEvaluationToken>> {
        retainUnsubmitted(context,std::move(prepared));
        return error;
    };
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||!device)
        return reject({ErrorCode::InvalidInput,"Prepared SR needs a device and immediate context"});
    ComPtr<ID3D11Device> contextDevice;
    context->GetDevice(&contextDevice);
    if(!contextDevice||identity(contextDevice.Get()).Get()!=identity(device).Get())
        return reject({ErrorCode::Conflict,"Prepared SR context device differs"});
    const auto retired=retirePrepared(context);
    if(const auto error=std::get_if<Error>(&retired))return reject(*error);
    auto& slot=preparedSlots_[nextPreparedSlot_++%preparedSlots_.size()];
    if(slot.inFlight) {
        retainUnsubmitted(context,std::move(prepared));
        return std::optional<SrEvaluationToken>{};
    }
    slot.frame.emplace(std::move(prepared));
    return evaluatePreparedSlot(device,context,slot,metadata,jitter);
}

Result<std::optional<SrEvaluationToken>> SdrDlssPresenter::evaluateOwnedScene(
    ID3D11Device* device,ID3D11DeviceContext* context,
    std::span<ID3D11Texture2D* const> sources,UINT outputWidth,UINT outputHeight,
    SrFrameMetadata metadata,NgxJitter jitter) {
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||!device)
        return Error{ErrorCode::InvalidInput,"Owned SR needs a device and immediate context"};
    ComPtr<ID3D11Device> contextDevice;
    context->GetDevice(&contextDevice);
    if(!contextDevice||identity(contextDevice.Get()).Get()!=identity(device).Get())
        return Error{ErrorCode::Conflict,"Owned SR context device differs"};
    const auto retired=retirePrepared(context);
    if(const auto error=std::get_if<Error>(&retired))return *error;
    auto& slot=preparedSlots_[nextPreparedSlot_++%preparedSlots_.size()];
    if(slot.inFlight)return std::optional<SrEvaluationToken>{};
    if(slot.frame) {
        const auto refreshed=slot.frame->refreshOwnedScene(context,sources);
        if(const auto error=std::get_if<Error>(&refreshed))return *error;
    } else {
        auto prepared=prepareSdrSrInputsFromOwnedScene(context,sources,
            outputWidth,outputHeight);
        if(const auto error=std::get_if<Error>(&prepared))return *error;
        slot.frame.emplace(std::move(std::get<PreparedSrInputs>(prepared)));
    }
    return evaluatePreparedSlot(device,context,slot,metadata,jitter);
}

Result<std::optional<SrEvaluationToken>> SdrDlssPresenter::evaluatePreparedSlot(
    ID3D11Device* device,ID3D11DeviceContext* context,PreparedSlot& slot,
    SrFrameMetadata metadata,NgxJitter jitter) {
    if(!slot.frame)
        return Error{ErrorCode::Conflict,"Prepared SR slot has no frame"};
    if(const auto checked=validatePrepared(device,*slot.frame,metadata,jitter);
       const auto error=std::get_if<Error>(&checked))return *error;
    if(preparedGeneration_&&
       (metadata.generation!=preparedGeneration_||
        metadata.frameId<=lastPreparedFrameId_||
        slot.frame->width()!=width_||slot.frame->height()!=height_||
        slot.frame->outputWidth()!=displayWidth_||
        slot.frame->outputHeight()!=displayHeight_))
        return Error{ErrorCode::Conflict,"Prepared SR frame generation, order or extent changed"};
    if(initialized_&&(!reduced_||identity(device_.Get()).Get()!=identity(device).Get()))
        return Error{ErrorCode::Conflict,"Existing DLSS feature mode or device differs"};
    if(!slot.completion) {
        const D3D11_QUERY_DESC query{D3D11_QUERY_EVENT,0};
        if(FAILED(device->CreateQuery(&query,&slot.completion)))
            return Error{ErrorCode::Unavailable,"Prepared SR completion query unavailable"};
    }
    slot.metadata=metadata;
    slot.evaluated=slot.published=false;
    const bool reset=metadata.resetHistory||resetPending_||!lastPreparedFrameId_;
    Result<bool> evaluated=Error{ErrorCode::Unavailable,"Prepared SR state isolation unavailable"};
    Result<bool> completed=Error{ErrorCode::Unavailable,"Prepared SR GPU completion unavailable"};
    bool completionIssued=false;
    {
        auto isolated=D3D11StateScope::begin(context);
        if(const auto error=std::get_if<Error>(&isolated))evaluated=*error;
        else {
            auto scope=std::move(std::get<std::unique_ptr<D3D11StateScope>>(isolated));
            if(preparedEvaluator_){
                if(!preparedGeneration_) {
                    width_=slot.frame->width();height_=slot.frame->height();
                    displayWidth_=slot.frame->outputWidth();
                    displayHeight_=slot.frame->outputHeight();reduced_=true;
                }
                evaluated=preparedEvaluator_(context,*slot.frame,metadata,jitter,reset);
            } else {
                if(!initialized_)evaluated=initialize(device,context,
                    slot.frame->width(),slot.frame->height(),
                    slot.frame->outputWidth(),slot.frame->outputHeight(),true);
                else evaluated=true;
                if(std::holds_alternative<bool>(evaluated)&&std::get<bool>(evaluated))
                    evaluated=submitPreparedNgx(context,*slot.frame,jitter,reset);
            }
            context->End(slot.completion.Get());
            completionIssued=true;
            completed=waitForGpuEvent(device,context,slot.completion.Get());
        }
    }
    if(!completionIssued) {
        context->End(slot.completion.Get());
        completed=waitForGpuEvent(device,context,slot.completion.Get());
    }
    slot.inFlight=true;
    preparedGeneration_=metadata.generation;
    lastPreparedFrameId_=metadata.frameId;
    if(const auto error=std::get_if<Error>(&completed))return *error;
    if(const auto error=std::get_if<Error>(&evaluated))return *error;
    if(!std::get<bool>(evaluated))return std::optional<SrEvaluationToken>{};
    slot.evaluated=true;
    resetPending_=false;
    ++submittedFrames_;
    return std::optional<SrEvaluationToken>{SrEvaluationToken{
        metadata.frameId,metadata.generation,
        static_cast<unsigned>(&slot-preparedSlots_.data())}};
}
Result<bool> SdrDlssPresenter::publishEvaluated(ID3D11DeviceContext* context,
    SrEvaluationToken token,ID3D11Texture2D* destination) {
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE||
       !destination||token.slot>=preparedSlots_.size())
        return Error{ErrorCode::InvalidInput,"Prepared SR publication arguments are invalid"};
    auto& slot=preparedSlots_[token.slot];
    if(!slot.frame||!slot.evaluated||slot.published||
       slot.metadata.frameId!=token.frameId||
       slot.metadata.generation!=token.generation||
       token.frameId!=lastPreparedFrameId_||
       token.generation!=preparedGeneration_)
        return Error{ErrorCode::Conflict,"Prepared SR publication token is stale"};
    D3D11_TEXTURE2D_DESC dest{};destination->GetDesc(&dest);
    if(dest.Width!=slot.frame->outputWidth()||
       dest.Height!=slot.frame->outputHeight()||
       dest.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||
       !(dest.BindFlags&D3D11_BIND_RENDER_TARGET))
        return Error{ErrorCode::Unsupported,"Prepared SR destination extent or format differs"};
    ComPtr<ID3D11Device> owner,contextDevice;
    destination->GetDevice(&owner);context->GetDevice(&contextDevice);
    if(!owner||!contextDevice||
       identity(owner.Get()).Get()!=identity(contextDevice.Get()).Get())
        return Error{ErrorCode::Conflict,"Prepared SR destination device differs"};
    ComPtr<ID3D11RenderTargetView> activeView;
    ComPtr<ID3D11Resource> activeTarget;
    context->OMGetRenderTargets(1,activeView.GetAddressOf(),nullptr);
    if(activeView)activeView->GetResource(&activeTarget);
    if(!activeTarget||identity(activeTarget.Get()).Get()!=identity(destination).Get())
        return Error{ErrorCode::Conflict,"Prepared SR publication target is not active RTV0"};
    const auto copied=copySdrDisplayFrame(context,destination,slot.frame->output());
    context->End(slot.completion.Get());
    slot.inFlight=true;
    if(const auto error=std::get_if<Error>(&copied))return *error;
    slot.published=true;
    return true;
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
    if(!context||context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)
        return Error{ErrorCode::InvalidInput,"SDR DLAA shutdown needs immediate context"};
    if(const auto retired=retirePrepared(context);
       const auto error=std::get_if<Error>(&retired))return *error;
    else if(!std::get<bool>(retired))return false;
    Microsoft::WRL::ComPtr<ID3D11Device> owner;
    context->GetDevice(&owner);
    if(!owner||(device_&&identity(owner.Get()).Get()!=identity(device_.Get()).Get()))
        return Error{ErrorCode::Conflict,"SDR DLAA shutdown device differs"};
    for(auto& slot:slots_)if(slot.inFlight) {
        const auto done=context->GetData(slot.completion.Get(),nullptr,0,
            D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(done==S_FALSE)return false;
        if(FAILED(done)||FAILED(device_->GetDeviceRemovedReason()))
            return Error{ErrorCode::DeviceRemoved,"SDR DLAA shutdown fence failed"};
        slot.inFlight=false;
    }
    if(feature_||parameters_||ngxInitSucceeded_) {
        auto isolated=D3D11StateScope::begin(context);
        if(const auto error=std::get_if<Error>(&isolated))return *error;
        auto scope=std::move(std::get<std::unique_ptr<D3D11StateScope>>(isolated));
        if(feature_) {
            if(!success(NVSDK_NGX_D3D11_ReleaseFeature(feature_)))
                return Error{ErrorCode::Unavailable,"SDR DLSS feature release failed"};
            feature_=nullptr;
        }
        if(parameters_) {
            if(!success(NVSDK_NGX_D3D11_DestroyParameters(parameters_)))
                return Error{ErrorCode::Unavailable,"SDR DLSS parameter destruction failed"};
            parameters_=nullptr;
        }
        if(ngxInitSucceeded_) {
            if(!success(NVSDK_NGX_D3D11_Shutdown1(device_.Get())))
                return Error{ErrorCode::Unavailable,"SDR DLSS NGX shutdown failed"};
            ngxInitSucceeded_=false;
        }
    }
    for(auto& slot:slots_) {
        slot.frame.reset();slot.completion.Reset();slot.inFlight=false;
    }
    for(auto& slot:preparedSlots_) {
        slot.frame.reset();slot.completion.Reset();
        slot.inFlight=slot.evaluated=slot.published=false;
    }
    retiredPrepared_.clear();
    unfencedPrepared_.clear();
    device_.Reset();
    if(runtimeFile_!=INVALID_HANDLE_VALUE)CloseHandle(runtimeFile_);
    runtimeFile_=INVALID_HANDLE_VALUE;
    width_=height_=displayWidth_=displayHeight_=0;
    preparedGeneration_=lastPreparedFrameId_=0;
    reduced_=false;initialized_=false;ngxStartAttempted_=false;
    preparedPlan_.reset();
    return true;
}
}
