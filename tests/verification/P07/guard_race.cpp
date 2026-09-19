#include <vita/runtime/transaction/manager.hpp>
#include "packets.hpp"
#include <atomic>
#include <thread>
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transaction;using namespace verify_p07;
int main(){
    for(unsigned iteration=0;iteration<100;++iteration){
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;StateSnapshot initial;initial.fields[0].validity=Validity::known;initial.fields[1].validity=Validity::known;initial.fields[1].value=*Hertz::from_integer(1);
        EngineOptions options;options.external_retention=true;options.profile=Profile::generic_virtual_test;Engine<2> engine(pool,backend.binding(),initial,options);RetentionStore<4,16384> store(pool);TransactionManager<2,4,16384> manager(engine,store,pool);
        auto original=make(false,42,2);auto a=manager.accept(original.view(),{}, {9,1});if(!a||!manager.progress({}))return 1;
        auto capability=backend.pending_capability();if(!capability)return 2;
        auto cancellation_packet=cancellation(42,2);auto c=manager.accept(cancellation_packet.view(),{}, {9,1});if(!c||!manager.progress({})||backend.pending())return 3;
        std::atomic<bool> start=false,published=false;
        std::thread producer([completion=std::move(*capability),&start,&published]() mutable {
            while(!start.load(std::memory_order_acquire))std::this_thread::yield();
            FieldOutcome contradictory;contradictory.id=SampleRate::id;contradictory.status=FieldStatus::executed;contradictory.validity=Validity::known;contradictory.value=*Hertz::from_integer(99);
            published=completion.complete(contradictory);completion={};
        });capability.reset();
        auto next=make(false,43,1);start.store(true,std::memory_order_release);auto admitted=manager.accept(next.view(),{}, {9,1});(void)admitted;
        producer.join();if(published||!manager.progress({})||!engine.faulted()||engine.state().fields[1].validity!=Validity::unknown||engine.state().fields[0].validity!=Validity::known||std::get<std::uint32_t>(engine.state().fields[0].value)!=0||backend.begins()!=1||backend.writes())return 4;
    }
    return 0;
}
