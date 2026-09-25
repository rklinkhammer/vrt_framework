#include <cassert>
#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
using namespace vita;
struct Samples {
  std::uint64_t produced = 0, received = 0;
  std::size_t maximum = 0, burst = 0;
  static Result<void>
  produce(void *p, profiles::iq::SampleWriteWindow &window) noexcept {
    auto &self = *static_cast<Samples *>(p);
    assert(window.first_ordinal() == self.produced);
    assert(window.count() ==
           std::min<std::size_t>(self.maximum,
                                 self.burst - self.produced % self.burst));
    for (std::size_t i = 0; i < window.count(); ++i)
      assert(window.write_iq16(i, 8192, -8192));
    self.produced += window.count();
    return {};
  }
  static void
  receive(void *p, const runtime::context::BorrowedSignalRx &signal) noexcept {
    auto &self = *static_cast<Samples *>(p);
    const auto ps = self.received * 1'000'000'000'000ull / 1'000'003;
    assert(signal.sample_time ==
           runtime::timing::ProtocolTime(
               1000 + static_cast<std::uint64_t>((ps + 20'000'000'000) /
                                                 1'000'000'000'000ull),
               static_cast<std::uint64_t>((ps + 20'000'000'000) %
                                          1'000'000'000'000ull)));
    std::size_t bytes = 0;
    for (std::size_t f = 0; f < signal.fragment_count(); ++f) {
      auto data = signal.fragment(f);
      assert(data);
      bytes += data->size();
      for (std::size_t i = 0; i < data->size(); i += 4) {
        assert((*data)[i] == std::byte{0x20} &&
               (*data)[i + 1] == std::byte{0} &&
               (*data)[i + 2] == std::byte{0xe0} &&
               (*data)[i + 3] == std::byte{0});
      }
    }
    assert(bytes ==
           4 * std::min<std::size_t>(self.maximum,
                                     self.burst - self.received % self.burst));
    self.received += bytes / 4;
  }
};
int main() {
  for (const auto [maximum, burst] :
       std::array<std::pair<std::size_t, std::size_t>, 6>{{{1, 2},
                                                           {2, 3},
                                                           {1023, 2050},
                                                           {1024, 32},
                                                           {1024, 2050},
                                                           {1024, 262144}}}) {
    Samples samples{0, 0, maximum, burst};
    auto config = profiles::iq::lab::config(0xabcdef);
    assert(config);
    config->clock.epoch = runtime::timing::Epoch::utc;
    profiles::iq::lab::PoolCounts counts;
    counts.payload_bytes = 4096;
    counts.rx_data_bytes = 8192;
    auto pools = profiles::iq::lab::pools(counts);
    assert(pools);
    auto made =
        VitaRuntime<1, 4, 32, 65536>::create(*config, std::move(*pools));
    assert(made);
    StreamConfig stream;
    stream.sid = 1;
    stream.controller_id = 2;
    stream.controllee_id = 3;
    stream.profile = profiles::iq::Profile::sdr_radio;
    stream.trailer = true;
    stream.ip_mtu = 4200;
    stream.maximum_samples_per_packet = maximum;
    stream.burst_pairs = burst;
    stream.source = {&samples, Samples::produce};
    stream.receiver = {&samples, Samples::receive};
    auto radio = (*made)->add_controllee(stream);
    assert(radio);
    auto controller = (*made)->add_controller(*radio);
    assert(controller);
    assert((*made)->observe_pps({0}, {1000, 0}));
    SdrRadioSettings settings;
    settings.sample_rate = *Hertz::from_integer(1'000'003);
    auto configured = controller->configure(settings);
    assert(configured);
    for (int i = 0; i < 8; ++i)
      assert((*made)->progress({0}));
    assert(radio->confirmed_state().version == 1);
    auto start = controller->start({1000, 20'000'000'000});
    assert(start);
    for (int i = 0; i < 8; ++i)
      assert((*made)->progress({0}));
    std::uint64_t now = 20'000'000;
    while (samples.received < 2 * burst) {
      for (int i = 0; i < 4; ++i)
        assert((*made)->progress({now}));
      assert(samples.produced > 0);
      now = 20'000'000 +
            (samples.produced * 1'000'000'000ull + 1'000'002) / 1'000'003;
    }
    assert(samples.received == 2 * burst);
    assert(radio->metrics().skipped_samples == 0);
  }
}
