#include <catch2/catch_test_macros.hpp>
#include "rk/NrEvaluationRecovery.hpp"

TEST_CASE("One failed NR evaluation waits for GPU handoff and retries with reset",
    "[nr_evaluation_recovery]") {
    rk::NrEvaluationRecovery recovery;
    unsigned waits{},flushes{};
    const auto action=recovery.failed([&] { ++waits; return true; },
        [&] { REQUIRE(waits==flushes+1); ++flushes; });
    REQUIRE(action==rk::NrEvaluationAction::Retry);
    REQUIRE(waits==1);
    REQUIRE(flushes==1);
    REQUIRE(recovery.consecutiveFailures()==1);
    recovery.succeeded();
    REQUIRE(recovery.consecutiveFailures()==0);
    REQUIRE(recovery.failed([&] { ++waits; return true; },
        [&] { ++flushes; })==
        rk::NrEvaluationAction::Retry);
    REQUIRE(flushes==2);
}

TEST_CASE("Repeated failed NR evaluations disable only after the third failure",
    "[nr_evaluation_recovery]") {
    rk::NrEvaluationRecovery recovery;
    unsigned waits{},flushes{};
    for(unsigned attempt=1;attempt<=3;++attempt) {
        const auto action=recovery.failed([&] { ++waits; return true; },
            [&] { ++flushes; });
        REQUIRE(action==(attempt<3?rk::NrEvaluationAction::Retry:
            rk::NrEvaluationAction::Disable));
    }
    REQUIRE(waits==3);
    REQUIRE(flushes==3);
    REQUIRE(recovery.consecutiveFailures()==3);
}

TEST_CASE("Failed NR GPU handoff is immediately fatal", "[nr_evaluation_recovery]") {
    rk::NrEvaluationRecovery recovery;
    bool flushed=false;
    REQUIRE(recovery.failed([] { return false; },[&] { flushed=true; })==
        rk::NrEvaluationAction::DeviceLost);
    REQUIRE_FALSE(flushed);
    REQUIRE(recovery.consecutiveFailures()==0);
}

TEST_CASE("A later failed NR output outlives an earlier copyback fence",
    "[nr_evaluation_recovery]") {
    rk::NrPendingFences pending;
    pending.markOutput(5);
    pending.markConsumer(6);
    pending.markOutput(8);
    REQUIRE(pending.consumer()==6);
    REQUIRE(pending.output()==8);
    REQUIRE_FALSE(pending.retireConsumer());
    REQUIRE(pending.consumer()==0);
    REQUIRE(pending.output()==8);
    pending.retireOutput();
    REQUIRE(pending.output()==0);
    pending.markOutput(10);
    pending.markConsumer(11);
    REQUIRE(pending.retireConsumer());
    REQUIRE(pending.output()==0);
}
