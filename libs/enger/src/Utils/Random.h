#pragma once

#include <array>
#include <cstdint>
#include <random>

namespace enger::random
{
    // Xoroshiro256++ implementation. Consider making this generator thread_local.
    struct Xoro256
    {
    public:
        explicit constexpr Xoro256(uint64_t seed) noexcept
        {
            uint64_t state = seed;
            for (auto& s : m_State)
            {
                s = splitmix64(state);
            }
        }
        constexpr Xoro256() noexcept
            : Xoro256(std::random_device{}())
        {}

        // Call to actually get a random number.
        constexpr uint64_t next() noexcept
        {
            const uint64_t result = rotl(m_State[0] + m_State[3], 23) + m_State[0];
            const uint64_t t = m_State[1] << 17;

            m_State[2] ^= m_State[0];
            m_State[3] ^= m_State[1];
            m_State[1] ^= m_State[2];
            m_State[0] ^= m_State[3];

            m_State[2] ^= t;
            m_State[3] = rotl(m_State[3], 45);

            return result;
        }

        constexpr auto operator()() noexcept
        {
            return next();
        }

    private:
        std::array<uint64_t, 4> m_State{};

        static constexpr uint64_t rotl(const uint64_t x, int k)
        {
            return (x << k) | (x >> (64 - k));
        }

        static constexpr uint64_t splitmix64(uint64_t& state)
        {
            uint64_t z = (state += 0x9E3779B97F4A7C15);
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EB;
            return z ^ (z >> 31);
        }
    };
}