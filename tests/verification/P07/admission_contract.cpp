#include <vita/runtime/transaction/manager.hpp>
#include "packets.hpp"
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transaction;using namespace verify_p07;
int main(){
    auto original=make(false);auto cancel=cancellation();
    for(unsigned unavailable=0;unavailable<2;++unavailable){
        auto capacities=AdmissionPool::reference_capacities();capacities.need(unavailable?Resource::cancellation_response:Resource::cancellation_queue,unavailable?1:0);AdmissionPool pool(capacities);VirtualBackend<> backend;StateSnapshot initial;EngineOptions options;options.profile=Profile::generic_virtual_test;options.external_retention=true;Engine<1> engine(pool,backend.binding(),initial,options);RetentionStore<2,8192> store(pool);TransactionManager<1,2,8192> manager(engine,store,pool);
        if(!manager.accept(original.view(),{}, {9,1})||!manager.progress({}))return 1;auto before=pool.used(Resource::duplicate_bytes);
        if(manager.accept(cancel.view(),{}, {9,1})||backend.pending()!=1||backend.begins()!=1||backend.writes()||pool.used(Resource::duplicate_bytes)!=before||pool.used(Resource::cancellation_queue)||pool.used(Resource::cancellation_response))return 2;
        if(!backend.complete_next()||!manager.progress({})||backend.writes()!=1)return 3;
    }
    {
        AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;StateSnapshot initial;EngineOptions options;options.profile=Profile::generic_virtual_test;options.external_retention=true;Engine<1> engine(pool,backend.binding(),initial,options);
        constexpr std::size_t capacity=40+3*sizeof(AckRecord)+4;RetentionStore<2,capacity> store(pool);TransactionManager<1,2,capacity> manager(engine,store,pool);
        if(!manager.accept(original.view(),{}, {9,1})||!manager.progress({}))return 4;auto before=pool.used(Resource::duplicate_bytes);
        if(manager.accept(cancel.view(),{}, {9,1})||backend.pending()!=1||backend.begins()!=1||backend.writes()||pool.used(Resource::duplicate_bytes)!=before||pool.used(Resource::cancellation_queue)||pool.used(Resource::cancellation_response))return 5;
    }
    return 0;
}
