#include <catch2/catch_test_macros.hpp>
#include "rk/FgWorldGuideLatch.hpp"
#include <variant>

namespace {
using Matrix=std::array<float,16>;
Matrix multiply(const Matrix& a,const Matrix& b) {
    Matrix out{};
    for(unsigned row=0;row<4;++row)
        for(unsigned column=0;column<4;++column)
            for(unsigned term=0;term<4;++term)
                out[row*4+column]+=a[row*4+term]*b[term*4+column];
    return out;
}
rk::FgGameCameraSample camera() {
    rk::FgGameCameraSample result{};
    result.view={1,0,0,0, 0,1,0,0, 0,0,-1,0, 0,0,0,1};
    result.inverseView=result.view;
    constexpr float nearPlane=15.0f,farPlane=1000.0f;
    const float a=farPlane/(farPlane-nearPlane);
    const float b=-farPlane*nearPlane/(farPlane-nearPlane);
    result.projection={0.5625f,0,0,0, 0,1,0,0, 0,0,a,b, 0,0,1,0};
    result.inverseProjection={1.0f/0.5625f,0,0,0, 0,1,0,0,
        0,0,0,1, 0,0,1.0f/b,-a/b};
    result.currentViewProjection=multiply(result.projection,result.view);
    result.previousViewProjection=result.currentViewProjection;
    result.inverseViewProjection=multiply(result.inverseView,
        result.inverseProjection);
    return result;
}
rk::FgWorldGuideFrame guide() {
    rk::FgWorldGuideFrame result{};
    result.frame.source=7;result.frame.generation=5;
    result.frame.presentToken=21;result.frame.resetEpoch=3;
    result.frame.render={20,12};result.frame.display={32,20};
    result.frame.worldActive=true;
    return result;
}
}

TEST_CASE("FG raw guide packet retains a same-frame camera candidate without readiness",
    "[fg_world_camera]") {
    auto packet=guide();
    const rk::FgCameraProducerSample producer{7,100,2,camera()};
    const auto attached=rk::attachFgWorldCameraCandidate(packet,producer,
        7,5,{0.1f,-0.2f});
    REQUIRE(std::holds_alternative<bool>(attached));
    REQUIRE(packet.cameraCandidate);
    REQUIRE(packet.cameraCandidate->source==7);
    REQUIRE(packet.cameraCandidate->generation==5);
    REQUIRE(packet.cameraCandidate->presentToken==21);
    REQUIRE(packet.cameraCandidate->sampleRevision==100);
    REQUIRE_FALSE(packet.frame.cameraValid);
    REQUIRE_FALSE(packet.frame.depth.ready);
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::attachFgWorldCameraCandidate(packet,producer,7,5,{0,0})));
}

TEST_CASE("FG camera candidate rejects mismatched world and jitter stamps",
    "[fg_world_camera]") {
    const rk::FgCameraProducerSample producer{7,100,2,camera()};
    for(const auto mismatch:{0,1,2,3}) {
        auto packet=guide();
        auto changed=producer;
        std::uint64_t jitterSource=7,jitterGeneration=5;
        if(mismatch==0)changed.source=6;
        if(mismatch==1)jitterSource=6;
        if(mismatch==2)jitterGeneration=4;
        if(mismatch==3)packet.frame.presentToken=0;
        REQUIRE(std::holds_alternative<rk::Error>(
            rk::attachFgWorldCameraCandidate(packet,changed,
                jitterSource,jitterGeneration,{0,0})));
        REQUIRE_FALSE(packet.cameraCandidate);
        REQUIRE_FALSE(packet.frame.cameraValid);
    }
}
