#pragma once
#include <vita/runtime/context/publisher.hpp>
#include <vita/profiles/iq/source.hpp>
#include <vita/adapters/posix_udp/socket.hpp>
#include <charconv>
#include <string_view>
namespace vita::bench::receiver {
inline constexpr std::uint32_t fixture_oui=0xabcdef;
inline constexpr std::size_t pairs=256,payload_bytes=1024;
inline Result<adapters::posix_udp::Address> address(std::string_view ip,std::uint16_t port) noexcept {
  adapters::posix_udp::Address out;out.family=adapters::posix_udp::Family::ipv4;out.port=port;
  for(unsigned i=0;i<4;++i){auto dot=ip.find('.');auto part=dot==ip.npos?ip:ip.substr(0,dot);unsigned value=0;auto parsed=std::from_chars(part.data(),part.data()+part.size(),value);if(part.empty()||parsed.ec!=std::errc{}||parsed.ptr!=part.data()+part.size()||value>255||(i<3)==(dot==ip.npos))return std::unexpected(Error{ErrorCode::invalid_argument});out.bytes[i]=std::byte(value);if(dot!=ip.npos)ip.remove_prefix(dot+1);}
  return out;
}
inline runtime::timing::ProtocolTime sample_time(std::uint64_t ordinal,std::uint64_t rate) noexcept {
  return {1000+ordinal/rate,static_cast<std::uint64_t>((static_cast<unsigned __int128>(ordinal%rate)*1000000000000ULL)/rate)};
}
inline Result<std::uint64_t> sample_ordinal(runtime::timing::ProtocolTime time,std::uint64_t rate) noexcept {
  if(time.seconds<1000||!runtime::timing::valid(time)||!rate)return std::unexpected(Error{ErrorCode::invalid_argument});
  auto ordinal=static_cast<unsigned __int128>(time.seconds-1000)*rate+(static_cast<unsigned __int128>(time.picoseconds)*rate+999999999999ULL)/1000000000000ULL;
  if(ordinal>UINT64_MAX-pairs)return std::unexpected(Error{ErrorCode::overflow});return static_cast<std::uint64_t>(ordinal);
}
inline std::uint64_t checksum(Bytes bytes) noexcept {std::uint64_t value=14695981039346656037ULL;for(auto byte:bytes)value=(value^std::to_integer<unsigned>(byte))*1099511628211ULL;return value;}
inline Result<void> canonical(MutableBytes bytes) noexcept {
  runtime::StateSnapshot state;auto window=profiles::iq::SampleWriteWindow::create(bytes,profiles::iq::SampleFormat::iq16,0,pairs,state);if(!window)return std::unexpected(window.error());return profiles::iq::default_source().produce(*window);
}
inline codec::Envelope envelope(std::uint32_t sid,codec::PacketType type,unsigned count) noexcept {
  codec::Envelope out;out.type=type;out.stream_id=sid;out.packet_count=count;out.class_id=codec::ClassId{fixture_oui,1,static_cast<std::uint16_t>(type==codec::PacketType::context?0x10:1)};return out;
}
inline Result<std::size_t> data_packet(std::uint32_t sid,unsigned count,std::uint64_t ordinal,std::uint64_t rate,Bytes payload,MutableBytes output) noexcept {
  auto packet=envelope(sid,codec::PacketType::signal,count);auto time=sample_time(ordinal,rate);if(time.seconds>UINT32_MAX)return std::unexpected(Error{ErrorCode::overflow});packet.timestamp={codec::Tsi::gps,codec::Tsf::picoseconds,static_cast<std::uint32_t>(time.seconds),time.picoseconds};return codec::encode_envelope(packet,payload,{},output);
}
inline Result<std::size_t> context_packet(std::uint32_t sid,unsigned count,std::uint64_t ordinal,std::uint64_t rate,MutableBytes output) noexcept {
  runtime::context::ContextFrame frame;frame.time=sample_time(ordinal,rate);frame.epoch=codec::Tsi::gps;frame.time_known=frame.valid=true;frame.refresh=count!=0;
  for(auto& field:frame.state.fields)field.validity=runtime::Validity::known;
  frame.state.fields[0].value=sid;frame.state.fields[1].value=Hertz{static_cast<std::int64_t>(rate)<<20};frame.state.fields[2].value=std::uint32_t{0};frame.state.fields[3].value=profiles::iq::payload_format(profiles::iq::SampleFormat::iq16);
  return runtime::context::encode_context(frame,envelope(sid,codec::PacketType::context,count),output);
}
} // namespace vita::bench::receiver
