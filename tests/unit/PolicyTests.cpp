#include <catch2/catch_test_macros.hpp>
#include "rk/Capabilities.hpp"
#include "rk/FrameContracts.hpp"
#include "rk/LoadingPictureProbe.hpp"

TEST_CASE("Loading picture probe selects one settled cold and one settled warm frame", "[loading_picture]") {
    rk::LoadingPictureProbe probe;
    REQUIRE_FALSE(probe.observe(0,false,true,false));
    for(std::uint64_t frame=1;frame<30;++frame) {
        REQUIRE_FALSE(probe.observe(frame,true,false,false));
        REQUIRE_FALSE(probe.observe(frame,true,false,false));
    }
    const auto cold=probe.observe(30,true,false,false);
    REQUIRE(cold);
    REQUIRE(cold->frame==30);
    REQUIRE(cold->phase==rk::LoadingPicturePhase::Cold);
    REQUIRE_FALSE(probe.observe(30,true,false,false));
    for(std::uint64_t frame=31;frame<90;++frame)
        REQUIRE_FALSE(probe.observe(frame,true,false,false));
    REQUIRE_FALSE(probe.observe(90,false,false,true));
    for(std::uint64_t frame=91;frame<120;++frame)
        REQUIRE_FALSE(probe.observe(frame,true,false,true));
    const auto warm=probe.observe(120,true,false,true);
    REQUIRE(warm);
    REQUIRE(warm->frame==120);
    REQUIRE(warm->phase==rk::LoadingPicturePhase::AfterWorld);
    for(std::uint64_t frame=121;frame<200;++frame)
        REQUIRE_FALSE(probe.observe(frame,true,false,true));
}

TEST_CASE("Loading picture probe requires consecutive distinct frames", "[loading_picture]") {
    rk::LoadingPictureProbe probe;
    REQUIRE_FALSE(probe.observe(0,false,true,false));
    REQUIRE_FALSE(probe.observe(10,true,false,false));
    for(int i=0;i<40;++i)REQUIRE_FALSE(probe.observe(10,true,false,false));
    REQUIRE_FALSE(probe.observe(12,true,false,false));
    for(std::uint64_t frame=13;frame<41;++frame)
        REQUIRE_FALSE(probe.observe(frame,true,false,false));
    REQUIRE(probe.observe(41,true,false,false));
}
TEST_CASE("Loading picture probe skips the initial load before Main Menu", "[loading_picture]") {
    rk::LoadingPictureProbe probe;
    for(std::uint64_t frame=1;frame<=100;++frame)
        REQUIRE_FALSE(probe.observe(frame,true,false,false));
    REQUIRE_FALSE(probe.observe(101,false,true,false));
    for(std::uint64_t frame=102;frame<131;++frame)
        REQUIRE_FALSE(probe.observe(frame,true,false,false));
    const auto selected=probe.observe(131,true,false,false);
    REQUIRE(selected);
    REQUIRE(selected->phase==rk::LoadingPicturePhase::Cold);
}

TEST_CASE("Unavailable explicit DLSS retains request and explains native fallback", "[policy]") {
    const auto selection = rk::selectSr(rk::Provider::DLSS, 0x10de, {{rk::Provider::DLSS, false, rk::Validation::Unverified, -8, "runtime missing"}});
    REQUIRE(selection.requested == rk::Provider::DLSS);
    REQUIRE(selection.effective == rk::Provider::Native);
    REQUIRE(selection.reason.find("runtime missing") != std::string::npos);
}
TEST_CASE("Auto requires validation and chooses actual render vendor first", "[policy]") {
    std::vector<rk::FeatureCapability> caps{
        {rk::Provider::DLSS, true, rk::Validation::Unverified},
        {rk::Provider::FSR, true, rk::Validation::Validated},
        {rk::Provider::XeSS, true, rk::Validation::Validated}};
    REQUIRE(rk::selectSr(rk::Provider::Auto, 0x10de, caps).effective == rk::Provider::FSR);
    REQUIRE(rk::selectSr(rk::Provider::Auto, 0x8086, caps).effective == rk::Provider::XeSS);
    caps[0].validation = rk::Validation::Experimental;
    REQUIRE(rk::selectSr(rk::Provider::Auto, 0x10de, caps).effective == rk::Provider::FSR);
    REQUIRE(rk::selectSr(rk::Provider::Auto, 0x10de, caps, true).effective == rk::Provider::DLSS);
}
TEST_CASE("Older successful evaluation cannot consume a newer reset", "[history]") {
    rk::HistoryEpoch history;
    for (int i=0; i<6; ++i) history.request();
    const auto captured = history.capture();
    REQUIRE(captured == 7);
    REQUIRE(history.request() == 8);
    history.consume(captured, true);
    REQUIRE(history.pending());
    history.consume(8, false);
    REQUIRE(history.pending());
    history.consume(8, true);
    REQUIRE_FALSE(history.pending());
    history.consume(7, true);
    REQUIRE_FALSE(history.pending());
}
TEST_CASE("Generated displays leave real-frame identity unchanged", "[frame_identity]") {
    rk::FrameIdentity identity;
    REQUIRE_FALSE(identity.presentGenerated(0));
    REQUIRE(identity.beginSource() == 1);
    for (int i=0; i<3; ++i) REQUIRE(identity.presentGenerated(1));
    REQUIRE(identity.source() == 1);
    REQUIRE(identity.generated() == 3);
    REQUIRE(identity.beginSource() == 2);
    REQUIRE_FALSE(identity.presentGenerated(1));
    REQUIRE_FALSE(rk::Extent{0, 1080}.valid());
}
