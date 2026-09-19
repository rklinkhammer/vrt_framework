#include <vita/runtime/transaction/engine.hpp>
#include <vita/profiles/iq/source.hpp>
#include <atomic>
#include <thread>
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transaction;
struct Sink {std::array<TraceEvent,32> events{};std::size_t count=0;bool overflow=false;std::atomic<std::uint64_t> ticks{100};static std::uint64_t now(void*p)noexcept{return static_cast<Sink*>(p)->ticks.fetch_add(10);}static void record(void*p,const TraceEvent&e)noexcept{auto&s=*static_cast<Sink*>(p);if(s.count==s.events.size()){s.overflow=true;return;}s.events[s.count++]=e;}};
int main(){auto sink=std::make_shared<Sink>();TraceBinding trace{sink,sink.get(),Sink::now,Sink::record,sizeof(Sink)+128};
 AdmissionPool pool(AdmissionPool::reference_capacities());VirtualBackend<> backend;if(!backend.set_inline_completion(true))return 1;EngineOptions options;options.profile=Profile::generic_virtual_test;options.trace=trace;StateSnapshot state;for(auto id:baseline_fields)state.fields[field_index(id)].validity=Validity::known;state.fields[1].value=*Hertz::from_integer(1);Engine<2> engine(pool,backend.binding(),state,options);
 ControlPacket packet;const auto format=profiles::iq::payload_format(profiles::iq::SampleFormat::iq16);if(!packet.set<ReferencePoint>(7)||!packet.set<SampleRate>(*Hertz::from_integer(2))||!packet.set<StateEvent>(0)||!packet.set<DataPayloadFormat>(format))return 2;
 codec::Envelope envelope;envelope.type=codec::PacketType::command;envelope.stream_id=1;envelope.command=codec::Command{0x091f0000,42};std::array<std::byte,256> bytes;auto size=codec::encode_packet(envelope,packet.freeze(),bytes);if(!size)return 3;auto parsed=codec::decode_packet(Bytes{bytes}.first(*size));if(!parsed)return 4;
 OperationContext now;now.operation=99;now.association_generation=7;now.trace_peer=9;auto handle=engine.accept(*parsed,now);if(!handle||sink->count!=1||sink->events[0].stage!=TraceStage::validated)return 5;
 if(!engine.progress(now)||backend.pending()||backend.begins()!=1||backend.writes()!=1||engine.state().version==backend.model().version)return 6;
 for(unsigned n=0;n<8;++n)if(!engine.progress(now))return 7;
 if(sink->count!=10||sink->overflow||backend.begins()!=4||backend.writes()!=4)return 8;
 const TraceKey expected{7,99,9,1,42};std::uint64_t previous=0;unsigned dispatch=0,done=0,recorded=0;
 for(std::size_t i=0;i<sink->count;++i){const auto&e=sink->events[i];if(e.key!=expected||e.monotonic_ns<previous||e.simulated)return 9;previous=e.monotonic_ns;if(e.stage==TraceStage::dispatch)++dispatch;if(e.stage==TraceStage::device_done){++done;if(e.status!=FieldStatus::executed||e.field!=sink->events[i-1].field)return 10;}if(e.stage==TraceStage::recorded){++recorded;if(i!=9||e.status!=FieldStatus::executed)return 11;}}
 if(dispatch!=4||done!=4||recorded!=1||engine.state().fields[1].value!=SemanticValue{*Hertz::from_integer(2)})return 12;
 // Ready publication synchronizes both semantic payload and captured completion
 // timestamp. Backend thread never invokes the strand-only record callback.
 CompletionArena<1> tickets;auto storage=std::make_shared<ResultStorage<1>>();storage->trace=trace;auto token=tickets.reserve(123);if(!token)return 13;auto& slot=storage->slots[0];AsyncResult result{token->publisher(),storage,&slot.outcome,0,&slot.guard,&storage->trace,&slot.device_done_ns};const auto before=sink->count;
 std::thread worker([result]{FieldOutcome out;out.id=SampleRate::id;out.status=FieldStatus::executed;out.value=*Hertz::from_integer(3);result.complete(out);});worker.join();auto completed=tickets.consume(0);if(!completed||slot.device_done_ns<=previous||sink->count!=before||slot.outcome.value!=SemanticValue{*Hertz::from_integer(3)})return 14;
 AdmissionPool exhausted(AdmissionRequest{});VirtualBackend<> rejected_backend;Engine<1> rejected(exhausted,rejected_backend.binding(),state,options);const auto trace_before=sink->count;if(rejected.accept(*parsed,now)||sink->count!=trace_before||rejected_backend.begins())return 15;
 return 0;}
