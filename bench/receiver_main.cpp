#include "receiver_core.hpp"
#include "receiver_cli.hpp"
#include "capture.hpp"
#include "allocation.hpp"
#include <pthread.h>
#include <filesystem>
#include <thread>
using namespace vita;using namespace vita::bench::receiver;
namespace {
#if defined(__APPLE__)
constexpr bool c_coverage=true;
#else
constexpr bool c_coverage=false;
#endif
struct ReceiveLoop {
  Receiver* receiver;const Config* config;bool timeout=false;
  std::uint64_t elapsed=0,cpu_used=0;
  static void* run(void* pointer) noexcept {
    auto& loop=*static_cast<ReceiveLoop*>(pointer);
    const auto start=runtime::transaction::steady_trace_ns(nullptr),cpu=clock_ns(CLOCK_THREAD_CPUTIME_ID);
    bench::critical_thread=true;
    for(;;){const auto now=runtime::transaction::steady_trace_ns(nullptr),first=loop.receiver->first_rx();
      if(!first&&now-start>=10000000000ULL){loop.timeout=true;break;}
      if(first&&now>=first+loop.config->warmup_ns+loop.config->duration_ns+20000000ULL)break;
      loop.receiver->progress(now);
    }
    bench::critical_thread=false;
    loop.elapsed=runtime::transaction::steady_trace_ns(nullptr)-start;
    loop.cpu_used=clock_ns(CLOCK_THREAD_CPUTIME_ID)-cpu;return nullptr;
  }
};
struct Capture {
  bench::Ring<Trace,8192> ring;std::array<char,65536> buffer{};FILE* file=nullptr;std::atomic<bool> finished{false},error{false};std::uint64_t rows=0,writer_cpu_ns=0;std::array<std::array<std::uint64_t,4>,3> counts{};
  static void record(void* p,const Trace& value) noexcept{static_cast<Capture*>(p)->ring.push(value);}
  static void* writer(void* p) noexcept{auto& self=*static_cast<Capture*>(p);const auto cpu=clock_ns(CLOCK_THREAD_CPUTIME_ID);for(;;){Trace row;bool work=false;while(self.ring.pop(row)){work=true;int n=std::fprintf(self.file,"%u,%llu,%llu,%llu,%llu,%llu,%u,%u,%u,%zu,%llu\n",row.sid,(unsigned long long)row.ordinal,(unsigned long long)row.rx_ns,(unsigned long long)row.validated_ns,(unsigned long long)row.app_ns,(unsigned long long)row.consumed_ns,row.phase,row.status,unsigned(row.known),row.bytes,(unsigned long long)row.checksum);if(n<0)self.error=true;else{++self.rows;++self.counts[row.phase][row.status];}}if(self.finished.load(std::memory_order_acquire)&&!work)break;if(!work)std::this_thread::sleep_for(std::chrono::microseconds(100));}if(std::fflush(self.file))self.error=true;self.writer_cpu_ns=clock_ns(CLOCK_THREAD_CPUTIME_ID)-cpu;return nullptr;}
};
}
int main(int argc,char** argv){
  Options options;if(!parse(argc,argv,options,true))return 2;std::error_code error;std::filesystem::create_directories(options.output,error);if(error)return 2;
  profiles::iq::lab::PoolCounts counts;counts.header=counts.payload=counts.trailer=counts.control=counts.cancellation=counts.emergency=1;counts.rx_data=256;counts.rx_control=counts.rx_cancellation=64;
  auto measured=profiles::iq::lab::measure(counts);auto pools=profiles::iq::lab::pools(counts);if(!measured||!pools)return 3;
  auto capture=std::make_unique<Capture>();capture->file=std::fopen((options.output+"/receiver.csv").c_str(),"w");if(!capture->file)return 3;std::setvbuf(capture->file,capture->buffer.data(),_IOFBF,capture->buffer.size());if(std::fprintf(capture->file,"sid,ordinal,rx_ns,validated_ns,app_ns,consumed_ns,phase,status,known,bytes,checksum\n")<0)return 3;
  Config config;config.streams=options.streams;config.rate=options.rate;config.warmup_ns=options.warmup*1e9;config.duration_ns=options.duration*1e9;
  for(unsigned i=0;i<3;++i){config.local[i]=*address(options.bind_ip,options.ports[i]);config.sender[i]=*address(options.peer_ip,options.peer_ports[i]);}
  auto receiver=Receiver::create(config,std::move(*pools),capture.get(),Capture::record);if(!receiver)return 3;
  constexpr std::size_t writer_stack=1024*1024,receiver_stack=1024*1024;pthread_attr_t attr;if(pthread_attr_init(&attr))return 3;if(pthread_attr_setstacksize(&attr,writer_stack))return 3;pthread_t writer;if(pthread_create(&writer,&attr,Capture::writer,capture.get()))return 3;pthread_attr_destroy(&attr);
  const auto bytes=measured->raw_bytes+measured->provider_metadata_bytes+Receiver::object_bytes()+Receiver::auxiliary_bytes()+sizeof(Capture)+writer_stack+receiver_stack+sizeof(ReceiveLoop);
  if(bytes>runtime::framework_budget){capture->finished=true;pthread_join(writer,nullptr);return 3;}
  auto* ready=std::fopen((options.output+"/ready.json").c_str(),"w");if(!ready){capture->finished=true;pthread_join(writer,nullptr);return 3;}
  const auto data=(*receiver)->adapter().local_address(adapters::posix_udp::Lane::data).port,control=(*receiver)->adapter().local_address(adapters::posix_udp::Lane::control).port,cancel=(*receiver)->adapter().local_address(adapters::posix_udp::Lane::cancellation).port;
  for(auto* output:{ready,stdout})std::fprintf(output,"{\"ready\":true,\"kind\":\"receiver\",\"data_port\":%u,\"control_port\":%u,\"cancel_port\":%u,\"streams\":%zu,\"sample_rate\":%llu}\n",data,control,cancel,options.streams,(unsigned long long)options.rate);std::fclose(ready);std::fflush(stdout);
  ReceiveLoop loop{receiver->get(),&config};pthread_t worker;
  if(pthread_attr_init(&attr)||pthread_attr_setstacksize(&attr,receiver_stack)){
    capture->finished=true;pthread_join(writer,nullptr);return 3;
  }
  if(pthread_create(&worker,&attr,ReceiveLoop::run,&loop)){
    pthread_attr_destroy(&attr);capture->finished=true;pthread_join(writer,nullptr);return 3;
  }
  pthread_attr_destroy(&attr);pthread_join(worker,nullptr);
  const auto elapsed=loop.elapsed,cpu_used=loop.cpu_used;const bool timeout=loop.timeout;
  capture->finished.store(true,std::memory_order_release);pthread_join(writer,nullptr);if(std::fclose(capture->file))capture->error=true;
  auto* summary=std::fopen((options.output+"/summary.json").c_str(),"w");if(!summary)return 4;auto first=(*receiver)->first_rx();auto metrics=(*receiver)->adapter().metrics();
  std::fprintf(summary,"{\"c_allocation_coverage_available\":%s,\"receiver_stack_bytes\":%zu,\"receiver_loop_state_bytes\":%zu,\"writer_thread_cpu_ns\":%llu,\"rx_data_blocks\":%zu,\"rx_control_blocks\":%zu,\"rx_cancellation_blocks\":%zu,",c_coverage?"true":"false",receiver_stack,sizeof(ReceiveLoop),(unsigned long long)capture->writer_cpu_ns,counts.rx_data,counts.rx_control,counts.rx_cancellation);
  std::fprintf(summary,"\"schema\":1,\"kind\":\"receiver\",\"scope\":\"Udp+RouteRegistry+ContextReceiver; no generator or VitaRuntime transaction facade\",\"duration_seconds\":%.9g,\"warmup_seconds\":%.9g,\"measurement_start_ns\":%llu,\"measurement_end_ns\":%llu,\"association_generation\":1,\"epoch_seconds\":1000,\"sample_rate\":%llu,\"samples_per_packet\":256,\"payload_bytes\":1024,\"checksum_algorithm\":\"fnv1a64\",\"rows\":%llu,\"generated\":%llu,\"capture_overflow\":%llu,\"capture_io_error\":%s,\"startup_timeout\":%s,\"elapsed_ns\":%llu,\"receiver_thread_cpu_ns\":%llu,\"critical_cpp_allocations\":%llu,\"critical_c_allocations\":%llu,\"accounted_bytes\":%zu,\"pool_raw_bytes\":%zu,\"pool_provider_bytes\":%zu,\"receiver_object_bytes\":%zu,\"receiver_auxiliary_bytes\":%zu,\"capture_bytes\":%zu,\"writer_stack_bytes\":%zu,\"network_kernel_loss_attribution\":\"unknown; observed ordinal gaps may include sender skips or network/kernel loss\",\"sids\":[",options.duration,options.warmup,(unsigned long long)(first?first+config.warmup_ns:0),(unsigned long long)(first?first+config.warmup_ns+config.duration_ns:0),(unsigned long long)options.rate,(unsigned long long)capture->rows,(unsigned long long)capture->ring.write.load(),(unsigned long long)capture->ring.overflow.load(),capture->error?"true":"false",timeout?"true":"false",(unsigned long long)elapsed,(unsigned long long)cpu_used,bench::critical_allocations.load(),bench::critical_c_allocations.load(),bytes,measured->raw_bytes,measured->provider_metadata_bytes,Receiver::object_bytes(),Receiver::auxiliary_bytes(),sizeof(Capture),writer_stack);
  for(std::size_t i=0;i<options.streams;++i)std::fprintf(summary,"%s%zu",i?",":"",i+1);std::fprintf(summary,"],\"phase_status_counts\":[");for(unsigned phase=0;phase<3;++phase){std::fprintf(summary,"%s[",phase?",":"");for(unsigned status=0;status<4;++status)std::fprintf(summary,"%s%llu",status?",":"",(unsigned long long)capture->counts[phase][status]);std::fprintf(summary,"]");}std::fprintf(summary,"],\"streams\":[");
  for(std::size_t i=0;i<options.streams;++i){const auto& s=(*receiver)->stats(i);std::fprintf(summary,"%s{\"sid\":%zu,\"checked_data\":%llu,\"contexts\":%llu,\"context_rejected\":%llu,\"delivered\":%llu,\"known\":%llu,\"invalid\":%llu,\"metadata_drops\":%llu,\"waiting_high_water\":%zu,\"pending_overflow\":%llu,\"measured_packets\":%llu,\"measured_samples\":%llu,\"observed_interior_gap_samples\":%llu,\"observed_overlap_samples\":%llu,\"first_measured_ordinal\":%llu,\"last_measured_one_past\":%llu}",i?",":"",i+1,(unsigned long long)s.checked,(unsigned long long)s.contexts,(unsigned long long)s.context_rejected,(unsigned long long)s.delivered,(unsigned long long)s.known,(unsigned long long)s.invalid,(unsigned long long)s.metadata_drops,s.waiting_peak,(unsigned long long)s.pending_overflow,(unsigned long long)s.measured_packets,(unsigned long long)s.measured_samples,(unsigned long long)s.measured_gaps,(unsigned long long)s.measured_overlap,(unsigned long long)s.first_ordinal,(unsigned long long)s.next_ordinal);}
  std::fprintf(summary,"],\"adapter\":{\"rx_delivered\":%llu,\"rx_malformed\":%llu,\"rx_truncated\":%llu,\"rx_unauthorized\":%llu,\"rx_pool_drop\":%llu,\"rx_lane_drop\":%llu,\"rx_mtu_drop\":%llu,\"socket_errors\":%llu},\"sockets\":[",(unsigned long long)metrics.rx_delivered,(unsigned long long)metrics.rx_malformed,(unsigned long long)metrics.rx_truncated,(unsigned long long)metrics.rx_unauthorized,(unsigned long long)metrics.rx_pool_drop,(unsigned long long)metrics.rx_lane_drop,(unsigned long long)metrics.rx_mtu_drop,(unsigned long long)metrics.socket_errors);
  for(unsigned i=0;i<3;++i){auto s=(*receiver)->adapter().socket_stats(static_cast<adapters::posix_udp::Lane>(i));std::fprintf(summary,"%s{\"lane\":%u,\"send_buffer_bytes\":%d,\"receive_buffer_bytes\":%d}",i?",":"",i,s.send_buffer_bytes,s.receive_buffer_bytes);}std::fprintf(summary,"]}\n");bool io=std::ferror(summary);if(std::fclose(summary))io=true;
  return io||timeout||capture->error||capture->ring.overflow||bench::critical_allocations||bench::critical_c_allocations?1:0;
}
