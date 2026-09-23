#include <catch2/catch_test_macros.hpp>
#include "rk/RenderSizePolicy.hpp"
#include <cmath>

TEST_CASE("Configured SR quality has a complete exact mapping", "[render_size_policy]") {
    using rk::UpscaleQuality;
    REQUIRE(rk::parseUpscaleQuality("NativeAA")==UpscaleQuality::NativeAA);
    REQUIRE(rk::parseUpscaleQuality("Quality")==UpscaleQuality::Quality);
    REQUIRE(rk::parseUpscaleQuality("Balanced")==UpscaleQuality::Balanced);
    REQUIRE(rk::parseUpscaleQuality("Performance")==UpscaleQuality::Performance);
    REQUIRE(rk::parseUpscaleQuality("UltraPerformance")==UpscaleQuality::UltraPerformance);
    REQUIRE_FALSE(rk::parseUpscaleQuality("quality").has_value());
}

TEST_CASE("Early owned scene sizing is deterministic before provider creation", "[render_size_policy]") {
    const rk::Extent display{2560,1440};
    const auto quality=rk::planEarlyOwnedScene(display,rk::UpscaleQuality::Quality,0.0);
    REQUIRE(quality.width==1707);REQUIRE(quality.height==960);
    const auto performance=rk::planEarlyOwnedScene(display,rk::UpscaleQuality::Performance,0.0);
    REQUIRE(performance.width==1280);REQUIRE(performance.height==720);
    const auto manual=rk::planEarlyOwnedScene(display,rk::UpscaleQuality::Quality,0.6);
    REQUIRE(manual.width==1536);REQUIRE(manual.height==864);
    REQUIRE_FALSE(rk::planEarlyOwnedScene(display,rk::UpscaleQuality::NativeAA,0.0).valid());
    REQUIRE_FALSE(rk::planEarlyOwnedScene({0,1440},rk::UpscaleQuality::Quality,0.0).valid());
    REQUIRE_FALSE(rk::planEarlyOwnedScene(display,rk::UpscaleQuality::Quality,0.1).valid());
}

TEST_CASE("Reduced world sizing requires both owned paths", "[render_size_policy]") {
    const rk::Extent display{2560,1440}, render{1280,720};
    auto native=rk::chooseRenderSize(display,render,false,true);
    REQUIRE(native.valid());
    REQUIRE_FALSE(native.reduced);
    REQUIRE(native.render.width==2560);
    REQUIRE_FALSE(rk::chooseRenderSize(display,render,true,false).reduced);
    REQUIRE(rk::chooseRenderSize(display,render,true,true).reduced);
    REQUIRE_FALSE(rk::chooseRenderSize(display,display,true,true).reduced);
    REQUIRE_FALSE(rk::chooseRenderSize(display,{3000,720},true,true).reduced);
    REQUIRE_FALSE(rk::chooseRenderSize(display,{0,720},true,true).reduced);
}

TEST_CASE("Automatic mip bias follows the smaller reduced-render axis",
    "[render_size_policy]") {
    const auto automatic=rk::resolveMipLodBias({1707,960},{2560,1440},true,0.0);
    REQUIRE(std::holds_alternative<float>(automatic));
    REQUIRE(std::abs(std::get<float>(automatic)+0.5849625f)<0.00001f);
    const auto manual=rk::resolveMipLodBias({1707,960},{2560,1440},false,-0.25);
    REQUIRE(std::get<float>(manual)==-0.25f);
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::resolveMipLodBias({1707,960},{2560,1440},false,-4.0)));
    REQUIRE(std::holds_alternative<rk::Error>(
        rk::resolveMipLodBias({3000,960},{2560,1440},true,0.0)));
}

TEST_CASE("Only world scissors scale and fractional edges retain coverage", "[render_size_policy]") {
    auto plan=rk::chooseRenderSize({2560,1440},{1280,720},true,true);
    const rk::ScissorExtent source{3,5,101,103};
    const auto world=rk::adaptScissor(plan,rk::RenderDomain::World,source);
    REQUIRE(world.x==1);
    REQUIRE(world.y==2);
    REQUIRE(world.width==51);
    REQUIRE(world.height==52);
    const auto ui=rk::adaptScissor(plan,rk::RenderDomain::Ui,source);
    REQUIRE(ui.x==source.x);
    REQUIRE(ui.y==source.y);
    REQUIRE(ui.width==source.width);
    REQUIRE(ui.height==source.height);
    const auto native=rk::adaptScissor(rk::chooseRenderSize({2560,1440},{1280,720},false,true),
        rk::RenderDomain::World,source);
    REQUIRE(native.x==source.x);
    REQUIRE(native.width==source.width);
}

TEST_CASE("World scissors clamp without 32-bit overflow", "[render_size_policy]") {
    auto plan=rk::chooseRenderSize({2560,1440},{1280,720},true,true);
    const auto empty=rk::adaptScissor(plan,rk::RenderDomain::World,{3,5,0,0});
    REQUIRE(empty.width==0);
    REQUIRE(empty.height==0);
    const auto edge=rk::adaptScissor(plan,rk::RenderDomain::World,{2559,1439,UINT32_MAX,UINT32_MAX});
    REQUIRE(edge.x==1279);
    REQUIRE(edge.y==719);
    REQUIRE(edge.width==1);
    REQUIRE(edge.height==1);
    REQUIRE_FALSE(rk::chooseRenderSize({0,1440},{1280,720},true,true).valid());
    REQUIRE_FALSE(rk::RenderSizePlan{{2560,1440},{3000,720},true}.valid());
}
