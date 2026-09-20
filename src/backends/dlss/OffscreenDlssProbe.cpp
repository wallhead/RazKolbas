#include "rk/OffscreenDlssProbe.hpp"
#include "rk/D3D11StateScope.hpp"
#include "rk/PatchDescriptor.hpp"
#include <nvsdk_ngx_helpers_d3d.h>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>

namespace rk {
namespace {
namespace fs=std::filesystem;
constexpr std::string_view runtimeHash="c85f971ce023c9f3492fc7455f0b01a24ba18ea39636407a846902c4360b0b7e";
Result<bool> exactRuntime(const fs::path& path) {
    if(!fs::is_regular_file(path)||fs::file_size(path)!=58956400)
        return Error{ErrorCode::Unavailable,"Pinned NVIDIA SR runtime missing or wrong size"};
    std::ifstream file(path,std::ios::binary);
    if(!file)return Error{ErrorCode::Io,"Cannot read pinned NVIDIA SR runtime"};
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fs::file_size(path)));
    if(!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
        return Error{ErrorCode::Io,"Cannot hash pinned NVIDIA SR runtime"};
    if(sha256(bytes)!=runtimeHash)
        return Error{ErrorCode::Conflict,"Pinned NVIDIA SR runtime hash differs"};
    return true;
}
fs::path pluginDirectory() {
    HMODULE self{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&pluginDirectory),&self))return {};
    std::array<wchar_t,32768> buffer{};
    const auto length=GetModuleFileNameW(self,buffer.data(),static_cast<DWORD>(buffer.size()));
    if(!length||length>=buffer.size())return {};
    return fs::path(buffer.data()).parent_path();
}
bool success(NVSDK_NGX_Result result) { return result==NVSDK_NGX_Result_Success; }
}

