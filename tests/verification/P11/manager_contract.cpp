#include <vita/runtime/transaction/manager.hpp>
#include "../P07/packets.hpp"
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transaction;using namespace verify_p07;
int main(){
 AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;
 StateSnapshot state;state.fields[0].validity=Validity::known;state.fields[1].validity=Validity::known;state.fields[1].value=*Hertz::from_integer(1);
 EngineOptions options;options.external_retention=true;options.profile=Profile::generic_virtual_test;
 Engine<2> engine(pool,backend.binding(),state,options);RetentionStore<8,32768> store(pool);TransactionManager<2,8,32768> manager(engine,store,pool);
 auto original=make(false,42,2);auto accepted=manager.accept(original.view(),{}, {9,1});if(!accepted||!manager.progress({}))return 1;
 auto relationship=transaction_key(original.view(),1,{9,1});if(!relationship||!store.association_retained(*relationship))return 2;
 if(!manager.request_quiesce({})||manager.drain_status().active||backend.pending()||!engine.safe_to_reset())return 3;
 auto retry=manager.accept(original.view(),{}, {9,1});if(!retry||retry->kind!=DuplicateKind::replay)return 4;
 auto changed=make(false,42,2,7);if(manager.accept(changed.view(),{}, {9,1}))return 5;
 auto fresh=make(false,43,2);if(manager.accept(fresh.view(),{}, {9,1})||manager.accept(cancellation(42,2).view(),{}, {9,1})||backend.begins()!=1||backend.writes())return 6;
 if(manager.reset_after_drain()||!engine.reset_state(state,2)||!manager.reset_after_drain())return 7;
 OperationContext current;current.association_generation=2;
 // A stale lifecycle request must not close the replacement association.
 if(manager.request_quiesce({}))return 8;
 auto next=manager.accept(fresh.view(),current,{9,1});if(!next||next->kind!=DuplicateKind::fresh)return 9;
 if(!store.association_retained(*relationship))return 10;
 return 0;
}
