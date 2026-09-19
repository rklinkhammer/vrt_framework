#pragma once
#include <vita/adapters/posix_udp/udp.hpp>
#include "../P10/runtime_fixture.hpp"
#include "peer.hpp"
namespace verify_p12 {
using namespace vita;using namespace vita::runtime;using namespace vita::adapters::posix_udp;
inline Config config(bool v6=false){Config c;for(auto& s:c.sockets)s.bind=Address::loopback(v6?Family::ipv6:Family::ipv4);return c;}
inline std::array<memory::ExternalPool,3> pools(){return{verify_p10::external_pool(2048,8),verify_p10::external_pool(2048,8),verify_p10::external_pool(2048,8)};}
inline PeerBinding binding(const Peer& peer,bool v6=false){PeerBinding p;p.local_source={7,1};p.remote_source={9,1};for(auto&a:p.remote)a=Address::loopback(v6?Family::ipv6:Family::ipv4,peer.port());return p;}
inline memory::TxStorage storage(memory::ExternalPool& pool,Bytes bytes){memory::TxStorage result;const auto first=std::min(std::size_t{8},bytes.size());for(auto part:{bytes.first(first),bytes.subspan(first)}){if(part.empty())continue;auto lease=pool.acquire({part.size()});if(!lease||!lease->set_size(part.size()))std::abort();auto target=lease->writable_bytes();if(!target)std::abort();std::memcpy(target->data(),part.data(),part.size());if(!result.append(std::move(*lease),0,part.size()))std::abort();}return result;}
struct Capture {unsigned calls=0;codec::Envelope envelope{};std::array<std::byte,32> payload{};std::size_t bytes=0;memory::RetentionQuota local{1},global{1};std::optional<memory::RetainedRx> held;bool retain=false;
 static void receive(void*p,const codec::PacketView& packet,const memory::RxEnvelope& rx)noexcept {auto& c=*static_cast<Capture*>(p);++c.calls;c.envelope=packet.envelope.envelope;c.bytes=packet.envelope.payload.size();if(c.bytes<=c.payload.size())std::memcpy(c.payload.data(),packet.envelope.payload.data(),c.bytes);if(c.retain&&!c.held){auto held=rx.retain(c.local,c.global);if(!held)std::abort();c.held=std::move(*held);}}
};
inline Route route(Capture& capture,codec::PacketType type=codec::PacketType::signal){Route r;r.key.source={9,1};r.key.stream_id=0x01020304;r.key.type=type;r.context=&capture;r.receive=Capture::receive;return r;}
} // namespace verify_p12
