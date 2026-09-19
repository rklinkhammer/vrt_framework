#include "support.hpp"
namespace {
using Transport=vita::adapters::loopback::Loopback<4,8,8>;
struct GuardConsumer {
    Transport* transport=nullptr;unsigned calls=0,blocked=0;
    static void receive(void* c,const vita::codec::PacketView&,const vita::memory::RxEnvelope&) noexcept {
        auto& self=*static_cast<GuardConsumer*>(c);++self.calls;
        auto nested=self.transport->progress_next();if(!nested&&nested.error().code==vita::ErrorCode::would_deadlock)++self.blocked;
    }
};
}
int main(){
    using namespace p05_test;
    auto txpool=pool(),data_pool=pool(),control_pool=pool(),cancel_pool=pool();
    runtime::RouteRegistry<8> routes;runtime::CounterRegistry<8> counters;GuardConsumer receiver;
    assert(routes.add({{{44,1},1,codec::PacketType::signal},&receiver,GuardConsumer::receive}));
    runtime::Route command{{{44,1},1,codec::PacketType::command,{},codec::Identifier::short_id(2),codec::Identifier::short_id(3)},&receiver,GuardConsumer::receive};assert(routes.add(command));routes.freeze();
    assert(counters.add({7,1,codec::PacketType::signal}));assert(counters.add({7,1,codec::PacketType::command}));counters.freeze();
    runtime::CompletionArena<16> tickets;runtime::AdmissionPool admission(runtime::AdmissionPool::reference_capacities());Transport transport(data_pool,control_pool,cancel_pool,admission,routes,counters);receiver.transport=&transport;
    auto first=transport.try_send(make(txpool,tickets,counters,1));auto second=transport.try_send(make(txpool,tickets,counters,2));assert(first&&second);
    auto full=transport.try_send(make(txpool,tickets,counters,3));assert(!full);assert(admission.used(runtime::Resource::data_queue)==2&&admission.used(runtime::Resource::ordinary_queue)==0);
    auto command_submission=[&](bool cancel,std::uint64_t operation){
        runtime::CounterKey key{7,1,codec::PacketType::command};auto count=counters.next(key);assert(count);
        auto lease=txpool.acquire({64});assert(lease);auto bytes=lease->writable_bytes();assert(bytes);
        codec::Envelope envelope{};envelope.type=codec::PacketType::command;envelope.stream_id=1;envelope.packet_count=*count;envelope.cancel=cancel;envelope.command=codec::Command{cancel?0xa9080000u:0xa0040000u,static_cast<std::uint32_t>(operation),codec::Identifier::short_id(2),codec::Identifier::short_id(3)};
        Result<std::size_t> encoded;
        if(cancel){CancelPacket q;assert(q.select<SampleRate>());encoded=codec::encode_packet(envelope,q.freeze(),*bytes);}else{QueryPacket q;assert(q.select<SampleRate>());encoded=codec::encode_packet(envelope,q.freeze(),*bytes);}
        assert(encoded&&lease->set_size(*encoded));memory::TxStorage storage;assert(storage.append(std::move(*lease),0,*encoded));auto ticket=tickets.reserve(operation);assert(ticket);
        return TxSubmission{std::move(storage),std::move(*ticket),{44,1},key,{}};
    };
    auto control=transport.try_send(command_submission(false,4));assert(control);
    auto cancel=transport.try_send(command_submission(true,5));assert(cancel);
    assert(admission.used(runtime::Resource::ordinary_queue)==1&&admission.used(runtime::Resource::cancellation_queue)==1);
    assert(transport.progress(*cancel));assert(transport.progress(*control));assert(transport.progress(*first));assert(transport.progress(*second));assert(receiver.calls==4&&receiver.blocked==4);
    assert(tickets.scan([](auto r) noexcept{assert(r.result.status==runtime::CompletionStatus::succeeded);})==4);
    assert(admission.used(runtime::Resource::data_queue)==0&&admission.used(runtime::Resource::ordinary_queue)==0&&admission.used(runtime::Resource::cancellation_queue)==0);
    Transport invalid(data_pool,data_pool,cancel_pool,admission,routes,counters);auto rejected=invalid.try_send(make(txpool,tickets,counters,6));assert(!rejected&&rejected.error().error.code==ErrorCode::invalid_argument);
    return 0;
}
