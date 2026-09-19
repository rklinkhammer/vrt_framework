#include "../../unit/P05/support.hpp"
int main(){
    using namespace p05_test;
    auto txpool=pool(),rxpool=pool(),controlpool=pool(),cancelpool=pool();Consumer consumer;consumer.should_retain=true;
    runtime::RouteRegistry<8> routes;runtime::CounterRegistry<8> counters;register_routes(routes,counters,consumer);
    runtime::CompletionArena<16> tickets;runtime::AdmissionPool admission(runtime::AdmissionPool::reference_capacities());
    {
        Loopback<4,8,8> transport(rxpool,controlpool,cancelpool,admission,routes,counters);
        auto submitted=transport.try_send(make(txpool,tickets,counters,1));assert(submitted&&consumer.calls==0&&txpool.return_count()==0);
        assert(transport.progress_next()&&consumer.calls==1);assert(txpool.return_count()==1&&rxpool.return_count()==0);
        assert(consumer.retained&&consumer.retained->fragment_count()==1);
        auto bytes=consumer.retained->fragment(0);assert(bytes&&bytes->size()==8);auto samples=codec::SampleView<std::int16_t>::create(*bytes);assert(samples&&samples->size()==2&&samples->at(0)->i==16384);
        assert(tickets.scan([](auto record) noexcept{assert(record.result.status==runtime::CompletionStatus::succeeded);})==1);
        assert(admission.used(runtime::Resource::ordinary_queue)==0&&admission.used(runtime::Resource::completion)==0);
    }
    assert(consumer.retained->fragment(0)&&rxpool.return_count()==0);consumer.retained.reset();assert(rxpool.return_count()==1&&consumer.local.active()==0&&consumer.global.active()==0);
    return 0;
}
