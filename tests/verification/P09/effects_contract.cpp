#include <vita/runtime/context/publisher.hpp>
#include <vita/runtime/transaction/engine.hpp>
#include <cstdlib>
#include "../P07/packets.hpp"
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::context;using namespace vita::runtime::transaction;
static StateSnapshot initial(){StateSnapshot s;for(auto id:baseline_fields)s.fields[field_index(id)].validity=Validity::known;s.fields[0].value=std::uint32_t{1};s.fields[1].value=*Hertz::from_integer(1000000);s.fields[2].value=std::uint32_t{valid_data_enable|valid_data_indicator};s.fields[3].value=PayloadFormat{0x200003cf00000000ULL};return s;}
static EffectiveEvent beginning(){EffectiveEvent e;e.state=initial();e.actual_time={100,0};e.time_known=e.ordinal_known=true;e.association_generation=1;return e;}
static AdmissionBundle credit(AdmissionPool& pool){return std::move(*pool.acquire(AdmissionRequest{}.need(Resource::revision).need(Resource::context_publication)));}
static Result<Handle> submit(Engine<2>& engine,const ControlPacket& packet,OperationContext now){codec::Envelope e;e.type=codec::PacketType::command;e.stream_id=1;e.command=codec::Command{0xa91c0000,1,codec::Identifier::short_id(2),codec::Identifier::short_id(3)};std::array<std::byte,256> bytes;auto size=codec::encode_packet(e,packet.freeze(),bytes);if(!size)return std::unexpected(size.error());auto decoded=codec::decode_packet(Bytes{bytes}.first(*size));if(!decoded)return std::unexpected(decoded.error());return engine.accept(*decoded,now);}
struct Published{std::array<ContextFrame,8> frames;unsigned count=0,data=0;bool reject=false;static Result<void> send(void* p,const ContextFrame& f)noexcept{auto&s=*static_cast<Published*>(p);if(s.reject)return std::unexpected(Error{ErrorCode::capacity_exhausted});if(s.count==8)std::abort();s.frames[s.count++]=f;return {};}static Result<void> data_send(void* p,memory::TxStorage& tx,const RevisionHandle&)noexcept{++static_cast<Published*>(p)->data;tx={};return {};}PublisherBinding binding(){return{this,send,data_send};}};
static FieldOutcome outcome(FieldId id,SemanticValue value,unsigned millis,FieldStatus status=FieldStatus::executed){FieldOutcome f;f.id=id;f.value=value;f.status=status;f.validity=status==FieldStatus::unknown_effect?Validity::unknown:Validity::known;f.actual_time={100,std::uint64_t(millis)*1000000000};f.time_known=f.ordinal_known=true;f.sample_ordinal=millis*1000;return f;}
int main(){
 for(unsigned scenario=6;scenario<=9;++scenario){
  AdmissionPool pool(AdmissionPool::reference_capacities());RevisionStore<8> store;auto first=store.initial(beginning(),credit(pool));if(!first)return 1;Published sent;ContextPublisher<8> publisher(store,sent.binding());if(!publisher.start({0})||!publisher.progress({0},{100,0},codec::Tsi::gps,true))return 2;
  VirtualBackend<> backend;Engine<2> engine(pool,backend.binding(),initial(),{Profile::generic_virtual_test,store.binding()});OperationContext now;now.clock={timing::ClockState::locked,{100,0},0,1,true,true,timing::Epoch::gps};now.timing=timing::TimingCapabilities::deterministic();
  ControlPacket control;if(scenario==9)control.set<ReferencePoint>(2);control.set<SampleRate>(*Hertz::from_integer(2000000));auto h=submit(engine,control,now);if(!h||!engine.progress(now)||backend.pending()!=1)return 3;
  const auto changed=scenario==9?ReferencePoint::id:SampleRate::id;SemanticValue value=scenario==9?SemanticValue{std::uint32_t{2}}:SemanticValue{*Hertz::from_integer(2000000)};
  if(!backend.complete_next(outcome(changed,value,10,scenario==7?FieldStatus::unknown_effect:FieldStatus::executed)))return 4;
  now.monotonic={10000000};now.clock.time={100,10000000000ULL};if(!engine.progress(now))return 5;
  if(scenario==9){
   if(!publisher.progress(now.monotonic,now.clock.time,codec::Tsi::gps,true)||sent.count!=2)return 6;
   for(unsigned i=0;i<3&&!backend.pending();++i)if(!engine.progress(now))return 7;
   if(!backend.pending()||!backend.complete_next(outcome(SampleRate::id,*Hertz::from_integer(2000000),12)))return 8;
   now.monotonic={12000000};now.clock.time={100,12000000000ULL};if(!engine.progress(now)||!publisher.progress(now.monotonic,now.clock.time,codec::Tsi::gps,true)||sent.count!=3)return 9;
   if(sent.frames[1].time!=timing::ProtocolTime{100,10000000000ULL}||sent.frames[2].time!=timing::ProtocolTime{100,12000000000ULL}||std::get<Hertz>(sent.frames[1].state.fields[1].value)!=*Hertz::from_integer(1000000)||std::get<Hertz>(sent.frames[2].state.fields[1].value)!=*Hertz::from_integer(2000000))return 10;
  }
  for(unsigned i=0;i<6&&!*engine.complete(*h);++i)if(!engine.progress(now))return 11;
  if(!*engine.complete(*h))return 12;bool x=false;
  for(;;){auto response=engine.take_response(*h);if(!response)return 13;if(!*response)break;auto ack=**response;if(ack.kind==AckKind::execution){x=true;if(scenario==7){if(!ack.partial||ack.scheduled_or_executed)return 14;}else if(!ack.scheduled_or_executed||ack.partial)return 15;if(scenario==9&&ack.time!=timing::ProtocolTime{100,12000000000ULL})return 16;}if(scenario==7&&ack.kind==AckKind::state&&(ack.selected_mask&2))return 17;}
  if(!x||!engine.release(*h))return 18;
  if(scenario==6){sent.reject=true;if(publisher.progress(now.monotonic,now.clock.time,codec::Tsi::gps,true))return 19;if(publisher.progress({20000000},{100,20000000000ULL},codec::Tsi::gps,true)||publisher.status()!=StreamStatus::context_unavailable||sent.data||std::get<Hertz>(engine.state().fields[1].value)!=*Hertz::from_integer(2000000))return 20;}
  if(scenario==7){if(!store.faulted()||engine.state().fields[1].validity!=Validity::unknown)return 21;publisher.progress(now.monotonic,now.clock.time,codec::Tsi::gps,true);if(publisher.status()!=StreamStatus::metadata_unknown||sent.frames[sent.count-1].valid)return 22;}
  if(scenario==8){auto latest=store.current();if(!latest||std::get<Hertz>(first->event().state.fields[1].value)!=*Hertz::from_integer(1000000)||std::get<Hertz>(latest->event().state.fields[1].value)!=*Hertz::from_integer(2000000)||pool.used(Resource::revision)<2)return 23;}
 }
 {
  // A contradictory late execution after successful disarm faults observation,
  // without inventing a new timed effective revision or restoring requested state.
  AdmissionPool pool(AdmissionPool::reference_capacities());RevisionStore<8> store;auto first=store.initial(beginning(),credit(pool));Published sent;ContextPublisher<8> publisher(store,sent.binding());if(!first||!publisher.start({0})||!publisher.progress({0},{100,0},codec::Tsi::gps,true))return 24;
  VirtualBackend<> backend;Engine<2> engine(pool,backend.binding(),initial(),{Profile::generic_virtual_test,store.binding()});OperationContext now;now.clock={timing::ClockState::locked,{100,0},0,1,true,true,timing::Epoch::gps};now.timing=timing::TimingCapabilities::deterministic();ControlPacket command;command.set<SampleRate>(*Hertz::from_integer(2000000));auto h=submit(engine,command,now);if(!h||!engine.progress(now))return 25;
  auto late=backend.pending_capability();auto cancellation=verify_p07::cancellation(1,2);auto cancelled=engine.cancel(*h,cancellation.view(),now);if(!late||!cancelled||!cancelled->cancelled[1]||!engine.progress(now))return 26;while(true){auto ack=engine.take_response(*h);if(!ack)return 29;if(!*ack)break;}if(!engine.release(*h))return 30;
  if(late->complete(outcome(SampleRate::id,*Hertz::from_integer(3000000),10))||!engine.progress(now)||!engine.faulted()||engine.state().fields[1].validity!=Validity::unknown)return 27;
  const auto old_id=store.current()->id();publisher.backend_fault(engine.state());if(publisher.progress({10000000},{100,10000000000ULL},codec::Tsi::gps,true)||publisher.status()!=StreamStatus::backend_fault||store.current()->id()!=old_id||sent.frames[sent.count-1].valid||!sent.frames[sent.count-1].observation||sent.frames[sent.count-1].state.fields[1].validity!=Validity::unknown)return 28;
 }
 return 0;}
