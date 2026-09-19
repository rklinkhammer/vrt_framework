#include <cassert>
#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
using namespace vita;
int main() {
  auto config = profiles::iq::lab::config(0xabcdef);
  auto pools = profiles::iq::lab::pools();
  assert(config && pools);
  auto runtime =
      VitaRuntime<1, 4, 32, 65536>::create(*config, std::move(*pools));
  assert(runtime);
  auto &r = **runtime;
  StreamConfig c;
  c.sid = 1;
  c.controller_id = 2;
  c.controllee_id = 3;
  auto stream = r.add_controllee(c);
  assert(stream);
  assert(r.observe_pps({0}, {1000, 0}));
  assert(stream->start());
  assert(r.progress({0}));
  RecoveryConfig recovery;
  recovery.new_sid = 2;
  recovery.peer_ready = true;
  recovery.confirmed_state = stream->confirmed_state();
  recovery.confirmed_state.fields[0].value = std::uint32_t{2};
  assert(stream->recover(recovery));
  assert(r.progress({1000}));
  assert(stream->sid() == 2);
  assert(stream->lifecycle().phase == LifecyclePhase::running);
  assert(r.progress({2000}));
  assert(stream->shutdown());
  assert(r.progress({3000}));
  assert(stream->lifecycle().phase == LifecyclePhase::stopped);
}
