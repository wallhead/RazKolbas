#include <catch2/catch_test_macros.hpp>
#include "rk/DrsState.hpp"

TEST_CASE("Reduced scene ratio preserves the prior ratio and claims only an unlocked state", "[drs_state]") {
    const rk::RenderSizePlan plan{{2560,1440},{1280,720},true};
    const rk::DrsStateSnapshot initial{2560,1440,1.0f,1.0f,0};
    const auto first=rk::planDrsTransition(plan,initial,false);
    REQUIRE(first.has_value());
    REQUIRE(first->previousWidth==1.0f);
    REQUIRE(first->previousHeight==1.0f);
    REQUIRE(first->currentWidth==0.5f);
    REQUIRE(first->currentHeight==0.5f);
    REQUIRE(first->lock==1);
    const auto next=rk::planDrsTransition(plan,{2560,1440,0.5f,0.5f,1},true);
    REQUIRE(next.has_value());
    REQUIRE(next->previousWidth==0.5f);
    REQUIRE(next->previousHeight==0.5f);
    REQUIRE_FALSE(rk::planDrsTransition(plan,{2560,1440,1.0f,1.0f,1},false));
}

TEST_CASE("DRS transition rejects unready, foreign and mismatched state", "[drs_state]") {
    const rk::RenderSizePlan plan{{2560,1440},{1280,720},true};
    REQUIRE_FALSE(rk::planDrsTransition({{2560,1440},{2560,1440},false},
        {2560,1440,1,1,0},false));
    REQUIRE_FALSE(rk::planDrsTransition(plan,{1920,1080,1,1,0},false));
    REQUIRE_FALSE(rk::planDrsTransition(plan,{2560,1440,1,1,2},false));
    REQUIRE_FALSE(rk::planDrsTransition(plan,{2560,1440,0.5f,0.5f,1},false));
    REQUIRE_FALSE(rk::planDrsTransition(plan,{2560,1440,1,1,1},true));
    REQUIRE_FALSE(rk::planDrsTransition(plan,{2560,1440,0,1,0},false));
}

TEST_CASE("Only the exact owned ratio can release the DRS lock", "[drs_state]") {
    const rk::RenderSizePlan plan{{2560,1440},{1280,720},true};
    const auto released=rk::planDrsRelease(plan,{1920,1080,0.5f,0.5f,1},true);
    REQUIRE(released.has_value());
    REQUIRE(released->previousWidth==0.5f);
    REQUIRE(released->currentWidth==1.0f);
    REQUIRE(released->currentHeight==1.0f);
    REQUIRE(released->lock==0);
    REQUIRE_FALSE(rk::planDrsRelease(plan,{1920,1080,0.75f,0.5f,1},true));
    REQUIRE_FALSE(rk::planDrsRelease(plan,{1920,1080,0.5f,0.5f,1},false));
}

TEST_CASE("An initial native-ratio lock can clear before a later world frame", "[drs_state]") {
    const rk::RenderSizePlan plan{{2560,1440},{1706,960},true};
    REQUIRE(rk::drsStateMayRetryAfterNativeLock(plan,{2560,1440,1.0f,1.0f,1}));
    REQUIRE_FALSE(rk::drsStateMayRetryAfterNativeLock(plan,{2560,1440,0.8f,1.0f,1}));
    REQUIRE_FALSE(rk::drsStateMayRetryAfterNativeLock(plan,{2560,1440,1.0f,1.0f,2}));
    REQUIRE_FALSE(rk::drsStateMayRetryAfterNativeLock(plan,{1920,1080,1.0f,1.0f,1}));
    REQUIRE_FALSE(rk::drsStateMayRetryAfterNativeLock(plan,{2560,1440,1.0f,1.0f,0}));
    REQUIRE_FALSE(rk::drsStateMayRetryAfterNativeLock(plan,{2560,1440,0.0f,1.0f,1}));
    REQUIRE_FALSE(rk::drsStateMayRetryAfterNativeLock({{2560,1440},{2560,1440},false},
        {2560,1440,1.0f,1.0f,1}));
}
