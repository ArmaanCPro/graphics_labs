#include <random>

#include "Utils/Random.h"
#include <doctest/doctest.h>

TEST_CASE("Random")
{
    using namespace enger::random;

    Xoro256 rng{};

    SUBCASE("Basic Usage")
    {
        auto r = rng();
        auto r2 = rng();
        CHECK_NE(r, r2);
        auto r3 = rng.next();
        auto r4 = rng.next();
        CHECK_NE(r3, r4);
    }
}