#include <cassert>
#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
using namespace vita;
struct Capture {
  std::size_t packets = 0;
  static void
  receive(void *context,
          const runtime::context::BorrowedSignalRx &signal) noexcept {
    auto &capture = *static_cast<Capture *>(context);
    std::size_t payload_bytes = 0;
    for (std::size_t i = 0; i < signal.fragment_count(); ++i) {
      auto fragment = signal.fragment(i);
      assert(fragment);
      payload_bytes += fragment->size();
    }
    assert(payload_bytes == 4096);
    assert(signal.metadata.state.profile ==
           profiles::iq::Profile::sdr_radio);
    assert(signal.sample_time.picoseconds <
           runtime::timing::picoseconds_per_second);
    ++capture.packets;
  }
};
int main() {
  auto config = profiles::iq::lab::config(0xabcdef);
  profiles::iq::lab::PoolCounts counts;
  counts.payload_bytes = 4096;
  counts.rx_data_bytes = 8192;
  auto pools = profiles::iq::lab::pools(counts);
  assert(config && pools);
  config->clock.epoch = runtime::timing::Epoch::utc;
  auto made = VitaRuntime<1, 4, 32, 65536>::create(*config, std::move(*pools));
  assert(made);
  Capture capture;
  StreamConfig stream;
  stream.sid = 1;
  stream.controller_id = 2;
  stream.controllee_id = 3;
  stream.profile = profiles::iq::Profile::sdr_radio;
  stream.sample_rate = 1'000'000;
  stream.bandwidth = 800'000;
  stream.ip_mtu = 4200;
  stream.maximum_samples_per_packet = 1024;
  stream.trailer = true;
  stream.receiver = {&capture, Capture::receive};
  auto controllee = (*made)->add_controllee(stream);
  assert(controllee);
  auto controller = (*made)->add_controller(*controllee);
  assert(controller);
  assert((*made)->observe_pps({0}, {1000, 0}));

  const auto verify_capabilities = [&] {
    const auto before = controllee->confirmed_state();
    const auto status_before = controllee->status();
    auto requested = controller->query_capabilities();
    assert(requested && (*made)->run_for(1'000'000));
    auto observed = controller->capabilities(*requested);
    assert(observed && *observed);
    assert((**observed).range<RFReferenceFrequency>() ==
           stream.sdr_capabilities.center_frequency);
    assert((**observed).range<SampleRate>() ==
           stream.sdr_capabilities.sample_rate);
    assert((**observed).range<Bandwidth>() ==
           stream.sdr_capabilities.bandwidth);
    assert((**observed).range<Gain>() == stream.sdr_capabilities.gain);
    const auto after = controllee->confirmed_state();
    assert(after.version == before.version);
    for (std::size_t i = 0; i < runtime::state_field_capacity; ++i) {
      assert(after.fields[i].id == before.fields[i].id);
      assert(after.fields[i].validity == before.fields[i].validity);
      assert(after.fields[i].value == before.fields[i].value);
    }
    assert(controllee->status() == status_before);
  };
  verify_capabilities();

  SdrRadioSettings settings;
  settings.bandwidth = *Hertz::from_integer(900'000);
  settings.center_frequency = *Hertz::from_integer(101'000'000);
  settings.gain = GainStages{10 * 128, 0};
  settings.sample_rate = *Hertz::from_integer(2'000'000);
  auto configured = controller->configure(settings);
  assert(configured);
  assert((*made)->run_for(10'000'000));
  auto execution = controller->wait(*configured, 0, WaitEvidence::execution);
  assert(execution && execution->status == WaitStatus::evidence_received &&
         execution->observation.confirms_execution);
  auto state = controller->state(*configured);
  assert(state && *state);
  assert((**state).value<Bandwidth>() == settings.bandwidth);
  assert((**state).value<RFReferenceFrequency>() == settings.center_frequency);
  assert((**state).value<Gain>() == settings.gain);
  assert((**state).value<SampleRate>() == settings.sample_rate);

  const runtime::timing::ProtocolTime start_time{1000, 51'000'000'000};
  auto started = controller->start(start_time);
  assert(started);
  assert((*made)->run_for(39'000'000));
  auto admission = controller->wait(*started, 0, WaitEvidence::validation);
  assert(admission && admission->observation.validation_accepted &&
         !admission->observation.confirms_execution);
  auto early = controller->wait(*started, 0, WaitEvidence::execution);
  assert(early && early->status == WaitStatus::wait_budget_expired);
  assert(capture.packets == 0 &&
         controllee->status() == SourceStatus::configured);
  assert((*made)->run_for(2'000'000));
  assert(controllee->status() == SourceStatus::running && capture.packets > 0);
  verify_capabilities();

  auto unsupported =
      controller->query_capabilities(QuerySelection{QueryField::state_event});
  assert(unsupported && (*made)->run_for(1'000'000));
  auto unsupported_validation =
      controller->wait(*unsupported, 0, WaitEvidence::validation);
  assert(unsupported_validation &&
         unsupported_validation->status == WaitStatus::evidence_received &&
         !unsupported_validation->observation.validation_accepted);
  auto start_execution = controller->wait(*started, 0, WaitEvidence::execution);
  assert(start_execution &&
         start_execution->status == WaitStatus::evidence_received &&
         start_execution->observation.confirms_execution);
  auto start_state = controller->state(*started);
  assert(start_state && *start_state);
  assert((**start_state).value<DiscreteIO32>() == 3);

  auto status = controller->status();
  assert(status && (*made)->run_for(1'000'000));
  auto streaming = controller->state(*status);
  assert(streaming && *streaming);
  assert((**streaming).value<DiscreteIO32>() == 3);
  assert((**streaming).value<Bandwidth>() == settings.bandwidth);
  assert((**streaming).value<RFReferenceFrequency>() ==
         settings.center_frequency);
  assert((**streaming).value<Gain>() == settings.gain);
  assert((**streaming).value<SampleRate>() == settings.sample_rate);

  CommandOptions execution_only;
  execution_only.state = false;
  auto stopped = controller->stop(execution_only);
  assert(stopped && (*made)->run_for(1'000'000));
  assert(controllee->status() == SourceStatus::stopped);
  auto stop_execution = controller->wait(*stopped, 0, WaitEvidence::execution);
  assert(stop_execution && stop_execution->observation.confirms_execution);
  auto suppressed = controller->state(*stopped);
  assert(suppressed && !*suppressed);
  verify_capabilities();

  auto invalid_settings = settings;
  invalid_settings.bandwidth = *Hertz::from_integer(2'000'000);
  invalid_settings.sample_rate = *Hertz::from_integer(1'000'000);
  auto invalid = controller->configure(invalid_settings);
  assert(invalid && (*made)->run_for(1'000'000));
  auto rejected = controller->wait(*invalid, 0, WaitEvidence::execution);
  assert(rejected && !rejected->observation.confirms_execution);
  const auto unchanged = controllee->confirmed_state();
  assert(std::get<Hertz>(unchanged.fields[1].value) == settings.sample_rate);
  assert(std::get<Hertz>(unchanged.fields[5].value) == settings.bandwidth);

  auto late = controller->start({1000, 1});
  assert(late && (*made)->run_for(1'000'000));
  auto late_execution = controller->wait(*late, 0, WaitEvidence::execution);
  assert(late_execution && !late_execution->observation.confirms_execution);
  assert(controllee->status() == SourceStatus::stopped);

  auto cancellable = controller->start({1001, 0});
  assert(cancellable);
  assert(
      controller->cancel(*cancellable, QuerySelection{QueryField::streaming}));
  assert((*made)->run_for(1'000'000));
  auto cancelled =
      controller->wait(*cancellable, 0, WaitEvidence::cancellation_execution);
  assert(cancelled && cancelled->status == WaitStatus::evidence_received &&
         cancelled->observation.confirms_cancellation);
  assert(controllee->status() == SourceStatus::stopped);

  auto armed = controller->start({1001, 0});
  assert(armed && (*made)->run_for(1'000'000));
  auto competing = controller->start({1001, 0});
  assert(competing && (*made)->run_for(1'000'000));
  auto denied = controller->wait(*competing, 0, WaitEvidence::execution);
  assert(denied && !denied->observation.confirms_execution);
  const auto before = controllee->confirmed_state().version;
  auto during = controller->configure(settings);
  assert(during && (*made)->run_for(1'000'000));
  assert(controllee->confirmed_state().version == before);
  auto disarm = controller->stop();
  assert(disarm && (*made)->run_for(1'000'000));
  auto stopped_ack = controller->wait(*disarm, 0, WaitEvidence::execution);
  assert(stopped_ack && stopped_ack->observation.confirms_execution);
  assert((*made)->run_for(1'000'000'000, 1'000'000));
  assert(controllee->status() == SourceStatus::stopped);
  // At1ksample/s a full packet spans1.024s. Immediate stop must not wait
  // for that next data boundary.
  settings.sample_rate = *Hertz::from_integer(1000);
  settings.bandwidth = *Hertz::from_integer(500);
  auto slow = controller->configure(settings);
  assert(slow && (*made)->run_for(1'000'000));
  auto when =
      runtime::timing::add((*made)->clock_snapshot()->time,
                           runtime::timing::Duration{0, 25'000'000'000});
  assert(when);
  auto slow_start = controller->start(*when);
  assert(slow_start && (*made)->run_for(26'000'000));
  assert(controllee->status() == SourceStatus::running);
  auto immediate = controller->stop();
  assert(immediate && (*made)->run_for(1'000'000));
  assert(controllee->status() == SourceStatus::stopped);
}