Result<bool> OffscreenDlssProbe::begin(ID3D11Device* device,ID3D11DeviceContext* context,
    PreparedSrInputs& inputs) {
    if(!device||!context||initialized_||pending_||feature_||parameters_)
        return Error{ErrorCode::InvalidInput,"DLSS probe has invalid lifecycle or device"};
    if(!inputs.color()||!inputs.motion()||!inputs.depth()||!inputs.output()||
        !inputs.width()||!inputs.height())
        return Error{ErrorCode::InvalidInput,"DLSS probe inputs are incomplete"};
    const auto folder=pluginDirectory();
    if(folder.empty())return Error{ErrorCode::Unavailable,"Cannot locate RazKolbas plugin directory"};
    const auto runtimeDir=folder/L"RazKolbasRuntime";
    const auto runtimePath=runtimeDir/L"nvngx_dlss.dll";
    runtimeFile_=CreateFileW(runtimePath.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,
        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(runtimeFile_==INVALID_HANDLE_VALUE)
        return Error{ErrorCode::Unavailable,"Pinned NVIDIA SR runtime is not installed"};
    if(const auto checked=exactRuntime(runtimePath);const auto error=std::get_if<Error>(&checked))return *error;
    if(const auto existing=GetModuleHandleW(L"nvngx_dlss.dll")) {
        wchar_t existingName[32768]{};
        const auto length=GetModuleFileNameW(existing,existingName,32768);
        if(!length||length>=32768)
            return Error{ErrorCode::Conflict,"Cannot identify preloaded NVIDIA SR runtime"};
        if(const auto checked=exactRuntime(existingName);const auto error=std::get_if<Error>(&checked))
            return Error{ErrorCode::Conflict,"Preloaded NVIDIA SR runtime differs from pinned file"};
    }
    const auto dataPath=fs::temp_directory_path()/L"RazKolbasNgxData";
    fs::create_directories(dataPath);
    auto isolated=D3D11StateScope::begin(context);
    if(const auto error=std::get_if<Error>(&isolated))return *error;
    auto scope=std::move(std::get<std::unique_ptr<D3D11StateScope>>(isolated));
    const wchar_t* pathList[]={runtimeDir.c_str()};
    NVSDK_NGX_FeatureCommonInfo info{};
    info.PathListInfo={pathList,1};
    if(!success(NVSDK_NGX_D3D11_Init_with_ProjectID("b3340e44-a57e-4b98-9318-d7150829d110",
        NVSDK_NGX_ENGINE_TYPE_CUSTOM,"RazKolbas-Live-Probe-1",dataPath.c_str(),device,&info)))
        return Error{ErrorCode::Unavailable,"NVIDIA NGX D3D11 init failed"};
    initialized_=true;
    if(!success(NVSDK_NGX_D3D11_GetCapabilityParameters(&parameters_))||!parameters_)
        return Error{ErrorCode::Unavailable,"NVIDIA NGX capability parameters unavailable"};
    int available=0;
    if(!success(parameters_->Get(NVSDK_NGX_Parameter_SuperSampling_Available,&available))||!available)
        return Error{ErrorCode::Unsupported,"NVIDIA DLSS SR unavailable on game device"};
    NVSDK_NGX_DLSS_Create_Params create{};
    create.Feature.InWidth=create.Feature.InTargetWidth=inputs.width();
    create.Feature.InHeight=create.Feature.InTargetHeight=inputs.height();
    create.Feature.InPerfQualityValue=NVSDK_NGX_PerfQuality_Value_DLAA;
    create.InFeatureCreateFlags=NVSDK_NGX_DLSS_Feature_Flags_IsHDR|
        NVSDK_NGX_DLSS_Feature_Flags_MVLowRes|NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;
    if(!success(NGX_D3D11_CREATE_DLSS_EXT(context,&feature_,parameters_,&create))||!feature_)
        return Error{ErrorCode::Unavailable,"NVIDIA DLAA feature creation failed"};
    wchar_t loadedName[32768]{};
    const auto loaded=GetModuleHandleW(L"nvngx_dlss.dll");
    const auto length=loaded?GetModuleFileNameW(loaded,loadedName,32768):0;
    if(!length||length>=32768)
        return Error{ErrorCode::Conflict,"Cannot identify loaded NVIDIA SR runtime"};
    if(const auto checked=exactRuntime(loadedName);const auto error=std::get_if<Error>(&checked))return *error;
    D3D11_TEXTURE2D_DESC output{};inputs.output()->GetDesc(&output);
    output.Usage=D3D11_USAGE_STAGING;output.BindFlags=0;output.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    output.MiscFlags=0;
    if(FAILED(device->CreateTexture2D(&output,nullptr,&readback_)))
        return Error{ErrorCode::Unavailable,"Cannot create DLSS output readback"};
    const D3D11_QUERY_DESC query{D3D11_QUERY_EVENT,0};
    if(FAILED(device->CreateQuery(&query,&completion_)))
        return Error{ErrorCode::Unavailable,"Cannot create DLSS completion query"};
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> outputUav;
    if(FAILED(device->CreateUnorderedAccessView(inputs.output(),nullptr,&outputUav)))
        return Error{ErrorCode::Unavailable,"Cannot create DLSS output sentinel UAV"};
    constexpr float nan=std::numeric_limits<float>::quiet_NaN();
    const float sentinel[4]{nan,nan,nan,nan};
    context->ClearUnorderedAccessViewFloat(outputUav.Get(),sentinel);
    NVSDK_NGX_D3D11_DLSS_Eval_Params eval{};
    eval.Feature.pInColor=inputs.color();eval.Feature.pInOutput=inputs.output();
    eval.pInDepth=inputs.depth();eval.pInMotionVectors=inputs.motion();
    eval.InRenderSubrectDimensions={inputs.width(),inputs.height()};
    // Reset-only diagnostic. Live jitter and guide units still need recovery.
    eval.InReset=1;eval.InJitterOffsetX=eval.InJitterOffsetY=0;
    eval.InMVScaleX=static_cast<float>(inputs.width());
    eval.InMVScaleY=static_cast<float>(inputs.height());
    eval.InPreExposure=eval.InExposureScale=1.0f;
    if(!success(NGX_D3D11_EVALUATE_DLSS_EXT(context,feature_,parameters_,&eval)))
        return Error{ErrorCode::Unavailable,"NVIDIA DLAA evaluation failed"};
    scope.reset(); // Restore Skyrim's complete context state before returning.
    context->CopyResource(readback_.Get(),inputs.output());
    context->End(completion_.Get());context->Flush();
    width_=inputs.width();height_=inputs.height();pending_=true;
    return true;
}

Result<std::string> OffscreenDlssProbe::poll(ID3D11Device* device,ID3D11DeviceContext* context) {
    if(!pending_||!device||!context)return Error{ErrorCode::InvalidInput,"No pending DLSS probe"};
    const auto status=context->GetData(completion_.Get(),nullptr,0,D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if(status==S_FALSE)return std::string{};
    if(FAILED(status)||FAILED(device->GetDeviceRemovedReason()))
        return Error{ErrorCode::DeviceRemoved,"DLSS probe completion or device failed; resources retained"};
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(width_)*height_*8);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(readback_.Get(),0,D3D11_MAP_READ,0,&mapped)))
        return Error{ErrorCode::Unavailable,"DLSS output readback failed; resources retained"};
    for(unsigned y=0;y<height_;++y)
        std::memcpy(bytes.data()+static_cast<std::size_t>(y)*width_*8,
            static_cast<const std::uint8_t*>(mapped.pData)+static_cast<std::size_t>(y)*mapped.RowPitch,
            static_cast<std::size_t>(width_)*8);
    context->Unmap(readback_.Get(),0);
    std::uint16_t first{};bool varying=false;std::size_t finite=0;
    for(std::size_t pixel=0;pixel<static_cast<std::size_t>(width_)*height_;++pixel) {
        for(unsigned channel=0;channel<3;++channel) {
            std::uint16_t half{};
            std::memcpy(&half,bytes.data()+pixel*8+channel*2,2);
            if((half&0x7c00U)!=0x7c00U)++finite;
            if(pixel||channel) varying|=half!=first;else first=half;
        }
    }
    const auto hash=sha256(bytes);
    if(finite!=static_cast<std::size_t>(width_)*height_*3||!varying)
        return Error{ErrorCode::Unavailable,"DLSS output is nonfinite or uniform; resources retained"};
    auto isolated=D3D11StateScope::begin(context);
    if(const auto error=std::get_if<Error>(&isolated))return *error;
    auto scope=std::move(std::get<std::unique_ptr<D3D11StateScope>>(isolated));
    if(!success(NVSDK_NGX_D3D11_ReleaseFeature(feature_)))
        return Error{ErrorCode::Unavailable,"DLSS release failed; resources retained"};
    feature_=nullptr;
    if(!success(NVSDK_NGX_D3D11_DestroyParameters(parameters_)))
        return Error{ErrorCode::Unavailable,"DLSS parameter destruction failed; resources retained"};
    parameters_=nullptr;
    if(!success(NVSDK_NGX_D3D11_Shutdown1(device)))
        return Error{ErrorCode::Unavailable,"DLSS shutdown failed; resources retained"};
    initialized_=false;pending_=false;
    CloseHandle(runtimeFile_);runtimeFile_=INVALID_HANDLE_VALUE;
    readback_.Reset();completion_.Reset();
    return hash;
}
}
