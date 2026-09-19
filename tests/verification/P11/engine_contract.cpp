#include <vita/runtime/transaction/engine.hpp>
#include "../P07/packets.hpp"
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transaction;using namespace verify_p07;
StateSnapshot initial_state(){StateSnapshot state;state.fields[0].validity=Validity::known;state.fields[1].validity=Validity::known;state.fields[1].value=*Hertz::from_integer(1);return state;}
template<class E,class H> bool drain(E& engine,H handle){for(unsigned n=0;n<8;++n){auto response=engine.take_response(handle);if(!response)return false;if(!*response)return true;}return false;}
int main(){
 EngineOptions options;options.profile=Profile::generic_virtual_test;
 {
  AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;auto binding=backend.binding();binding.quiescence=nullptr;Engine<2> engine(pool,binding,initial_state(),options);
  if(engine.drain_status().capability_holders||engine.safe_to_reset()||engine.reset_state(initial_state(),2))return 1;
 }
 {
  AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;Engine<2> engine(pool,backend.binding(),initial_state(),options);
  auto packet=make(false,42,2);auto old=engine.accept(packet.view(),{});if(!old||!engine.progress({}))return 2;
  auto capability=backend.pending_capability();if(!capability||!engine.request_quiesce({})||backend.pending()||engine.accept(packet.view(),{}))return 3;
  if(!drain(engine,*old)||!engine.release(*old)||engine.safe_to_reset()||engine.reset_state(initial_state(),2))return 4;
  if(engine.drain_status().capability_holders!=1||!engine.drain_status().backend.quiescent)return 5;
  capability.reset();backend.set_quiescence_available(false);
  if(engine.safe_to_reset()||engine.reset_state(initial_state(),2))return 6;
  backend.set_quiescence_available(true);auto malformed=initial_state();malformed.fields[1].value=std::uint32_t{5};
  if(engine.reset_state(malformed,2)||engine.association_generation()!=1||!engine.quiescing())return 7;
  if(!engine.safe_to_reset()||!engine.reset_state(initial_state(),2)||engine.quiescing()||engine.association_generation()!=2)return 8;
  OperationContext current;current.association_generation=2;auto fresh=engine.accept(packet.view(),current);
  if(!fresh||fresh->generation==old->generation||engine.release(*old))return 9;
 }
 {
  AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;backend.set_reversible(false);Engine<2> engine(pool,backend.binding(),initial_state(),options);
  auto packet=make(false,42,2);auto handle=engine.accept(packet.view(),{});if(!handle||!engine.progress({}))return 10;
  auto capability=backend.pending_capability();if(!capability||!capability->synthetic_failure()||!engine.progress({})||!drain(engine,*handle)||!engine.release(*handle))return 11;
  capability.reset();if(!backend.pending()||engine.safe_to_reset()||engine.reset_state(initial_state(),2))return 12;
  if(!backend.reinitialize()||!engine.safe_to_reset()||!engine.reset_state(initial_state(),2))return 13;
 }
 return 0;
}
