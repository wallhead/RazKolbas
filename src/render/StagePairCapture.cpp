#include "rk/StagePairCapture.hpp"
#include "rk/PatchDescriptor.hpp"
#include <wrl/client.h>
#include <array>
#include <fstream>
#include <utility>

namespace rk {
namespace {
std::uintptr_t identity(ID3D11Texture2D* texture) {
    Microsoft::WRL::ComPtr<IUnknown> canonical;
    return texture&&SUCCEEDED(texture->QueryInterface(IID_PPV_ARGS(&canonical)))?
        reinterpret_cast<std::uintptr_t>(canonical.Get()):0;
}
constexpr std::size_t captureBudget=64*1024*1024;
}
Result<StagePairCapture> StagePairCapture::capturePostWorld(ID3D11DeviceContext* context,
    ID3D11Texture2D* scene,ID3D11Texture2D* backbuffer) {
    if(!context||!scene||!backbuffer||scene==backbuffer)
        return Error{ErrorCode::InvalidInput,"Stage pair requires distinct HDR scene and backbuffer"};
    D3D11_TEXTURE2D_DESC hdr{},back{};
    scene->GetDesc(&hdr);backbuffer->GetDesc(&back);
    if(hdr.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT||
       back.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||
       !hdr.Width||!hdr.Height||hdr.Width!=back.Width||hdr.Height!=back.Height||
       static_cast<std::uint64_t>(hdr.Width)*hdr.Height*16>captureBudget)
        return Error{ErrorCode::Unsupported,"Stage pair HDR/backbuffer formats or total CPU budget differ"};
    const auto backIdentity=identity(backbuffer);
    if(!backIdentity)return Error{ErrorCode::Unavailable,"Cannot identify stage pair backbuffer"};
    const std::array<ID3D11Texture2D*,2> sources{scene,backbuffer};
    auto images=readbackCandidates(context,sources,captureBudget);
    if(const auto error=std::get_if<Error>(&images))return *error;
    auto& pair=std::get<std::vector<ProbeImage>>(images);
    StagePairCapture result;
    result.scene_=std::move(pair[0]);result.postWorld_=std::move(pair[1]);
    result.backbufferIdentity_=backIdentity;
    return result;
}
Result<bool> StagePairCapture::captureBeforePresent(ID3D11DeviceContext* context,
    ID3D11Texture2D* backbuffer) {
    if(complete_||!backbufferIdentity_||!backbuffer)
        return Error{ErrorCode::InvalidInput,"Stage pair is complete or missing its backbuffer"};
    if(identity(backbuffer)!=backbufferIdentity_)
        return Error{ErrorCode::Conflict,"Backbuffer changed between stage observations"};
    const std::array<ID3D11Texture2D*,1> source{backbuffer};
    auto image=readbackCandidates(context,source,captureBudget);
    if(const auto error=std::get_if<Error>(&image))return *error;
    auto& captured=std::get<std::vector<ProbeImage>>(image).front();
    if(captured.descriptor.Width!=postWorld_.descriptor.Width||
       captured.descriptor.Height!=postWorld_.descriptor.Height||
       captured.descriptor.Format!=postWorld_.descriptor.Format)
        return Error{ErrorCode::Conflict,"Backbuffer geometry changed before Present"};
    beforePresent_=std::move(captured);
    complete_=true;
    return true;
}
Result<bool> StagePairCapture::save(const std::filesystem::path& directory) const {
    namespace fs=std::filesystem;
    if(!complete_||directory.empty())
        return Error{ErrorCode::InvalidInput,"Cannot save an incomplete stage pair"};
    try {
        if(fs::exists(directory))return Error{ErrorCode::Conflict,"Stage pair directory already exists"};
        fs::create_directories(directory);
        const std::array images{&scene_,&postWorld_,&beforePresent_};
        constexpr std::array names{"scene-hdr.raw","post-world-backbuffer.raw",
            "before-present-backbuffer.raw"};
        for(std::size_t i=0;i<images.size();++i) {
            std::ofstream output(directory/names[i],std::ios::binary);
            output.exceptions(std::ios::failbit|std::ios::badbit);
            output.write(reinterpret_cast<const char*>(images[i]->pixels.data()),
                static_cast<std::streamsize>(images[i]->pixels.size()));
            output.close();
        }
        std::ofstream manifest(directory/"manifest.pending");
        manifest.exceptions(std::ios::failbit|std::ios::badbit);
        manifest<<"RazKolbas stage pair; same world frame; no display write\n";
        for(std::size_t i=0;i<images.size();++i) {
            const auto& frame=*images[i];
            manifest<<names[i]<<" width="<<frame.descriptor.Width
                <<" height="<<frame.descriptor.Height
                <<" format="<<static_cast<unsigned>(frame.descriptor.Format)
                <<" rowBytes="<<frame.rowBytes<<" bytes="<<frame.pixels.size()
                <<" sha256="<<sha256(frame.pixels)<<"\n";
        }
        manifest<<"complete=true\n";
        manifest.close();
        fs::rename(directory/"manifest.pending",directory/"manifest.txt");
        return true;
    } catch(const std::exception& error) {
        return Error{ErrorCode::Io,std::string("Cannot save stage pair: ")+error.what()};
    }
}
}
