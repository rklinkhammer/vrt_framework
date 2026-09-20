#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
namespace vita::profiles::iq {
enum class Profile : std::uint8_t { generator_v1, frequency_tunable, graphx_radio };
constexpr std::uint16_t information_class(Profile profile) noexcept { return profile==Profile::graphx_radio?0:profile==Profile::frequency_tunable?2:1; }
constexpr std::uint16_t context_class(Profile profile) noexcept { return profile==Profile::graphx_radio?0:profile==Profile::frequency_tunable?0x110:0x10; }
constexpr std::uint16_t command_class(Profile profile) noexcept { return profile==Profile::graphx_radio?0:profile==Profile::frequency_tunable?0x120:0x20; }
constexpr std::uint16_t data_class(Profile profile,std::uint16_t baseline) noexcept { return profile==Profile::graphx_radio?0:profile==Profile::frequency_tunable?static_cast<std::uint16_t>(0x100+baseline):baseline; }
inline constexpr std::uint32_t graphx_unknown_oui=0x00ffffff;
enum class SampleFrame : std::uint8_t { single=0,first=1,middle=2,final=3 };
inline constexpr std::size_t graphx_burst_pairs=1024u*1024u/4u;
constexpr std::size_t graphx_packet_pairs(std::uint64_t ordinal,std::size_t maximum,std::size_t burst_pairs=graphx_burst_pairs) noexcept {
	const auto remaining=burst_pairs-ordinal%burst_pairs;
	return std::min<std::size_t>(maximum,remaining);
}
constexpr SampleFrame graphx_sample_frame(std::uint64_t ordinal,std::size_t pairs,std::size_t burst_pairs=graphx_burst_pairs) noexcept {
	const bool first=ordinal%burst_pairs==0;
	const bool final=ordinal%burst_pairs+pairs==burst_pairs;
	return first?(final?SampleFrame::single:SampleFrame::first):(final?SampleFrame::final:SampleFrame::middle);
}
constexpr std::uint32_t graphx_trailer(SampleFrame frame) noexcept {
	return 0x00c00000u|(static_cast<std::uint32_t>(frame)<<10);
}
inline constexpr std::uint64_t minimum_center_hz=1'000'000,maximum_center_hz=6'000'000'000;
}
