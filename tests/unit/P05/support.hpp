#pragma once
#include <vita/adapters/loopback/loopback.hpp>
#include <vita/codec/samples.hpp>
#include <cassert>
#include <memory>
#include <array>
#include <optional>
namespace p05_test {
using namespace vita;
using namespace vita::adapters::loopback;
inline memory::ExternalPool pool() {
    auto storage=std::make_shared<std::array<std::byte,512*32>>();
    memory::BufferSpec spec{storage,storage->data(),512,32,1};auto result=memory::ExternalPool::create({&spec,1});assert(result);return std::move(*result);
}
struct Consumer {
    std::array<std::uint8_t,64> counts{};std::size_t calls=0;
    std::array<std::byte,32> logical{};std::size_t logical_size=0;
    memory::RetentionQuota local{4},global{8};std::optional<memory::RetainedRx> retained;
    bool should_retain=false;
    static void receive(void* context,const codec::PacketView& view,const memory::RxEnvelope& rx) noexcept {
        auto& self=*static_cast<Consumer*>(context);self.counts[self.calls++]=view.envelope.envelope.packet_count;
        self.logical_size=view.envelope.wire.size();assert(self.logical_size<=self.logical.size());std::copy(view.envelope.wire.begin(),view.envelope.wire.end(),self.logical.begin());
        if(self.should_retain){auto r=rx.retain(self.local,self.global);assert(r);self.retained.emplace(std::move(*r));}
    }
};
inline TxSubmission make(memory::ExternalPool& pool,runtime::CompletionArena<16>& tickets,runtime::CounterRegistry<8>& counters,std::uint64_t operation,Fault fault={},bool segmented=false) {
    const runtime::CounterKey key{7,1,codec::PacketType::signal};auto count=counters.next(key);assert(count);
    auto block=pool.acquire({20});assert(block);auto output=block->writable_bytes();assert(output);
    const std::array<std::byte,8> iq{std::byte{0x40},std::byte{0},std::byte{0},std::byte{0},std::byte{0x3b},std::byte{0x21},std::byte{0x18},std::byte{0x7e}};
    codec::Envelope envelope{};envelope.stream_id=1;envelope.packet_count=*count;envelope.trailer=true;
    auto encoded=codec::encode_envelope(envelope,iq,0xc00c0000,*output);assert(encoded&&*encoded==20);assert(block->set_size(*encoded));
    memory::TxStorage storage;
    if(segmented){std::size_t offset=0;for(auto length:{8u,8u,4u}){auto part=pool.acquire({length});assert(part);auto bytes=part->writable_bytes();assert(bytes);std::copy(output->begin()+offset,output->begin()+offset+length,bytes->begin());assert(part->set_size(length));assert(storage.append(std::move(*part),0,length));offset+=length;}}
    else assert(storage.append(std::move(*block),0,*encoded));
    auto ticket=tickets.reserve(operation);assert(ticket);
    return TxSubmission{std::move(storage),std::move(*ticket),{44,1},key,fault};
}
inline void register_routes(runtime::RouteRegistry<8>& routes,runtime::CounterRegistry<8>& counters,Consumer& consumer){
    runtime::Route route{{{44,1},1,codec::PacketType::signal},&consumer,Consumer::receive};assert(routes.add(route));routes.freeze();assert(counters.add({7,1,codec::PacketType::signal}));counters.freeze();
}
} // namespace p05_test
