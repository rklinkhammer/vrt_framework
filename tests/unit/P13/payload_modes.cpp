#include "../../../bench/payload_source.hpp"
#include <vita/profiles/iq/lab.hpp>
#include <cassert>
using namespace vita;
int main(){
  auto pools=profiles::iq::lab::pools();assert(pools);
  runtime::StateSnapshot state;
  for(auto mode:{bench::PayloadMode::generated,bench::PayloadMode::precomputed_copy,bench::PayloadMode::prefilled_pool}){
    bench::PayloadSource source;assert(source.initialize(mode,pools->payload));
    assert(source.setup_blocks()==(mode==bench::PayloadMode::prefilled_pool?32:0));
    std::array<memory::BufferLease,32> held;
    for(auto& lease:held){auto acquired=pools->payload.acquire({1024});assert(acquired);lease=std::move(*acquired);}
    for(unsigned reuse=0;reuse<2;++reuse)for(auto& lease:held){
      auto bytes=lease.writable_bytes();assert(bytes);
      auto window=profiles::iq::SampleWriteWindow::create(bytes->first(1024),profiles::iq::SampleFormat::iq16,reuse*256,256,state);assert(window);
      assert(source.provider().produce(*window)&&window->validate_complete());
      assert(std::equal(window->wire().begin(),window->wire().end(),source.canonical().begin()));
    }
    assert(source.calls()==64);
    assert(source.payload_write_bytes()==(mode==bench::PayloadMode::prefilled_pool?0:65536));
    assert(source.payload_copy_calls()==(mode==bench::PayloadMode::precomputed_copy?64:0));
    auto bytes=held[0].writable_bytes();assert(bytes);
    auto bad=profiles::iq::SampleWriteWindow::create(bytes->first(1024),profiles::iq::SampleFormat::iq16,1,256,state);assert(bad&&!source.provider().produce(*bad));
  }
  // Bulk completion must validate actual wire values before setting coverage.
  std::array<std::byte,8> nan{};codec::detail::store32(nan,0,0x7fc00000);
  auto floating=profiles::iq::SampleWriteWindow::create(nan,profiles::iq::SampleFormat::float32,0,1,state);assert(floating);
  assert(!floating->complete_from_wire()&&!floating->validate_complete());
  codec::detail::store32(nan,0,0);assert(floating->complete_from_wire()&&floating->validate_complete());
  bench::PayloadSource invalid;assert(!invalid.initialize(static_cast<bench::PayloadMode>(99),pools->payload));
  assert(!bench::payload_mode("unknown"));
}
