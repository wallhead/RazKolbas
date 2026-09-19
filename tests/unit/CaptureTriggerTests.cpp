#include "rk/CaptureTrigger.hpp"
#include "rk/Settings.hpp"
#include <catch2/catch_test_macros.hpp>
using rk::CaptureSignal;
TEST_CASE("Capture requires a new foreground chord and never runs at startup", "[capture_trigger]") {
    rk::CaptureTrigger trigger;
    REQUIRE(trigger.poll(0,true,true)==CaptureSignal::None);
    REQUIRE(trigger.poll(90000,true,true)==CaptureSignal::None);
    REQUIRE(trigger.poll(90001,true,false)==CaptureSignal::None);
    REQUIRE(trigger.poll(90002,true,true)==CaptureSignal::Attempt);
    REQUIRE(trigger.requests()==1);
    trigger.consume();
    REQUIRE(trigger.poll(100000,true,true)==CaptureSignal::None);
    REQUIRE(trigger.poll(100001,true,false)==CaptureSignal::None);
    REQUIRE(trigger.poll(100002,true,true)==CaptureSignal::Attempt);
    REQUIRE(trigger.requests()==2);
}
TEST_CASE("Capture cancels on focus loss and requires release after refocusing", "[capture_trigger]") {
    rk::CaptureTrigger trigger;
    trigger.poll(0,true,false);
    REQUIRE(trigger.poll(1,true,true)==CaptureSignal::Attempt);
    REQUIRE(trigger.poll(2,false,true)==CaptureSignal::None);
    REQUIRE(trigger.poll(6000,true,true)==CaptureSignal::None);
    REQUIRE(trigger.poll(6001,true,false)==CaptureSignal::None);
    REQUIRE(trigger.poll(6002,true,true)==CaptureSignal::Attempt);
}
TEST_CASE("Capture retries are bounded and expire without late automatic capture", "[capture_trigger]") {
    rk::CaptureTrigger trigger;
    trigger.poll(0,true,false);
    REQUIRE(trigger.poll(1,true,true)==CaptureSignal::Attempt);
    REQUIRE(trigger.poll(2,true,false)==CaptureSignal::None);
    REQUIRE(trigger.poll(50,true,false)==CaptureSignal::None);
    REQUIRE(trigger.poll(51,true,false)==CaptureSignal::Attempt);
    // Regular polling, but not fast enough to exhaust 32 retries before expiry.
    for(unsigned t=201;t<2001;t+=200)REQUIRE(trigger.poll(t,true,false)==CaptureSignal::Attempt);
    REQUIRE(trigger.poll(2001,true,false)==CaptureSignal::Expired);
    REQUIRE(trigger.poll(50000,true,false)==CaptureSignal::None);
    REQUIRE(trigger.requests()==1);
    rk::CaptureTrigger bounded;bounded.poll(0,true,false);
    for(unsigned i=0;i<32;++i)REQUIRE(bounded.poll(1+i*50,true,true)==CaptureSignal::Attempt);
    REQUIRE(bounded.poll(1601,true,true)==CaptureSignal::Expired);
    REQUIRE(bounded.attempts()==32);
}
TEST_CASE("A presentation gap cancels requests and rejects an already-held chord", "[capture_trigger]") {
    rk::CaptureTrigger idle;idle.poll(0,true,false);
    // No background Present callback occurs during this simulated minimize.
    REQUIRE(idle.poll(1000,true,true)==CaptureSignal::None);
    REQUIRE(idle.requests()==0);
    idle.poll(1001,true,false);
    REQUIRE(idle.poll(1002,true,true)==CaptureSignal::Attempt);
    REQUIRE(idle.poll(1500,true,true)==CaptureSignal::Interrupted);
    REQUIRE(idle.poll(1501,true,false)==CaptureSignal::None);
    REQUIRE(idle.poll(1600,true,false)==CaptureSignal::None);
}
TEST_CASE("Capture cooldown discards presses and session budget includes failures", "[capture_trigger]") {
    rk::CaptureTrigger trigger;trigger.poll(0,true,false);
    REQUIRE(trigger.poll(1,true,true)==CaptureSignal::Attempt);trigger.consume();
    trigger.poll(2,true,false);
    REQUIRE(trigger.poll(4999,true,true)==CaptureSignal::None);
    REQUIRE(trigger.poll(5001,true,true)==CaptureSignal::None); // no queued cooldown press
    trigger.poll(5002,true,false);
    for(unsigned i=1;i<6;++i) {
        const auto now=5003+i*5000;
        trigger.poll(now-1,true,false);
        REQUIRE(trigger.poll(now,true,true)==CaptureSignal::Attempt);
        trigger.consume(); // success and failure both spend the slot
    }
    trigger.poll(50000,true,false);
    REQUIRE(trigger.poll(50001,true,true)==CaptureSignal::LimitReached);
    REQUIRE(trigger.poll(60000,true,true)==CaptureSignal::None);
    REQUIRE(trigger.requests()==6);
}
TEST_CASE("Capture hotkey is opt-in validated and restart-scoped", "[capture_trigger][config]") {
    const auto defaults=rk::defaultSettings();
    REQUIRE(defaults.values.contains("Diagnostics.CaptureHotkey"));
    REQUIRE(defaults.get<rk::Choice>("Diagnostics.CaptureHotkey").value=="Off");
    const auto parsed=rk::parseIni("[Diagnostics]\nCaptureHotkey=CtrlShiftF10\n");
    REQUIRE(std::holds_alternative<rk::Settings>(parsed));
    REQUIRE(std::get<rk::Settings>(parsed).get<rk::Choice>("Diagnostics.CaptureHotkey").value=="CtrlShiftF10");
    REQUIRE(rk::classifyChange(defaults,std::get<rk::Settings>(parsed))==rk::ChangeCategory::RestartRequired);
    REQUIRE(std::holds_alternative<rk::Error>(rk::parseIni("[Diagnostics]\nCaptureHotkey=F10\n")));
}
