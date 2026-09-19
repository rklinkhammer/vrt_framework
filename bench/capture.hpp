#pragma once
#include <vita/runtime/transaction/trace.hpp>
#include <array>
#include <atomic>
#include <cstdio>
namespace vita::bench {
using namespace runtime::transaction;
template<class T,std::size_t N> struct Ring {
  std::array<T,N> entries{};
  alignas(64) std::atomic<std::uint64_t> write{0};
  alignas(64) std::atomic<std::uint64_t> read{0};
  std::atomic<std::uint64_t> overflow{0};
  bool push(const T& value) noexcept {auto w=write.load(std::memory_order_relaxed);if(w-read.load(std::memory_order_acquire)==N){overflow.fetch_add(1,std::memory_order_relaxed);return false;}entries[w%N]=value;write.store(w+1,std::memory_order_release);return true;}
  bool pop(T& value) noexcept {auto r=read.load(std::memory_order_relaxed);if(r==write.load(std::memory_order_acquire))return false;value=entries[r%N];read.store(r+1,std::memory_order_release);return true;}
};
struct PeerEvent {std::uint64_t monotonic_ns=0;std::uint32_t sid=0,mid=0;std::uint32_t kind=0;bool success=false;std::uint32_t ack_cam=0;};
struct Capture {
  Ring<TraceEvent,4096> trace;
  Ring<PeerEvent,4096> peer;
  std::array<char,65536> trace_buffer{},peer_buffer{};
  FILE* trace_file=nullptr;FILE* peer_file=nullptr;
  std::atomic<bool> finish{false},io_error{false};
  std::atomic<std::uint64_t> written_trace{0},written_peer{0};
  static void record(void* p,const TraceEvent& event) noexcept {static_cast<Capture*>(p)->trace.push(event);}
  bool drain() noexcept {
    bool work=false;TraceEvent event;
    for(std::size_t i=0;i<4096&&trace.pop(event);++i){work=true;if(std::fprintf(trace_file,"%llu,%llu,%llu,%u,%u,%u,%llu,%u,%u,%u,%u\n",(unsigned long long)event.key.association_generation,(unsigned long long)event.key.operation,(unsigned long long)event.key.peer,event.key.sid,event.key.mid,unsigned(event.stage),(unsigned long long)event.monotonic_ns,unsigned(event.field.cif),unsigned(event.field.bit),unsigned(event.status),unsigned(event.simulated))<0)io_error.store(true);else written_trace.fetch_add(1,std::memory_order_relaxed);}
    PeerEvent packet;for(std::size_t i=0;i<4096&&peer.pop(packet);++i){work=true;if(std::fprintf(peer_file,"%llu,%u,%u,%u,%u,%u\n",(unsigned long long)packet.monotonic_ns,packet.sid,packet.mid,packet.kind,unsigned(packet.success),packet.ack_cam)<0)io_error.store(true);else written_peer.fetch_add(1,std::memory_order_relaxed);}return work;
  }
};
} // namespace vita::bench
