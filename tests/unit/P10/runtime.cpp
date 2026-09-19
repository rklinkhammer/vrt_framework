#include <cassert>
#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
using namespace vita;
struct Capture {
  std::size_t packets = 0;
  bool known = true;
  std::size_t executions = 0, validations = 0, states = 0;
  VitaRuntime<2, 4, 32, 65536>::Controller *controller = nullptr;
  TransactionHandle transaction{};
  static void data(void *p,
                   const runtime::context::BorrowedSignalRx &signal) noexcept {
    auto &c = *static_cast<Capture *>(p);
    ++c.packets;
    c.known &=
        signal.metadata.confidence == runtime::context::Confidence::known;
    assert(signal.fragment(0));
  }
  static void
  event(void *p,
        const runtime::transaction::Observation &observation) noexcept {
    auto &c = *static_cast<Capture *>(p);
    using K = runtime::transaction::ObservationKind;
    c.executions += observation.kind == K::execution;
    c.validations += observation.kind == K::validation;
    c.states += observation.kind == K::state;
    if (c.controller)
      assert(!c.controller->wait(c.transaction, 1));
  }
};
int main() {
  for (auto format :
       {profiles::iq::SampleFormat::iq16, profiles::iq::SampleFormat::iq32,
        profiles::iq::SampleFormat::float32}) {
    auto config = profiles::iq::lab::config(0xabcdef);
    auto pools = profiles::iq::lab::pools();
    assert(config && pools);
    auto runtime =
        VitaRuntime<2, 4, 32, 65536>::create(*config, std::move(*pools));
    assert(runtime);
    auto &r = **runtime;
    Capture capture;
    StreamConfig stream;
    stream.sid = 1;
    stream.controller_id = 2;
    stream.controllee_id = 3;
    stream.format = format;
    stream.receiver = {&capture, Capture::data};
    auto device = r.add_controllee(stream);
    assert(device);
    auto controller = r.add_controller(*device);
    assert(controller);
    capture.controller = &*controller;
    assert(!device->start());
    assert(r.observe_pps({0}, {1000, 0}));
    assert(device->start());
    assert(r.progress({0}));
    assert(capture.packets == 1 && capture.known);
    auto command = controller->set_sample_rate(*Hertz::from_integer(2'000'000));
    assert(command);
    capture.transaction = *command;
    assert(controller->observe(*command, &capture, Capture::event));
    assert(r.run_for(1'000'000));
    assert(capture.executions == 1 && capture.validations == 1 &&
           capture.states == 1);
    assert(capture.known);
    auto state = controller->state(*command);
    assert(state && *state);
    assert(std::get<Hertz>((*state)->state.fields[1].value).q20 ==
           (2'000'000ll << 20));
    assert(r.budget().charged_bytes() < runtime::framework_budget);
  }
  auto config = profiles::iq::lab::config(0xabcdef);
  auto pools = profiles::iq::lab::pools();
  assert(config && pools);
  config->memory_limit = 128;
  assert(!VitaRuntime<>::create(*config, std::move(*pools)));
}
