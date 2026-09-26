#include <catch2/catch_test_macros.hpp>
#include "rk/NrCommandRing.hpp"

TEST_CASE("NR command slots are reused only after their output fence retires",
    "[nr_command_ring]") {
    rk::NrCommandRing ring;
    const auto first=ring.acquire(0);
    REQUIRE(first==0);
    ring.markSubmitted(*first,1);
    const auto second=ring.acquire(0);
    REQUIRE(second==1);
    ring.markSubmitted(*second,3);
    const auto third=ring.acquire(0);
    REQUIRE(third==2);
    ring.markSubmitted(*third,5);
    REQUIRE_FALSE(ring.acquire(0));
    REQUIRE(ring.acquire(2)==0);
    ring.markSubmitted(0,7);
    REQUIRE_FALSE(ring.acquire(2));
    REQUIRE(ring.acquire(3)==1);
    REQUIRE(ring.acquire(5)==2);
}
