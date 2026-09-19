#include "packet_target.hpp"
#include <array>
int main() {
  //17 CIF0 selectors, all13 CIF7 attributes:221 views, zero value bytes.
  const std::array<std::uint32_t,8> query{0x60000008,1,0xa0040000,1,2,3,0x7fffc080,0xfff80000};
  std::array<std::byte,32> wire{};
  for(std::size_t i=0;i<query.size();++i)for(unsigned b=0;b<4;++b)wire[i*4+b]=std::byte((query[i]>>(24-8*b))&255);
  auto small=vita::codec::decode_packet(wire);
  vita::fuzz::require(!small && small.error().code==vita::ErrorCode::resource_limit);
  auto large=vita::codec::decode_packet_bounded<128,1664>(wire);
  vita::fuzz::require(large && large->fields.size()==221);
  vita::fuzz::exercise(wire);
  auto limited=vita::codec::DecodeOptions{};limited.limits.work_units=220;
  auto rejected=vita::codec::decode_packet_bounded<128,1664>(wire,limited);
  vita::fuzz::require(!rejected && rejected.error().code==vita::ErrorCode::resource_limit);
  //17 literal single-word CIF2 fields, no helper-generated encoder oracle.
  const std::array<std::uint32_t,21> context{0x40000015,1,4,0xc6bffc00,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  std::array<std::byte,84> values{};
  for(std::size_t i=0;i<context.size();++i)for(unsigned b=0;b<4;++b)values[i*4+b]=std::byte((context[i]>>(24-8*b))&255);
  auto parsed=vita::codec::decode_packet_bounded<128,1664>(values);
  vita::fuzz::require(parsed && parsed->fields.size()==17);
  vita::fuzz::exercise(values);
}
