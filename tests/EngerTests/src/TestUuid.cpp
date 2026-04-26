#include <doctest/doctest.h>
#include "Utils/Uuid.h"

TEST_CASE("uuidv7 basic validation")
{
    using namespace enger::uuid;
    uuidv7 u1;
    const auto data = u1.data();

    SUBCASE("Version and Variant Bits")
    {
        // Version 7: Byte 6 high bits are all 0b0111 (0x70)
        CHECK_EQ(data[6] & 0xF0, 0x70);
        // Variant 2: Byte 8 high bits are 0b0010 (0x2)
        CHECK_EQ((data[8] & 0xC0), 0x80);
    }

    SUBCASE("Monotonicity")
    {
        uuidv7 u2;
        // Even if in the same millisecond, random bits are different
        CHECK_NE(u1, u2);
    }

    SUBCASE("String Round-Trip")
    {
        std::string s = u1.to_string();
        uuidv7 u3 = uuidv7::from_string(s);
        CHECK_EQ(u1, u3);
    }
}