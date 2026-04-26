#pragma once

#include <cstdint>
#include <array>
#include <chrono>
#include <random>
#include <span>
#include <functional>
#include <ostream>
#include <format>

// fmt is from spdlog
#include <fmt/format.h>

#include "Profiling/Profiler.h"
#include "Random.h"

namespace enger::uuid
{
    // A minimal UUID V7 Implementation.
    class uuidv7
    {
    public:
        uuidv7()
        {
            using namespace std::chrono;
            ENGER_PROFILE_FUNCTION()

            thread_local random::Xoro256 gen{std::random_device{}()};

            // Set the first 6 bytes to the timestamp
            const uint64_t ms = system_clock::now().time_since_epoch() / 1ms;
            for (auto i = 0; i < 6; i++)
            {
                m_Data[i] = static_cast<uint8_t>(ms >> (40 - i * 8));
            }
            const uint64_t r1 = gen.next();
            const uint64_t r2 = gen.next();

            // We have to manually set these bytes instead of memcpy because Uuid requires big-endian.
            m_Data[6] = static_cast<uint8_t>(r1 >> 56);
            m_Data[7] = static_cast<uint8_t>(r1 >> 48);

            for (auto i = 0; i < 8; i++)
            {
                m_Data[i + 8] = static_cast<uint8_t>(r2 >> (64 - i * 8));
            }

            m_Data[6] = (m_Data[6] & 0x0F) | 0x70; // Set version 7
            m_Data[8] = (m_Data[8] & 0x3F) | 0x80; // Set variant 2
        }

        [[nodiscard]] auto data() const noexcept -> std::span<const uint8_t> { return m_Data; }
        [[nodiscard]] auto data() noexcept -> std::span<uint8_t> { return m_Data; }
        [[nodiscard]] auto timestamp() const noexcept -> uint64_t
        {
            uint64_t ts = 0;
            for (auto i = 0; i < 6; i++)
            {
                ts |= (ts << 8) | m_Data[i];
            }
            return ts;
        }

        auto operator<=>(const uuidv7&) const noexcept = default;

        [[nodiscard]] std::string to_string() const
        {
            static constexpr char hexChars[] = "0123456789abcdef";
            std::string res(36, '-');
            size_t charIndex = 0;
            for (size_t i = 0; i < 16; ++i)
            {
                if (i == 4 || i == 6 || i == 8 || i == 10)
                {
                    charIndex++;
                }
                res[charIndex++] = hexChars[(m_Data[i] >> 4) & 0x0F];
                res[charIndex++] = hexChars[m_Data[i] & 0x0F];
            }
            return res;
        }

        static uuidv7 from_string(std::string_view s)
        {
            uuidv7 uuid = uuidv7::nil();
            auto& data = uuid.m_Data;

            size_t byteIndex = 0;
            for (size_t i = 0; i < s.size() && byteIndex < 16; ++i)
            {
                if (s[i] == '-') continue;
                if (i + 1 >= s.size()) break;

                auto hexToInt = [](char c) -> uint8_t {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    return 0;
                };

                data[byteIndex++] = (hexToInt(s[i]) << 4) | hexToInt(s[i + 1]);
                i++; // skip the second char of the pair
            }
            return uuid;
        }

        [[nodiscard]] static constexpr uuidv7 nil() noexcept
        {
            return uuidv7{InternalNilTag{}};
        }
        [[nodiscard]] static constexpr uuidv7 max() noexcept
        {
            return uuidv7{InternalMaxTag{}};
        }

    private:
        struct InternalNilTag{};
        explicit constexpr uuidv7(InternalNilTag) noexcept : m_Data{0} {}
        struct InternalMaxTag{};
        explicit constexpr uuidv7(InternalMaxTag) noexcept : m_Data{std::numeric_limits<uint8_t>::max()} {}
        std::array<uint8_t, 16> m_Data{};
    };

    inline std::ostream& operator<<(std::ostream& os, const uuidv7& uuid)
    {
        return os << uuid.to_string();
    }
}


template<>
struct std::hash<enger::uuid::uuidv7>
{
    std::size_t operator()(const enger::uuid::uuidv7& uuid) const noexcept
    {
        const auto parts = reinterpret_cast<const uint64_t*>(uuid.data().data());
        return parts[0] ^ parts[1];
    }
};

template<>
struct std::formatter<enger::uuid::uuidv7> : std::formatter<std::string>
{
    auto format(const enger::uuid::uuidv7& uuid, std::format_context& ctx) const
    {
        return std::formatter<std::string>::format(uuid.to_string(), ctx);
    }
};

template<>
struct fmt::formatter<enger::uuid::uuidv7>
{
    constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const enger::uuid::uuidv7& uuid, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "{}", uuid.to_string());
    }
};
