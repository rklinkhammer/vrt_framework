#include <vita/runtime/transaction/manager.hpp>
#include <vita/runtime/transaction/controller.hpp>
#include <cassert>
#include <cstdlib>
#include <new>
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transaction;
static std::size_t allocations=0;
void* operator new(std::size_t n){++allocations;if(auto p=std::malloc(n?n:1))return p;std::abort();}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete(void* p) noexcept{std::free(p);}void operator delete[](void* p) noexcept{std::free(p);}
StateSnapshot state(){StateSnapshot s;for(auto id:baseline_fields)s.fields[field_index(id)].validity=Validity::known;s.fields[0].value=std::uint32_t{1};s.fields[1].value=*Hertz::from_integer(1000000);s.fields[2].value=std::uint32_t{0};s.fields[3].value=PayloadFormat{0x200003cf00000000ULL};return s;}
codec::PacketView packet(std::array<std::byte,512>& wire,std::uint32_t mid=1,bool cancel=false){codec::Envelope e;e.type=codec::PacketType::command;e.stream_id=1;e.cancel=cancel;e.command=codec::Command{0xa90c0000,mid,codec::Identifier::short_id(2),codec::Identifier::short_id(3)};Result<std::size_t> n;
 if(cancel){CancelPacket p;assert(p.select<SampleRate>());n=codec::encode_packet(e,p.freeze(),wire);}else{ControlPacket p;assert(p.set<SampleRate>(*Hertz::from_integer(2000000)));n=codec::encode_packet(e,p.freeze(),wire);}assert(n);auto parsed=codec::decode_packet(Bytes{wire}.first(*n));assert(parsed);return std::move(*parsed);}
int main(){
 AdmissionPool admission(AdmissionPool::reference_capacities());VirtualBackend<4> backend;Engine<2> engine(admission,backend.binding(),state(),{Profile::iq_generator_v1,{},nullptr,true});RetentionStore<8,32768> retention(admission);TransactionManager<2,8,32768> manager(engine,retention,admission);ControllerRegistry<4> controller;
 std::array<std::byte,512> wire{};auto original=packet(wire);OperationContext now;now.operation=1;auto key=transaction_key(original,1,{7,1});assert(key);auto relation=controller.register_relationship(*key);assert(relation);auto tracked=controller.track(*relation,original,{0},100);assert(tracked&&controller.association_retained(*key));
 const auto baseline=allocations;
 auto accepted=manager.accept(original,now,{7,1});assert(accepted);assert(manager.progress(now));assert(backend.pending()==1);
 auto capability=backend.pending_capability();assert(capability);assert(manager.request_quiesce(now));assert(backend.pending()==0&&manager.drain_status().active==0);assert(engine.drain_status().capability_holders==1&&!engine.safe_to_reset());
 auto unchanged=engine.state();assert(!engine.reset_state(state(),2));assert(engine.state().version==unchanged.version);
 auto replay=manager.accept(original,now,{7,1});assert(replay&&replay->kind==DuplicateKind::replay);assert(manager.release(replay->token));
 std::array<std::byte,512> other_wire;assert(!manager.accept(packet(other_wire,2),now,{7,1}));assert(!manager.accept(packet(other_wire,1,true),now,{7,1}));assert(backend.begins()==1&&backend.writes()==0);
 auto response=manager.response(accepted->token,0);assert(response&&*response&&(**response).kind==AckKind::execution&&!(**response).scheduled_or_executed);
 assert(retention.association_retained(*key));assert(manager.release(accepted->token));retention.expire({30000000000ULL});assert(!retention.association_retained(*key));
 FieldOutcome late;late.id=SampleRate::id;late.status=FieldStatus::executed;late.validity=Validity::known;late.value=*Hertz::from_integer(3000000);assert(!capability->complete(late));assert(engine.progress(now));assert(engine.faulted());capability.reset();assert(engine.safe_to_reset());
 auto bad=state();bad.fields[1].value=std::uint32_t{7};assert(!engine.reset_state(bad,2));assert(engine.faulted());assert(engine.reset_state(state(),2));assert(manager.reset_after_drain());assert(!engine.faulted()&&!engine.quiescing());
 now.association_generation=2;now.operation=2;auto again=manager.accept(original,now,{7,2});assert(again);assert(manager.progress(now));auto new_capability=backend.pending_capability();assert(new_capability);backend.set_reversible(false);backend.set_quiescence_available(false);assert(manager.request_quiesce(now));assert(engine.drain_status().running==1&&!engine.drain_status().backend.known&&!engine.safe_to_reset());assert(!backend.reinitialize());backend.set_quiescence_available(true);assert(backend.reinitialize());assert(manager.progress(now));assert(!engine.safe_to_reset());new_capability.reset();assert(engine.safe_to_reset());assert(manager.release(again->token));
 assert(controller.release(tracked->handle));assert(controller.advance({100}));controller.expire({30000000100ULL});assert(!controller.association_retained(*key));auto fresh=*key;fresh.stream_id=2;fresh.binding_generation=2;fresh.peer.generation=2;assert(controller.can_register_relationship(fresh));assert(!controller.can_register_relationship(TransactionKey{2,{7,2},1,key->controller,key->controllee,0}));
 assert(allocations==baseline);
 // Holder absence alone cannot prove backend quiescence.
 auto missing=backend.binding();missing.quiescence=nullptr;Engine<1> no_evidence(admission,missing,state());assert(no_evidence.drain_status().capability_holders==0&&!no_evidence.safe_to_reset());assert(!no_evidence.reset_state(state(),2));
 {
  VirtualBackend<1> future_backend;Engine<1> future(admission,future_backend.binding(),state());
  OperationContext clock;clock.clock={timing::ClockState::locked,{100,0},0,1,true,true,timing::Epoch::gps};clock.timing=timing::TimingCapabilities::deterministic();
  std::array<timing::Boundary,1> boundaries{{{{110,0},1000,1,false,true,0}}};clock.boundaries=boundaries;
  codec::Envelope envelope;envelope.type=codec::PacketType::command;envelope.stream_id=1;envelope.timestamp={codec::Tsi::gps,codec::Tsf::picoseconds,110,0};envelope.command=codec::Command{0xa9081000,4,codec::Identifier::short_id(2),codec::Identifier::short_id(3)};ControlPacket control;assert(control.set<SampleRate>(*Hertz::from_integer(2000000)));auto size=codec::encode_packet(envelope,control.freeze(),wire);assert(size);auto request=codec::decode_packet(Bytes{wire}.first(*size));assert(request);auto handle=future.accept(*request,clock);assert(handle);assert(future.request_quiesce(clock));assert(future_backend.begins()==0&&*future.complete(*handle));auto ack=future.take_response(*handle);assert(ack&&*ack&&(**ack).kind==AckKind::execution&&(**ack).timing==7&&!(**ack).time_known);assert(future.release(*handle)&&future.safe_to_reset());
 }

}
