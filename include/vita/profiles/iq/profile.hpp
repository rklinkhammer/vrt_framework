#pragma once
#include <cstdint>
namespace vita::profiles::iq {
enum class Profile : std::uint8_t { generator_v1, frequency_tunable };
constexpr std::uint16_t information_class(Profile profile) noexcept { return profile==Profile::frequency_tunable?2:1; }
constexpr std::uint16_t context_class(Profile profile) noexcept { return profile==Profile::frequency_tunable?0x110:0x10; }
constexpr std::uint16_t command_class(Profile profile) noexcept { return profile==Profile::frequency_tunable?0x120:0x20; }
constexpr std::uint16_t data_class(Profile profile,std::uint16_t baseline) noexcept { return profile==Profile::frequency_tunable?static_cast<std::uint16_t>(0x100+baseline):baseline; }
inline constexpr std::uint64_t minimum_center_hz=1'000'000,maximum_center_hz=6'000'000'000;
}
