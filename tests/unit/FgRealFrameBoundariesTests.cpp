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
