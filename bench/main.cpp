#include "allocation.hpp"
#include "capture.hpp"
#include "peer_capture.hpp"
#include "payload_source.hpp"
#include <vita/adapters/posix_udp/factory.hpp>
#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
#include <pthread.h>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <limits>
#include <string>
#include <thread>
using namespace vita;
using namespace vita::adapters::posix_udp;
using namespace vita::runtime::transaction;
namespace {
constexpr std::uint32_t fixture_oui=0xabcdef,cam=0xa91f0000;
constexpr std::size_t stack_bytes=1024*1024;
#if defined(__APPLE__)
constexpr bool c_allocation_coverage=true;
#else
constexpr bool c_allocation_coverage=false;
#endif
std::uint64_t now_ns() noexcept {return steady_trace_ns(nullptr);}
struct Options {std::string mode="normal",output="artifacts/P13/run",payload_mode="generated";double duration=1800,warmup=5,burst_period=60;};
struct DataStats {std::uint64_t packets=0,bytes=0,gap_samples=0,overlap_samples=0,invalid=0,next_ordinal=0,measured_samples=0,measured_next=0,measured_overlap=0;bool seen=false;};
struct Run {
  Options options;
  std::uint64_t sample_rate=1'000'000,control_rate=100;
  std::shared_ptr<bench::Capture> capture;
  FactoryConfig<> factory_config;
  bench::PayloadSource payload_source;
  std::unique_ptr<VitaRuntime<>> runtime;
  std::array<std::optional<VitaRuntime<>::Controllee>,4> sources;
  std::array<Socket,3> peer_sockets;Socket unused_peer;
  std::array<Address,3> destination;
  std::array<unsigned,5> packet_count{};
  std::array<DataStats,4> data{};
  std::array<StreamMetrics,4> warm_metrics{},final_metrics{};
  std::array<SourceStatus,4> observed_status{},measurement_end_status{},final_status{};
  std::array<std::array<std::uint64_t,9>,4> status_entries{};
  std::array<std::uint64_t,4> first_fault_ns{};
  bool progress_error_seen=false;unsigned last_progress_error=0;
  std::atomic<std::uint64_t> runtime_error{0},peer_error{0};
  std::atomic<bool> runtime_done{false},peer_done{false};
  std::uint64_t start=0,measure_start=0,measure_end=0,end=0;
  std::uint64_t normal_sent=0,burst_sent=0,burst_count=0,send_retry=0,acks_v=0,acks_x=0,acks_s=0,ack_failed=0,peer_bad=0,pending_overwrite=0;
  std::uint32_t next_mid=1;
  std::uint64_t progress_calls=0,backpressure=0;
  bool send_command(std::size_t model,bool burst) noexcept {
    if(!next_mid||next_mid==UINT32_MAX||model>=packet_count.size()){peer_error=5;return false;}
    const auto mid=next_mid;codec::Envelope envelope;envelope.type=codec::PacketType::command;envelope.stream_id=101+model;envelope.class_id=codec::ClassId{fixture_oui,1,0x20};envelope.packet_count=packet_count[model];envelope.command=codec::Command{cam,mid,codec::Identifier::short_id(3),codec::Identifier::short_id(2)};
    ControlPacket body;auto set=body.set<SampleRate>(Hertz{static_cast<std::int64_t>(1'000'000+mid)*(1ll<<20)});if(!set){peer_error=1;return false;}
    std::array<std::byte,128> bytes{};auto encoded=codec::encode_packet(envelope,body.freeze(),bytes);if(!encoded){peer_error=2;return false;}
    std::array<Bytes,1> parts{Bytes(bytes).first(*encoded)};const auto sent_at=now_ns();auto sent=peer_sockets[1].send(destination[1],parts);
    if(!sent){if(sent.error().retryable){++send_retry;return false;}peer_error=3;return false;}
    capture->peer.push({sent_at,101u+static_cast<std::uint32_t>(model),mid,burst?1u:0u,true});++next_mid;packet_count[model]=(packet_count[model]+1)&15;
    if(burst)++burst_sent;else ++normal_sent;return true;
  }
  bool receive(std::size_t lane) noexcept {
    std::array<std::byte,2048> bytes{};auto received=peer_sockets[lane].receive(bytes);
    if(!received){if(!received.error().retryable)peer_error=4;return false;}
    const auto observed=now_ns();if(received->source!=factory_config.instance->local_address(static_cast<Lane>(lane))||received->truncated){++peer_bad;return true;}
    codec::DecodeOptions decode;decode.request=codec::RequestContext{cam};auto packet=codec::decode_packet(Bytes(bytes).first(received->bytes),decode);if(!packet){++peer_bad;return true;}
    const auto& envelope=packet->envelope.envelope;if(!envelope.stream_id||!envelope.class_id||envelope.class_id->oui!=fixture_oui){++peer_bad;return true;}
    if(codec::is_data(envelope.type)){
      if(lane!=0||*envelope.stream_id<1||*envelope.stream_id>4){++peer_bad;return true;}auto& stats=data[*envelope.stream_id-1];
      if(envelope.timestamp.tsi!=codec::Tsi::gps||envelope.timestamp.tsf!=codec::Tsf::picoseconds||envelope.timestamp.integer<1000||packet->envelope.payload.size()!=1024){++stats.invalid;return true;}
      if(!std::equal(packet->envelope.payload.begin(),packet->envelope.payload.end(),payload_source.canonical().begin())){++stats.invalid;return true;}
      const auto elapsed_seconds=std::uint64_t(envelope.timestamp.integer)-1000;
      const std::uint64_t ordinal=elapsed_seconds*sample_rate+(static_cast<unsigned __int128>(envelope.timestamp.fractional)*sample_rate+999'999'999'999ULL)/1'000'000'000'000ULL;
      if(stats.seen){if(ordinal>stats.next_ordinal)stats.gap_samples+=ordinal-stats.next_ordinal;else if(ordinal<stats.next_ordinal)stats.overlap_samples+=stats.next_ordinal-ordinal;}
      const auto first=static_cast<std::uint64_t>(options.warmup*sample_rate);
      const auto last=first+static_cast<std::uint64_t>(options.duration*sample_rate);
      const auto covered_first=std::max(first,ordinal),covered_last=std::min(last,ordinal+256);
      if(covered_last>covered_first){
        if(covered_first<stats.measured_next)stats.measured_overlap+=std::min(covered_last,stats.measured_next)-covered_first;
        const auto unique_first=std::max(covered_first,stats.measured_next);
        if(covered_last>unique_first)stats.measured_samples+=covered_last-unique_first;
        stats.measured_next=std::max(stats.measured_next,covered_last);
      }
      stats.seen=true;stats.next_ordinal=ordinal+256;++stats.packets;stats.bytes+=received->bytes;
    }else if(envelope.type==codec::PacketType::command&&envelope.ack&&envelope.command){
      if(lane!=1){++peer_bad;return true;}
      bench::PeerAckPolicy policy{factory_config.instance->local_address(Lane::control),fixture_oui};
      auto event=bench::capture_ack(policy,received->source,received->truncated,*packet,next_mid,observed);
      if(!event){++peer_bad;return true;}
      if(event->kind==2)++acks_v;if(event->kind==3){++acks_x;if(!event->success)++ack_failed;}if(event->kind==4)++acks_s;
      capture->peer.push(*event);
    }else if(envelope.type!=codec::PacketType::context)++peer_bad;
    return true;
  }
  static void* peer_worker(void* pointer) noexcept {
    auto& r=*static_cast<Run*>(pointer);while(now_ns()<r.start)std::this_thread::sleep_for(std::chrono::microseconds(100));bench::critical_thread=true;
    std::uint64_t next_normal=0,next_burst=1,burst_left=0,burst_index=0,normal_not_before=0;
    while(now_ns()<r.end&&!r.peer_error.load()){
      const auto now=now_ns();
      if(now>=r.measure_start&&now<r.measure_end){
        const auto elapsed=now-r.measure_start;
        if(!burst_left&&r.options.burst_period>0&&elapsed>=static_cast<std::uint64_t>(r.options.burst_period*1e9)*next_burst){burst_left=64;burst_index=0;++next_burst;++r.burst_count;normal_not_before=now+1'000'000;}
        if(burst_left){for(unsigned work=0;work<64&&burst_left;++work){if(!r.send_command(1+burst_index/16,true))break;++burst_index;--burst_left;}}
        else if(now>=normal_not_before&&elapsed>=next_normal*1'000'000'000ULL/r.control_rate){if(r.send_command(0,false))++next_normal;}
      }
      bool work=false;for(unsigned cycle=0;cycle<64;++cycle){bool received=false;for(std::size_t lane=0;lane<3;++lane)received|=r.receive(lane);work|=received;if(!received)break;}
      if(!work)std::this_thread::sleep_for(std::chrono::microseconds(25));
    }
    bench::critical_thread=false;r.peer_done=true;return nullptr;
  }
  static void* runtime_worker(void* pointer) noexcept {
    auto& r=*static_cast<Run*>(pointer);while(now_ns()<r.start)std::this_thread::sleep_for(std::chrono::microseconds(100));bench::critical_thread=true;
    auto first=r.runtime->observe_pps({0},{1000,0});if(!first){r.runtime_error=1;r.runtime_done=true;return nullptr;}
    for(auto& source:r.sources)if(!source->start()){r.runtime_error=2;r.runtime_done=true;return nullptr;}
    std::uint64_t next_pps=1'000'000'000;bool warm=false,stopped=false;
    while(now_ns()<r.end&&!r.runtime_error.load()){
      const auto actual=now_ns();const auto elapsed=actual-r.start;
      if(elapsed>=next_pps){auto observed=r.runtime->observe_pps({elapsed},{1000+elapsed/1'000'000'000,(elapsed%1'000'000'000)*1000});if(!observed){r.runtime_error=3;break;}next_pps=elapsed+1'000'000'000;}
      if(actual>=r.measure_start&&!warm){for(std::size_t i=0;i<4;++i)r.warm_metrics[i]=r.sources[i]->metrics();warm=true;}
      if(actual>=r.measure_end&&!stopped){for(std::size_t i=0;i<4;++i){r.measurement_end_status[i]=r.sources[i]->status();r.sources[i]->pause();}stopped=true;}
      auto progressed=r.runtime->progress({elapsed});++r.progress_calls;
      for(std::size_t i=0;i<4;++i){auto status=r.sources[i]->status();if(status!=r.observed_status[i]){++r.status_entries[i][static_cast<std::size_t>(status)];r.observed_status[i]=status;}if(static_cast<unsigned>(status)>=static_cast<unsigned>(SourceStatus::clock_unavailable)&&!r.first_fault_ns[i])r.first_fault_ns[i]=actual;}
      if(!progressed){r.progress_error_seen=true;r.last_progress_error=static_cast<unsigned>(progressed.error().code);if(progressed.error().code==ErrorCode::capacity_exhausted||progressed.error().code==ErrorCode::resource_limit)++r.backpressure;else {r.runtime_error=100+static_cast<unsigned>(progressed.error().code);break;}}
    }
    for(std::size_t i=0;i<4;++i){r.final_metrics[i]=r.sources[i]->metrics();r.final_status[i]=r.sources[i]->status();if(!stopped)r.measurement_end_status[i]=r.final_status[i];}bench::critical_thread=false;r.runtime_done=true;return nullptr;
  }
  static void* writer_worker(void* pointer) noexcept {
    auto& capture=*static_cast<bench::Capture*>(pointer);while(!capture.finish.load(std::memory_order_acquire)){if(!capture.drain())std::this_thread::sleep_for(std::chrono::microseconds(100));}while(capture.drain()){}if(std::fflush(capture.trace_file)!=0||std::fflush(capture.peer_file)!=0)capture.io_error.store(true);return nullptr;
  }
};
bool thread_start(pthread_t& thread,void*(*function)(void*) noexcept,void* context){pthread_attr_t attr;if(pthread_attr_init(&attr))return false;auto ok=pthread_attr_setstacksize(&attr,stack_bytes)==0&&pthread_create(&thread,&attr,function,context)==0;pthread_attr_destroy(&attr);return ok;}
bool setup(Run& run){
 auto pools=profiles::iq::lab::pools(profiles::iq::lab::reference_counts());auto config=profiles::iq::lab::config(fixture_oui);if(!pools||!config)return false;
 auto selected_mode=bench::payload_mode(run.options.payload_mode);if(!selected_mode)return false;
 auto initialized=run.payload_source.initialize(*selected_mode,pools->payload);if(!initialized)return false;
 SocketConfig socket;socket.bind=Address::loopback(Family::ipv4);for(auto& peer:run.peer_sockets){auto made=Socket::open(socket);if(!made)return false;peer=std::move(*made);}auto dummy=Socket::open(socket);if(!dummy)return false;run.unused_peer=std::move(*dummy);
 for(auto& bound:run.factory_config.config.sockets)bound.bind=Address::loopback(Family::ipv4);
 run.factory_config.config.capabilities.reserved_control_slots=64;run.factory_config.config.capabilities.reserved_cancellation_slots=64;
 PeerBinding inbound;inbound.local_source={2,1};inbound.remote_source={1,1};for(std::size_t lane=0;lane<3;++lane)inbound.remote[lane]=run.peer_sockets[lane].local_address();if(!run.factory_config.peers.push_back(inbound))return false;
 PeerBinding unused;unused.local_source={1,1};unused.remote_source={2,1};unused.remote.fill(run.unused_peer.local_address());if(!run.factory_config.peers.push_back(unused))return false;
 config->transport=factory(run.factory_config);config->worker_stack_bytes=3*stack_bytes;
 auto runtime=VitaRuntime<>::create(*config,std::move(*pools));if(!runtime){std::fprintf(stderr,"runtime setup error %u\n",unsigned(runtime.error().code));return false;}run.runtime=std::move(*runtime);
 for(std::size_t i=0;i<16;++i){StreamConfig stream;stream.sid=i<4?1+i:i<9?101+i-4:5+i-8;stream.controller_id=2;stream.controllee_id=3;stream.sample_rate=run.sample_rate;stream.source=run.payload_source.provider();
   if(i>=4&&i<9){stream.kind=ControlleeKind::virtual_register;stream.trace={run.capture,run.capture.get(),steady_trace_ns,bench::Capture::record,sizeof(bench::Capture)};}
   auto device=run.runtime->add_controllee(stream);if(!device){std::fprintf(stderr,"stream %zu setup error %u\n",i,unsigned(device.error().code));return false;}if(i<4)run.sources[i]=*device;
 }
 if(run.runtime->budget().charged_bytes()>runtime::framework_budget-sizeof(bench::PayloadSource))return false;
 for(std::size_t lane=0;lane<3;++lane)run.destination[lane]=run.factory_config.instance->local_address(static_cast<Lane>(lane));return true;
}
bool component(const Options& options){
 auto file=std::fopen((options.output+"/components.json").c_str(),"w");if(!file)return false;
 constexpr std::size_t iterations=100000;std::array<std::byte,1052> wire{};std::array<std::byte,1024> payload{};codec::Envelope envelope;envelope.type=codec::PacketType::signal;envelope.stream_id=1;envelope.class_id=codec::ClassId{fixture_oui,1,1};envelope.timestamp={codec::Tsi::gps,codec::Tsf::picoseconds,1000,0};std::uint64_t checksum=0;
 auto begin=now_ns();bench::critical_thread=true;for(std::size_t i=0;i<iterations;++i){envelope.packet_count=i&15;auto encoded=codec::encode_envelope(envelope,payload,{},wire);if(!encoded)return false;checksum+=std::to_integer<unsigned>(wire[2]);}auto encode_ns=now_ns()-begin;
 begin=now_ns();for(std::size_t i=0;i<iterations;++i){auto decoded=codec::decode_packet(wire);if(!decoded)return false;checksum+=decoded->envelope.envelope.packet_count;}auto decode_ns=now_ns()-begin;
 runtime::StateSnapshot state;begin=now_ns();for(std::size_t i=0;i<iterations;++i){auto window=profiles::iq::SampleWriteWindow::create(payload,profiles::iq::SampleFormat::iq16,i*256,256,state);if(!window||!profiles::iq::default_source().produce(*window)||!window->validate_complete())return false;checksum+=std::to_integer<unsigned>(payload[0]);}auto source_ns=now_ns()-begin;bench::critical_thread=false;
 auto pools=profiles::iq::lab::pools();if(!pools)return false;
 begin=now_ns();bench::critical_thread=true;
 for(std::size_t i=0;i<iterations;++i){auto lease=pools->payload.acquire({1024,64,memory::MemoryDomain::cpu,true});if(!lease)return false;checksum+=lease->capacity();}
 const auto lease_ns=now_ns()-begin;bench::critical_thread=false;
 SocketConfig socket_config;socket_config.bind=Address::loopback(Family::ipv4);
 auto sender=Socket::open(socket_config),receiver=Socket::open(socket_config);if(!sender||!receiver)return false;
 constexpr std::size_t transport_iterations=10000;std::array<Bytes,2> segments{Bytes(wire).first(28),Bytes(wire).subspan(28)};std::array<std::byte,2048> received{};
 begin=now_ns();bench::critical_thread=true;
 for(std::size_t i=0;i<transport_iterations;++i){auto sent=sender->send(receiver->local_address(),segments);if(!sent)return false;auto deadline=now_ns()+100000000;for(;;){auto got=receiver->receive(received);if(got){checksum+=got->bytes;break;}if(!got.error().retryable||now_ns()>deadline)return false;}}
 const auto transport_ns=now_ns()-begin;bench::critical_thread=false;
 bench::Ring<TraceEvent,4096> trace_ring;TraceEvent trace_event;
 begin=now_ns();for(std::size_t i=0;i<iterations;++i){trace_event.monotonic_ns=now_ns();trace_ring.push(trace_event);trace_ring.pop(trace_event);checksum+=trace_event.monotonic_ns&1;}const auto trace_ns=now_ns()-begin;
 std::fprintf(file,"{\"lease_acquire_return_ns\":%llu,\"gathered_udp_roundtrip_ns\":%llu,\"transport_iterations\":%zu,\"trace_clock_push_pop_ns\":%llu,\"critical_c_allocations\":%llu,\"iterations\":%zu,\"packet_bytes\":1052,\"encode_ns\":%llu,\"decode_ns\":%llu,\"source_256_pairs_ns\":%llu,\"checksum\":%llu,\"critical_cpp_allocations\":%llu}\n",(unsigned long long)lease_ns,(unsigned long long)transport_ns,transport_iterations,(unsigned long long)trace_ns,bench::critical_c_allocations.load(),iterations,(unsigned long long)encode_ns,(unsigned long long)decode_ns,(unsigned long long)source_ns,(unsigned long long)checksum,bench::critical_allocations.load());std::fclose(file);return true;
}
}
double parse_number(const char* text){char* end=nullptr;auto value=std::strtod(text,&end);return end&&*end==0&&end!=text?value:std::numeric_limits<double>::quiet_NaN();}
int main(int argc,char** argv){
 Options options;for(int i=1;i<argc;++i){std::string key=argv[i];if(key=="--help"){std::puts("vita_benchmark --mode normal|overload|components --payload-mode generated|precomputed-copy|prefilled-pool --duration-seconds N --warmup-seconds N --burst-every-seconds N --output-dir PATH");return 0;}if(i+1==argc)return 2;const char* value=argv[++i];if(key=="--mode")options.mode=value;else if(key=="--output-dir")options.output=value;else if(key=="--payload-mode")options.payload_mode=value;else if(key=="--duration-seconds")options.duration=parse_number(value);else if(key=="--warmup-seconds")options.warmup=parse_number(value);else if(key=="--burst-every-seconds")options.burst_period=parse_number(value);else return 2;}
 if(!std::isfinite(options.duration)||!std::isfinite(options.warmup)||!std::isfinite(options.burst_period)||options.duration<=0||options.duration>86400||options.warmup<0||options.warmup>3600||options.burst_period<0||(options.mode!="normal"&&options.mode!="overload"&&options.mode!="components"))return 2;
 if(!bench::payload_mode(options.payload_mode)||(options.mode=="components"&&options.payload_mode!="generated"))return 2;
 std::error_code error;std::filesystem::create_directories(options.output,error);if(error)return 2;if(options.mode=="components")return component(options)?0:1;
 auto run=std::make_unique<Run>();run->options=options;if(options.mode=="overload"){run->sample_rate=1'200'000;run->control_rate=120;}
 run->capture=std::make_shared<bench::Capture>();auto& capture=*run->capture;
 capture.trace_file=std::fopen((options.output+"/trace.csv").c_str(),"w");capture.peer_file=std::fopen((options.output+"/peer.csv").c_str(),"w");if(!capture.trace_file||!capture.peer_file)return 2;
 std::setvbuf(capture.trace_file,capture.trace_buffer.data(),_IOFBF,capture.trace_buffer.size());std::setvbuf(capture.peer_file,capture.peer_buffer.data(),_IOFBF,capture.peer_buffer.size());
 std::fprintf(capture.trace_file,"generation,operation,peer,sid,mid,stage,monotonic_ns,cif,bit,status,simulated\n");std::fprintf(capture.peer_file,"monotonic_ns,sid,mid,kind,success,ack_cam\n");
 if(!setup(*run))return 3;
 run->start=now_ns()+200'000'000;run->measure_start=run->start+static_cast<std::uint64_t>(options.warmup*1e9);run->measure_end=run->measure_start+static_cast<std::uint64_t>(options.duration*1e9);run->end=run->measure_end+1'000'000'000;
 pthread_t writer,peer,worker;
 if(!thread_start(writer,Run::writer_worker,&capture))return 4;
 if(!thread_start(peer,Run::peer_worker,run.get())){capture.finish.store(true);pthread_join(writer,nullptr);return 4;}
 if(!thread_start(worker,Run::runtime_worker,run.get())){run->peer_error.store(99);pthread_join(peer,nullptr);capture.finish.store(true);pthread_join(writer,nullptr);return 4;}
 pthread_join(worker,nullptr);pthread_join(peer,nullptr);capture.finish.store(true,std::memory_order_release);pthread_join(writer,nullptr);if(std::fclose(capture.trace_file)!=0)capture.io_error.store(true);if(std::fclose(capture.peer_file)!=0)capture.io_error.store(true);
 auto* summary=std::fopen((options.output+"/summary.json").c_str(),"w");if(!summary)return 5;
 std::fprintf(summary,"{\"schema\":1,\"peer_capture_policy\":\"stateless_checked_raw_v1\",\"mode\":\"%s\",\"duration_seconds\":%.9g,\"warmup_seconds\":%.9g,\"measurement_start_ns\":%llu,\"measurement_end_ns\":%llu,\"iq_streams\":4,\"sample_rate\":%llu,\"samples_per_packet\":256,\"vrt_bytes\":1052,\"control_rate\":%llu,\"command_bytes\":44,\"cam\":%u,\"writable_fields_per_command\":1,\"burst_size\":64,\"burst_period_seconds\":%.9g,\"burst_count\":%llu,\"normal_sent\":%llu,\"burst_sent\":%llu,\"send_retry\":%llu,\"ack_v\":%llu,\"ack_x\":%llu,\"ack_s\":%llu,\"ack_failed\":%llu,\"peer_bad\":%llu,\"pending_overwrite\":%llu,\"runtime_error\":%llu,\"peer_error\":%llu,\"backpressure\":%llu,\"progress_calls\":%llu,\"critical_cpp_allocations\":%llu,\"trace_overflow\":%llu,\"peer_trace_overflow\":%llu,\"trace_rows\":%llu,\"peer_rows\":%llu,\"framework_bytes\":%zu,\"trace_storage_bytes\":%zu,\"worker_stack_bytes\":%zu,\"peer_application_bytes\":%zu,\"clock\":\"explicit injected GPS epoch with actual steady-clock pacing; not GPS qualification\",\"affinity\":\"not pinned\",\"deployment_qualified\":false,\"data\":[",options.mode.c_str(),options.duration,options.warmup,(unsigned long long)run->measure_start,(unsigned long long)run->measure_end,(unsigned long long)run->sample_rate,(unsigned long long)run->control_rate,cam,options.burst_period,(unsigned long long)run->burst_count,(unsigned long long)run->normal_sent,(unsigned long long)run->burst_sent,(unsigned long long)run->send_retry,(unsigned long long)run->acks_v,(unsigned long long)run->acks_x,(unsigned long long)run->acks_s,(unsigned long long)run->ack_failed,(unsigned long long)run->peer_bad,(unsigned long long)run->pending_overwrite,(unsigned long long)run->runtime_error.load(),(unsigned long long)run->peer_error.load(),(unsigned long long)run->backpressure,(unsigned long long)run->progress_calls,bench::critical_allocations.load(),(unsigned long long)capture.trace.overflow.load(),(unsigned long long)capture.peer.overflow.load(),(unsigned long long)capture.written_trace.load(),(unsigned long long)capture.written_peer.load(),run->runtime->budget().charged_bytes(),sizeof(bench::Capture),3*stack_bytes,sizeof(Run)-sizeof(bench::PayloadSource));
 for(std::size_t i=0;i<4;++i){const auto& got=run->data[i];const auto& made=run->final_metrics[i];const auto& warm=run->warm_metrics[i];std::fprintf(summary,"%s{\"sid\":%zu,\"accepted_packets_total\":%llu,\"accepted_packets_measured\":%llu,\"skipped_samples_measured\":%llu,\"skipped_packets_measured\":%llu,\"send_failures\":%llu,\"peer_packets_total\":%llu,\"peer_gap_samples_total\":%llu,\"peer_overlap_samples\":%llu,\"peer_invalid\":%llu,\"peer_measured_samples\":%llu,\"peer_expected_measured_samples\":%llu,\"peer_measured_missing_samples\":%llu,\"peer_measured_overlap_samples\":%llu}",i?",":"",i+1,(unsigned long long)made.packets,(unsigned long long)(made.packets-warm.packets),(unsigned long long)(made.skipped_samples-warm.skipped_samples),(unsigned long long)(made.skipped_packets-warm.skipped_packets),(unsigned long long)made.send_failures,(unsigned long long)got.packets,(unsigned long long)got.gap_samples,(unsigned long long)got.overlap_samples,(unsigned long long)got.invalid,(unsigned long long)got.measured_samples,(unsigned long long)(options.duration*run->sample_rate),(unsigned long long)(static_cast<std::uint64_t>(options.duration*run->sample_rate)-got.measured_samples),(unsigned long long)got.measured_overlap);}
 std::fprintf(summary,"],\"source_diagnostics\":{\"progress_error_seen\":%s,\"last_progress_error_code\":%u,\"status_names\":[\"recovering\",\"quiescing\",\"configured\",\"running\",\"stopped\",\"clock_unavailable\",\"context_unavailable\",\"faulted\",\"temporal_association\"],\"streams\":[",run->progress_error_seen?"true":"false",run->last_progress_error);
 for(std::size_t i=0;i<4;++i){std::fprintf(summary,"%s{\"sid\":%zu,\"measurement_end_status\":%u,\"final_status\":%u,\"first_fault_monotonic_ns\":%llu,\"status_entries\":[",i?",":"",i+1,unsigned(run->measurement_end_status[i]),unsigned(run->final_status[i]),(unsigned long long)run->first_fault_ns[i]);for(std::size_t j=0;j<9;++j)std::fprintf(summary,"%s%llu",j?",":"",(unsigned long long)run->status_entries[i][j]);std::fprintf(summary,"]}");}
 std::fprintf(summary,"]}");
 std::fprintf(summary,",\"critical_c_allocations\":%llu,\"c_allocation_coverage_available\":%s,\"c_allocation_instrumentation\":\"Mach-O interposition on Apple; unavailable elsewhere\",\"capture_io_error\":%s,\"trace_generated\":%llu,\"peer_generated\":%llu,\"socket_buffers\":[",bench::critical_c_allocations.load(),c_allocation_coverage?"true":"false",capture.io_error.load()?"true":"false",(unsigned long long)capture.trace.write.load(),(unsigned long long)capture.peer.write.load());for(std::size_t i=0;i<3;++i){const auto& stats=run->factory_config.instance->socket_stats(static_cast<Lane>(i));std::fprintf(summary,"%s{\"lane\":%zu,\"runtime_port\":%u,\"peer_port\":%u,\"send_bytes\":%d,\"receive_bytes\":%d,\"ip_mtu\":1500,\"family\":\"IPv4\",\"no_fragment\":true}",i?",":"",i,run->destination[i].port,run->peer_sockets[i].local_address().port,stats.send_buffer_bytes,stats.receive_buffer_bytes);}std::fprintf(summary,"],\"payload_mode\":\"%s\",\"source_provider_bytes\":%zu,\"framework_plus_source_bytes\":%zu,\"payload_setup_blocks\":%zu,\"payload_setup_scratch_bytes\":%zu,\"payload_counts_scope\":\"total_warmup_measurement_drain\",\"payload_source_calls\":%llu,\"payload_write_bytes\":%llu,\"payload_copy_calls\":%llu,\"payload_peer_check\":\"all1024bytesagainstcanonicaltemplate\",\"budget_rows\":[",options.payload_mode.c_str(),sizeof(bench::PayloadSource),run->runtime->budget().charged_bytes()+sizeof(bench::PayloadSource),run->payload_source.setup_blocks(),run->payload_source.setup_blocks()*sizeof(memory::BufferLease),(unsigned long long)run->payload_source.calls(),(unsigned long long)run->payload_source.payload_write_bytes(),(unsigned long long)run->payload_source.payload_copy_calls());
 for(std::size_t i=0;i<runtime::budget_category_count;++i){auto row=run->runtime->budget().row(static_cast<runtime::BudgetCategory>(i));std::fprintf(summary,"%s{\"category\":%zu,\"reserved\":%zu,\"charged\":%zu}",i?",":"",i,row.reserved,row.charged);}
 std::fprintf(summary,"],\"peer_socket_buffers\":[");for(std::size_t i=0;i<3;++i){auto stats=run->peer_sockets[i].stats();std::fprintf(summary,"%s{\"lane\":%zu,\"send_bytes\":%d,\"receive_bytes\":%d}",i?",":"",i,stats.send_buffer_bytes,stats.receive_buffer_bytes);}
 const auto adapter=run->factory_config.instance->metrics();
 std::fprintf(summary,"],\"adapter\":{\"tx_accepted\":%llu,\"tx_completed\":%llu,\"tx_failed\":%llu,\"tx_retry\":%llu,\"rx_delivered\":%llu,\"rx_pool_drop\":%llu,\"rx_malformed\":%llu,\"rx_truncated\":%llu,\"socket_errors\":%llu},\"copy_path\":\"external header+IQ payload gathered by sendmsg; kernel copy; contiguous recvmsg\"}\n",(unsigned long long)adapter.tx_accepted,(unsigned long long)adapter.tx_completed,(unsigned long long)adapter.tx_failed,(unsigned long long)adapter.tx_retry,(unsigned long long)adapter.rx_delivered,(unsigned long long)adapter.rx_pool_drop,(unsigned long long)adapter.rx_malformed,(unsigned long long)adapter.rx_truncated,(unsigned long long)adapter.socket_errors);
 bool summary_error=std::ferror(summary)!=0;if(std::fclose(summary)!=0)summary_error=true;
 std::printf("%s: commands=%llu trace=%llu framework=%zu critical_allocations=%llu\n",options.mode.c_str(),(unsigned long long)(run->normal_sent+run->burst_sent),(unsigned long long)capture.written_trace.load(),run->runtime->budget().charged_bytes(),bench::critical_allocations.load());
 return summary_error||capture.io_error||run->runtime_error||run->peer_error||capture.trace.overflow||capture.peer.overflow||bench::critical_allocations||bench::critical_c_allocations?1:0;
}
