#include "receiver_cli.hpp"
#include "allocation.hpp"
#include <vita/runtime/transaction/trace.hpp>
#include <filesystem>
#include <cstdio>
using namespace vita;using namespace vita::bench::receiver;using namespace vita::adapters::posix_udp;
#if defined(__APPLE__)
constexpr bool c_coverage=true;
#else
constexpr bool c_coverage=false;
#endif
int main(int argc,char** argv){
  Options options;if(!parse(argc,argv,options,false))return 2;std::error_code error;std::filesystem::create_directories(options.output,error);if(error)return 2;
  std::array<Socket,3> sockets;std::array<Address,3> destinations;
  for(unsigned i=0;i<3;++i){SocketConfig config;config.bind=*address(options.bind_ip,options.ports[i]);auto socket=Socket::open(config);if(!socket)return 3;sockets[i]=std::move(*socket);destinations[i]=*address(options.peer_ip,options.peer_ports[i]);}
  std::array<std::byte,payload_bytes> payload{};if(!canonical(payload))return 3;
  struct Stats{std::uint64_t ordinal=0,offered=0,accepted=0,skipped=0,retries=0,failed=0,contexts=0,last_context=0;unsigned data_count=0,context_count=0;bool context_started=false;};std::array<Stats,4> stats;
  const auto start=runtime::transaction::steady_trace_ns(nullptr),duration=static_cast<std::uint64_t>(options.duration*1e9),cpu=clock_ns(CLOCK_THREAD_CPUTIME_ID);std::uint64_t native_error=0;bench::critical_thread=true;
  while(!native_error){auto now=runtime::transaction::steady_trace_ns(nullptr);const auto elapsed=now-start;if(elapsed>=duration)break;
    const auto due_ordinal=static_cast<std::uint64_t>((static_cast<unsigned __int128>(elapsed)*options.rate)/1000000000ULL)/pairs*pairs;
    for(std::size_t i=0;i<options.streams;++i){auto& s=stats[i];if(s.ordinal>due_ordinal)continue;if(due_ordinal>s.ordinal){s.skipped+=due_ordinal-s.ordinal;s.ordinal=due_ordinal;}
      if(!s.context_started||now-s.last_context>=1000000000ULL){std::array<std::byte,128> wire{};auto encoded=context_packet(i+1,s.context_count,s.ordinal,options.rate,wire);if(!encoded){native_error=1;break;}std::array<Bytes,1> vector{Bytes(wire).first(*encoded)};auto sent=sockets[1].send(destinations[1],vector);if(!sent){if(sent.error().retryable){++s.retries;continue;}native_error=sent.error().native_error?sent.error().native_error:2;break;}s.context_started=true;s.last_context=now;s.context_count=(s.context_count+1)&15;++s.contexts;}
      std::array<std::byte,1052> wire{};auto encoded=data_packet(i+1,s.data_count,s.ordinal,options.rate,payload,wire);if(!encoded){native_error=3;break;}std::array<Bytes,1> vector{Bytes(wire).first(*encoded)};++s.offered;auto sent=sockets[0].send(destinations[0],vector);if(!sent){if(sent.error().retryable){++s.retries;continue;}++s.failed;native_error=sent.error().native_error?sent.error().native_error:4;break;}++s.accepted;s.data_count=(s.data_count+1)&15;s.ordinal+=pairs;
    }
  }
  bench::critical_thread=false;const auto elapsed=runtime::transaction::steady_trace_ns(nullptr)-start,cpu_used=clock_ns(CLOCK_THREAD_CPUTIME_ID)-cpu;
  auto* file=std::fopen((options.output+"/summary.json").c_str(),"w");if(!file)return 4;
  std::fprintf(file,"{\"c_allocation_coverage_available\":%s,",c_coverage?"true":"false");
  std::fprintf(file,"\"schema\":1,\"kind\":\"replay_sender\",\"scope\":\"separate native replay process; precomputed payload; no remote timing subtraction\",\"duration_seconds\":%.9g,\"start_ns\":%llu,\"elapsed_ns\":%llu,\"sender_thread_cpu_ns\":%llu,\"sample_rate\":%llu,\"samples_per_packet\":256,\"payload_bytes\":1024,\"critical_cpp_allocations\":%llu,\"critical_c_allocations\":%llu,\"native_error\":%llu,\"data\":[",options.duration,(unsigned long long)start,(unsigned long long)elapsed,(unsigned long long)cpu_used,(unsigned long long)options.rate,bench::critical_allocations.load(),bench::critical_c_allocations.load(),(unsigned long long)native_error);
  for(std::size_t i=0;i<options.streams;++i){const auto& s=stats[i];std::fprintf(file,"%s{\"sid\":%zu,\"send_attempts\":%llu,\"accepted_packets\":%llu,\"accepted_samples\":%llu,\"skipped_samples\":%llu,\"next_ordinal\":%llu,\"retries\":%llu,\"failed\":%llu,\"context_packets\":%llu}",i?",":"",i+1,(unsigned long long)s.offered,(unsigned long long)s.accepted,(unsigned long long)(s.accepted*pairs),(unsigned long long)s.skipped,(unsigned long long)s.ordinal,(unsigned long long)s.retries,(unsigned long long)s.failed,(unsigned long long)s.contexts);}
  std::fprintf(file,"],\"socket_buffers\":[");for(unsigned i=0;i<3;++i){auto s=sockets[i].stats();std::fprintf(file,"%s{\"lane\":%u,\"send_bytes\":%d,\"receive_bytes\":%d}",i?",":"",i,s.send_buffer_bytes,s.receive_buffer_bytes);}std::fprintf(file,"]}\n");bool io=std::ferror(file);if(std::fclose(file))io=true;return io||native_error||bench::critical_allocations||bench::critical_c_allocations?1:0;
}
