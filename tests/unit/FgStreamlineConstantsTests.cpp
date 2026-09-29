#include <catch2/catch_test_macros.hpp>
#include "rk/FgStreamlineConstants.hpp"
#include <limits>
#include <variant>

namespace {
std::array<float,16> identity() {
    std::array<float,16> value{};
    for(unsigned i=0;i<4;++i)value[i*4+i]=1.0f;
    return value;
}
rk::FgSourceFrame frame() {
    rk::FgSourceFrame value{};
    value.source=27;value.generation=3;value.presentToken=1004;
    value.resetEpoch=2;value.cameraValid=true;
    value.render={1485,835};value.display={2560,1440};
    return value;
}
rk::FgCameraData camera() {
    rk::FgCameraData value{};
    value.source=27;value.generation=3;value.presentToken=1004;
    value.resetEpoch=2;value.sampleRevision=9546;
    value.viewToClip=identity();value.viewToClip[1]=2.0f;
    value.clipToView=identity();value.clipToPrevClip=identity();
    value.clipToPrevClip[12]=3.0f;
    value.prevClipToClip=identity();value.prevClipToClip[12]=-3.0f;
    value.position={4,5,6};value.right={1,0,0};
    value.up={0,1,0};value.forward={0,0,-1};
    value.jitter={0.25f,-0.125f};value.mvecScale={1,1};
    value.nearPlane=15;value.farPlane=1000;
    value.fovRadians=1.5f;value.aspectRatio=16.0f/9.0f;
    value.cameraMotionIncluded=true;value.reset=true;
    return value;
}
}

TEST_CASE("FG Streamline constants preserve stamped camera and measured guide units",
    "[fg_streamline_constants]") {
    const auto result=rk::makeFgStreamlineConstants(frame(),camera());
    REQUIRE(std::holds_alternative<sl::Constants>(result));
    const auto& values=std::get<sl::Constants>(result);
    REQUIRE(values.cameraViewToClip[0].y==2.0f);
    REQUIRE(values.clipToPrevClip[3].x==3.0f);
    REQUIRE(values.prevClipToClip[3].x==-3.0f);
    REQUIRE(values.jitterOffset.x==0.25f);
    REQUIRE(values.jitterOffset.y==-0.125f);
    REQUIRE(values.mvecScale.x==1.0f);
    REQUIRE(values.mvecScale.y==1.0f);
    REQUIRE(values.cameraPos.x==4.0f);
    REQUIRE(values.cameraFwd.z==-1.0f);
    REQUIRE(values.cameraNear==15.0f);
    REQUIRE(values.cameraFar==1000.0f);
    REQUIRE(values.cameraFOV==1.5f);
    REQUIRE(values.depthInverted==sl::Boolean::eFalse);
    REQUIRE(values.cameraMotionIncluded==sl::Boolean::eTrue);
    REQUIRE(values.motionVectors3D==sl::Boolean::eFalse);
    REQUIRE(values.reset==sl::Boolean::eTrue);
}

TEST_CASE("FG Streamline constants reject stale token and nonfinite camera",
    "[fg_streamline_constants]") {
    auto stale=camera();stale.presentToken++;
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::makeFgStreamlineConstants(frame(),stale)));
    auto invalid=camera();
    invalid.clipToPrevClip[4]=std::numeric_limits<float>::quiet_NaN();
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::makeFgStreamlineConstants(frame(),invalid)));
}
