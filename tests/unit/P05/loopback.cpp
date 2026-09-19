#include "support.hpp"
int main(){
    using namespace p05_test;
    auto txpool=pool(),rxpool=pool(),controlpool=pool(),cancelpool=pool();Consumer consumer;runtime::RouteRegistry<8> routes;runtime::CounterRegistry<8> counters;register_routes(routes,counters,consumer);
    runtime::CompletionArena<16> tickets;runtime::AdmissionPool admission(runtime::AdmissionPool::reference_capacities());Loopback<4,8,8> transport(rxpool,controlpool,cancelpool,admission,routes,counters);
    auto first=make(txpool,tickets,counters,1);auto sent=transport.try_send(std::move(first));assert(sent&&consumer.calls==0);
    assert(*counters.next({7,1,codec::PacketType::signal})==1);assert(transport.progress(*sent));assert(consumer.calls==1&&consumer.counts[0]==0);
    std::size_t completions=0;assert(tickets.scan([&](runtime::CompletionRecord r) noexcept {assert(r.operation==1&&r.result.status==runtime::CompletionStatus::succeeded);++completions;})==1);
    auto rejected=transport.try_send(make(txpool,tickets,counters,2,Fault{true}));assert(!rejected&&rejected.error().submission.storage.segment_count()==1);assert(*counters.next({7,1,codec::PacketType::signal})==1);assert(tickets.scan([](auto) noexcept{})==0);
    rejected.error().submission.fault={};auto retried=transport.try_send(std::move(rejected.error().submission));assert(retried);assert(transport.progress(*retried));assert(tickets.scan([](auto) noexcept{})==1);
    auto a=transport.try_send(make(txpool,tickets,counters,3));auto b=transport.try_send(make(txpool,tickets,counters,4,Fault{false,false,true}));assert(a&&b);assert(transport.progress(*b));assert(transport.progress(*a));assert(consumer.counts[2]==3&&consumer.counts[3]==3&&consumer.counts[4]==2);assert(tickets.scan([](auto) noexcept{})==2);
    auto lost=transport.try_send(make(txpool,tickets,counters,5,Fault{false,true}));assert(lost);assert(transport.progress(*lost));assert(consumer.calls==5);assert(tickets.scan([](runtime::CompletionRecord r) noexcept {assert(r.result.status==runtime::CompletionStatus::succeeded);})==1);
    auto failure=transport.try_send(make(txpool,tickets,counters,6,Fault{false,false,false,true,true}));assert(failure);auto returns=txpool.return_count();assert(transport.progress(*failure));assert(txpool.return_count()==returns&&transport.outstanding()==1);assert(tickets.scan([](runtime::CompletionRecord r) noexcept {assert(r.result.status==runtime::CompletionStatus::failed);})==1);assert(transport.prove_quiescent(*failure));assert(txpool.return_count()==returns+1&&transport.outstanding()==0);assert(!transport.prove_quiescent(*failure));
    // Both chains contain the same payload/trailer; count is the only intentional header difference.
    auto contiguous=transport.try_send(make(txpool,tickets,counters,7));assert(contiguous&&transport.progress(*contiguous));auto bytes=consumer.logical;
    auto segmented=transport.try_send(make(txpool,tickets,counters,8,{},true));assert(segmented&&transport.progress(*segmented));for(std::size_t i=4;i<20;++i)assert(bytes[i]==consumer.logical[i]);assert(tickets.scan([](auto) noexcept{})==2);
    for(unsigned i=0;i<10;++i){auto next=transport.try_send(make(txpool,tickets,counters,9+i));assert(next&&transport.progress(*next));tickets.scan([](auto) noexcept{});}assert(*counters.next({7,1,codec::PacketType::signal})==2);
    assert(!routes.add({{{44,1},1,codec::PacketType::context},&consumer,Consumer::receive}));
    runtime::RouteRegistry<4> duplicate;runtime::Route route{{{44,1},std::nullopt,codec::PacketType::signal_without_sid},&consumer,Consumer::receive};assert(duplicate.add(route));assert(!duplicate.add(route));duplicate.freeze();codec::Envelope no_sid{};no_sid.type=codec::PacketType::signal_without_sid;assert(duplicate.lookup({44,1},no_sid));assert(!duplicate.lookup({44,2},no_sid));
    transport.close();auto closed=transport.try_send(make(txpool,tickets,counters,20));assert(!closed&&closed.error().submission.storage.byte_size()==20);
    return 0;
}
