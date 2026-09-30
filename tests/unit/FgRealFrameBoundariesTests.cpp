#include <catch2/catch_test_macros.hpp>
#include "rk/FgRealFrameBoundaries.hpp"

TEST_CASE("FG boundary pairs one completed world with one real Present",
    "[fg_real_boundaries]") {
    rk::FgRealFrameBoundaries boundaries;
    REQUIRE(boundaries.present(10,false,true).kind==rk::FgBoundaryKind::NoWorld);
    boundaries.world(10);
    const auto first=boundaries.present(10,false,true);
    REQUIRE(first.kind==rk::FgBoundaryKind::Ready);
    REQUIRE(first.world==1);
    REQUIRE(first.epoch==1);
    REQUIRE(first.worldThread==10);
    REQUIRE(first.presentThread==10);
    REQUIRE(boundaries.present(10,false,true).kind==rk::FgBoundaryKind::NoWorld);
}

TEST_CASE("FG TEST and foreign swap do not consume a world",
    "[fg_real_boundaries]") {
    rk::FgRealFrameBoundaries boundaries;
    boundaries.world(11);
    REQUIRE(boundaries.present(11,true,true).kind==rk::FgBoundaryKind::Test);
    REQUIRE(boundaries.present(11,false,false).kind==rk::FgBoundaryKind::ForeignSwap);
    REQUIRE(boundaries.present(11,false,true).kind==rk::FgBoundaryKind::Ready);
}

TEST_CASE("FG boundary rejects multiple world draws and stale resize frame",
    "[fg_real_boundaries]") {
    rk::FgRealFrameBoundaries boundaries;
    boundaries.world(12);
    boundaries.world(12);
    const auto multiple=boundaries.present(12,false,true);
    REQUIRE(multiple.kind==rk::FgBoundaryKind::MultipleWorlds);
    REQUIRE(multiple.world==2);
    REQUIRE(boundaries.present(12,false,true).kind==rk::FgBoundaryKind::NoWorld);
    boundaries.world(12);
    boundaries.reset();
    const auto afterResize=boundaries.present(12,false,true);
    REQUIRE(afterResize.kind==rk::FgBoundaryKind::NoWorld);
    REQUIRE(afterResize.epoch==2);
    boundaries.world(12);
    REQUIRE(boundaries.present(12,false,true).kind==rk::FgBoundaryKind::Ready);
}

TEST_CASE("FG boundary rejects intra-frame thread change but accepts next-frame migration",
    "[fg_real_boundaries]") {
    rk::FgRealFrameBoundaries boundaries;
    boundaries.world(21);
    REQUIRE(boundaries.present(22,false,true).kind==rk::FgBoundaryKind::ThreadMismatch);
    boundaries.world(22);
    REQUIRE(boundaries.present(22,false,true).kind==rk::FgBoundaryKind::Ready);
    boundaries.world(21);
    REQUIRE(boundaries.present(21,false,true).kind==rk::FgBoundaryKind::Ready);
}

TEST_CASE("FG phase needs one ordered renderer, world entry and completion", "[fg_real_boundaries]") {
    rk::FgRealFrameBoundaries boundaries;
    boundaries.rendererBegin(31);
    boundaries.worldBegin(31);
    boundaries.world(31);
    const auto test=boundaries.present(31,true,true);
    REQUIRE(test.rendererBegins==1);
    REQUIRE(test.worldBegins==1);
    REQUIRE(boundaries.present(31,false,false).worldBegins==1);
    const auto ready=boundaries.present(31,false,true);
    REQUIRE(ready.kind==rk::FgBoundaryKind::Ready);
    REQUIRE(ready.phaseReady);
    REQUIRE(ready.phaseReadyCount==1);
    REQUIRE(ready.rendererThread==31);
    REQUIRE(ready.worldBeginThread==31);
    REQUIRE_FALSE(boundaries.present(31,false,true).phaseReady);
    boundaries.rendererBegin(32);
    boundaries.worldBegin(32);
    boundaries.world(32);
    REQUIRE(boundaries.present(32,false,true).phaseReady);
}

TEST_CASE("FG phase rejects missing duplicate and late entries", "[fg_real_boundaries]") {
    rk::FgRealFrameBoundaries boundaries;
    boundaries.worldBegin(41);
    boundaries.world(41);
    REQUIRE_FALSE(boundaries.present(41,false,true).phaseReady);
    boundaries.rendererBegin(41);
    boundaries.rendererBegin(41);
    boundaries.worldBegin(41);
    boundaries.world(41);
    const auto duplicate=boundaries.present(41,false,true);
    REQUIRE(duplicate.rendererBegins==2);
    REQUIRE_FALSE(duplicate.phaseReady);
    boundaries.worldBegin(41);
    boundaries.rendererBegin(41);
    boundaries.world(41);
    REQUIRE_FALSE(boundaries.present(41,false,true).phaseReady);
    boundaries.rendererBegin(41);
    boundaries.world(41);
    REQUIRE_FALSE(boundaries.present(41,false,true).phaseReady);
}

TEST_CASE("FG phase rejects cross-thread events and resize-stale entry", "[fg_real_boundaries]") {
    rk::FgRealFrameBoundaries boundaries;
    boundaries.rendererBegin(51);
    boundaries.worldBegin(52);
    boundaries.world(52);
    REQUIRE_FALSE(boundaries.present(52,false,true).phaseReady);
    boundaries.rendererBegin(52);
    boundaries.worldBegin(52);
    boundaries.world(52);
    boundaries.reset();
    REQUIRE_FALSE(boundaries.present(52,false,true).phaseReady);
    boundaries.rendererBegin(51);
    boundaries.worldBegin(51);
    boundaries.world(51);
    REQUIRE(boundaries.present(51,false,true).phaseReady);
}
