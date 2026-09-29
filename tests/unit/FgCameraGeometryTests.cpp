#include <catch2/catch_test_macros.hpp>
#include "rk/FgCameraGeometry.hpp"
#include <array>
#include <cmath>
#include <limits>
#include <variant>

namespace {
using Matrix=std::array<float,16>;
Matrix identity() {
    Matrix m{};for(unsigned i=0;i<4;++i)m[i*4+i]=1.0f;return m;
}
Matrix multiply(const Matrix& left,const Matrix& right) {
    Matrix out{};
    for(unsigned row=0;row<4;++row)
        for(unsigned column=0;column<4;++column)
            for(unsigned term=0;term<4;++term)
                out[row*4+column]+=left[row*4+term]*right[term*4+column];
    return out;
}
rk::FgGameCameraSample camera() {
    rk::FgGameCameraSample value{};
    value.view=identity();value.projection=identity();
    value.currentViewProjection=identity();
    value.previousViewProjection=identity();
    value.inverseProjection=identity();value.inverseView=identity();
    value.inverseViewProjection=identity();
    return value;
}
rk::FgGameCameraSample leftHandedCamera() {
    auto value=camera();
    value.view[10]=-1.0f;
    value.inverseView=value.view;
    constexpr float nearPlane=15.0f,farPlane=1000.0f;
    const float a=farPlane/(farPlane-nearPlane);
    const float b=-farPlane*nearPlane/(farPlane-nearPlane);
    value.projection={0.5625f,0,0,0,
        0,1,0,0,
        0,0,a,b,
        0,0,1,0};
    value.inverseProjection={1.0f/0.5625f,0,0,0,
        0,1,0,0,
        0,0,0,1,
        0,0,1.0f/b,-a/b};
    value.currentViewProjection=multiply(value.projection,value.view);
    value.previousViewProjection=value.currentViewProjection;
    value.inverseViewProjection=multiply(value.inverseView,
        value.inverseProjection);
    return value;
}
std::array<float,4> transformRow(const std::array<float,4>& point,
    const Matrix& matrix) {
    std::array<float,4> out{};
    for(unsigned column=0;column<4;++column)
        for(unsigned row=0;row<4;++row)
            out[column]+=point[row]*matrix[row*4+column];
    return out;
}
}

TEST_CASE("FG temporal transform includes the change in world camera position",
    "[fg_camera_geometry]") {
    auto sample=camera();
    sample.position={5,0,0};sample.previousPosition={2,0,0};
    const auto result=rk::deriveFgCameraTransforms(sample);
    REQUIRE(std::holds_alternative<rk::FgCameraTransforms>(result));
    const auto& matrices=std::get<rk::FgCameraTransforms>(result);
    REQUIRE(transformRow({1,0,0,1},matrices.clipToPrevClip)==
        std::array<float,4>{4,0,0,1});
    REQUIRE(transformRow({4,0,0,1},matrices.prevClipToClip)==
        std::array<float,4>{1,0,0,1});
}

TEST_CASE("FG camera geometry rejects nonfinite projection or camera motion",
    "[fg_camera_geometry]") {
    for(const bool corruptProjection:{true,false}) {
        auto sample=camera();
        if(corruptProjection)
            sample.projection[0]=std::numeric_limits<float>::quiet_NaN();
        else sample.position[0]=std::numeric_limits<float>::infinity();
        REQUIRE(std::holds_alternative<rk::Error>(
            rk::deriveFgCameraTransforms(sample)));
    }
}

TEST_CASE("FG projection matrices are transposed for Streamline row vectors",
    "[fg_camera_geometry]") {
    auto sample=camera();
    sample.projection[0]=2.0f;
    sample.projection[5]=3.0f;
    sample.projection[10]=4.0f;
    sample.projection[11]=5.0f;
    sample.inverseProjection[0]=0.5f;
    sample.inverseProjection[5]=1.0f/3.0f;
    sample.inverseProjection[10]=0.25f;
    sample.inverseProjection[11]=-1.25f;
    const auto result=rk::deriveFgCameraTransforms(sample);
    REQUIRE(std::holds_alternative<rk::FgCameraTransforms>(result));
    const auto& matrices=std::get<rk::FgCameraTransforms>(result);
    REQUIRE(matrices.cameraViewToClip[14]==5.0f);
    REQUIRE(matrices.clipToCameraView[14]==-1.25f);
    REQUIRE(transformRow({0,0,2,1},matrices.cameraViewToClip)==
        std::array<float,4>{0,0,13,1});
}

TEST_CASE("FG game camera calibration keeps the measured left-handed basis and forward-Z frustum",
    "[fg_camera_geometry]") {
    auto sample=leftHandedCamera();
    sample.position={5,6,7};
    const auto result=rk::deriveFgCameraCalibration(sample);
    REQUIRE(std::holds_alternative<rk::FgCameraCalibration>(result));
    const auto& calibration=std::get<rk::FgCameraCalibration>(result);
    REQUIRE(calibration.right==std::array<float,3>{1,0,0});
    REQUIRE(calibration.up==std::array<float,3>{0,1,0});
    REQUIRE(calibration.forward==std::array<float,3>{0,0,-1});
    REQUIRE(calibration.position==sample.position);
    REQUIRE(std::abs(calibration.nearPlane-15.0f)<0.001f);
    REQUIRE(std::abs(calibration.farPlane-1000.0f)<0.01f);
    REQUIRE(std::abs(calibration.verticalFovRadians-1.5707963f)<1e-5f);
    REQUIRE(std::abs(calibration.horizontalFovRadians-
        2.0f*std::atan(1.0f/0.5625f))<1e-5f);
    REQUIRE(std::abs(calibration.aspectRatio-16.0f/9.0f)<1e-6f);
}

TEST_CASE("FG game calibration rejects a reversed or nonorthonormal camera",
    "[fg_camera_geometry]") {
    auto reversed=leftHandedCamera();
    reversed.projection[10]=-1.0f;
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::deriveFgCameraCalibration(reversed)));
    auto crooked=leftHandedCamera();
    crooked.view[0]=1.1f;
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::deriveFgCameraCalibration(crooked)));
    auto opposite=leftHandedCamera();
    opposite.view=identity();
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::deriveFgCameraCalibration(opposite)));
}
