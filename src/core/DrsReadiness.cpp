#include "rk/DrsReadiness.hpp"
#include <cmath>

namespace rk {
std::optional<EngineDrsTarget> engineDrsTarget(std::uint32_t displayWidth,
    std::uint32_t displayHeight,float widthRatio,float heightRatio) noexcept {
    if(!displayWidth||!displayHeight||displayWidth>8192||displayHeight>8192||
       !std::isfinite(widthRatio)||!std::isfinite(heightRatio)||
       widthRatio<=0.0f||heightRatio<=0.0f||
       widthRatio>1.0f||heightRatio>1.0f)return std::nullopt;
    const auto width=static_cast<std::uint32_t>(std::lround(displayWidth*widthRatio));
    const auto height=static_cast<std::uint32_t>(std::lround(displayHeight*heightRatio));
    if(!width||!height)return std::nullopt;
    return EngineDrsTarget{width,height};
}
bool nativeDlaaRatiosReady(float currentWidth,float currentHeight,
    float previousWidth,float previousHeight) noexcept {
    return std::isfinite(currentWidth)&&std::isfinite(currentHeight)&&
        std::isfinite(previousWidth)&&std::isfinite(previousHeight)&&
        currentWidth==1.0f&&currentHeight==1.0f&&
        previousWidth==1.0f&&previousHeight==1.0f;
}
std::optional<std::uint64_t> StableDrsTupleGate::observe(
    std::array<float,4> ratios) noexcept {
    for(const auto ratio:ratios)if(!std::isfinite(ratio)||ratio<=0.0f||ratio>1.0f) {
        hasTuple_=false;consecutive_=0;emitted_=false;
        return std::nullopt;
    }
    bool same=hasTuple_;
    for(std::size_t i=0;i<ratios.size();++i)
        same&=std::abs(ratios[i]-last_[i])<=0.0001f;
    if(!same) {
        last_=ratios;hasTuple_=true;consecutive_=1;emitted_=false;
        ++generation_;
        return std::nullopt;
    }
    if(consecutive_<2)++consecutive_;
    if(consecutive_==2&&!emitted_) {
        emitted_=true;
        return generation_;
    }
    return std::nullopt;
}
bool reducedSdrRegionLooksUnscaled(std::span<const std::uint8_t> rgba,
    std::uint32_t displayWidth,std::uint32_t displayHeight,std::size_t rowBytes,
    std::uint32_t renderWidth,std::uint32_t renderHeight) noexcept {
    if(!displayWidth||!displayHeight||displayWidth>8192||displayHeight>8192||
       !renderWidth||!renderHeight||renderWidth>displayWidth||
       renderHeight>displayHeight||
       (renderWidth==displayWidth&&renderHeight==displayHeight)||
       rowBytes<static_cast<std::size_t>(displayWidth)*4||
       rowBytes>rgba.size()/displayHeight)return false;
    const auto pixel=[&](std::uint32_t x,std::uint32_t y) {
        const auto offset=static_cast<std::size_t>(y)*rowBytes+x*4;
        return std::array<std::uint8_t,3>{rgba[offset],rgba[offset+1],rgba[offset+2]};
    };
    std::array<std::array<std::uint8_t,3>,100> seen{};
    unsigned distinct{};
    for(std::uint32_t y=0;y<10;++y)for(std::uint32_t x=0;x<10;++x) {
        const auto value=pixel(static_cast<std::uint32_t>((2*x+1)*
            static_cast<std::uint64_t>(renderWidth)/20),
            static_cast<std::uint32_t>((2*y+1)*
            static_cast<std::uint64_t>(renderHeight)/20));
        bool known=false;
        for(unsigned i=0;i<distinct;++i)known|=seen[i]==value;
        if(!known)seen[distinct++]=value;
    }
    if(distinct<16)return false;
    std::optional<std::array<std::uint8_t,3>> outside;
    const auto checkOutside=[&](std::uint32_t left,std::uint32_t top,
        std::uint32_t width,std::uint32_t height) {
        for(std::uint32_t y=0;y<10;++y)for(std::uint32_t x=0;x<10;++x) {
            const auto value=pixel(left+static_cast<std::uint32_t>((2*x+1)*
                static_cast<std::uint64_t>(width)/20),
                top+static_cast<std::uint32_t>((2*y+1)*
                static_cast<std::uint64_t>(height)/20));
            if(!outside)outside=value;
            for(unsigned channel=0;channel<3;++channel)
                if(std::abs(static_cast<int>(value[channel])-
                    static_cast<int>((*outside)[channel]))>2)return false;
        }
        return true;
    };
    if(renderWidth<displayWidth&&
       !checkOutside(renderWidth,0,displayWidth-renderWidth,displayHeight))return false;
    if(renderHeight<displayHeight&&
       !checkOutside(0,renderHeight,renderWidth,displayHeight-renderHeight))return false;
    return outside.has_value();
}
}
