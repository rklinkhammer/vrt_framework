#include <cassert>
#include <vita/runtime/state/contracts.hpp>
#include <vita/runtime/transaction/cam.hpp>
using namespace vita;
using namespace vita::profiles::iq;
using namespace vita::runtime;
int main() {
  static_assert(sdr_unknown_oui == 0xffffff);
  static_assert(information_class(Profile::sdr_radio) == 0);
  static_assert(context_class(Profile::sdr_radio) == 0);
  static_assert(command_class(Profile::sdr_radio) == 0);
  static_assert(data_class(Profile::sdr_radio, 7) == 0);
  static_assert(sdr_burst_pairs == 262144);
  static_assert(sdr_trailer(SampleFrame::single) == 0x00c00000);
  static_assert(sdr_trailer(SampleFrame::first) == 0x00c00400);
  static_assert(sdr_trailer(SampleFrame::middle) == 0x00c00800);
  static_assert(sdr_trailer(SampleFrame::final) == 0x00c00c00);
  static_assert(sdr_sample_frame(0, sdr_burst_pairs) ==
                SampleFrame::single);
  static_assert(sdr_sample_frame(0, 1024) == SampleFrame::first);
  static_assert(sdr_sample_frame(1024, 1024) == SampleFrame::middle);
  static_assert(sdr_packet_pairs(sdr_burst_pairs - 2, 1024) == 2);
  static_assert(sdr_sample_frame(sdr_burst_pairs - 2, 2) ==
                SampleFrame::final);

  StateSnapshot state;
  state.profile = Profile::sdr_radio;
  for (auto &field : state.fields)
    field.validity = Validity::known;
  state.fields[0].value = std::uint32_t{1};
  state.fields[1].value = *Hertz::from_integer(1'000'000);
  state.fields[2].value = std::uint32_t{0};
  state.fields[3].value = PayloadFormat{0x200003cf00000000};
  state.fields[4].value = *Hertz::from_integer(100'000'000);
  state.fields[5].value = *Hertz::from_integer(800'000);
  state.fields[6].value = GainStages{};
  state.fields[7].value = std::uint32_t{2};
  assert(validate_snapshot(state));
  auto invalid_snapshot = state;
  invalid_snapshot.fields[0].value = std::uint32_t{5};
  assert(!validate_snapshot(invalid_snapshot));
  invalid_snapshot = state;
  invalid_snapshot.fields[3].value = PayloadFormat{0x200007df00000000};
  assert(!validate_snapshot(invalid_snapshot));

  assert(profile_field(Profile::sdr_radio, Bandwidth::id));
  assert(profile_field(Profile::sdr_radio, Gain::id));
  assert(!profile_field(Profile::frequency_tunable, Bandwidth::id));

  const auto bad_type = transaction::sdr_validate(
      SampleRate::id, SemanticValue{std::uint32_t{1}});
  assert(!bad_type.resolvable &&
         bad_type.diagnostics.errors == transaction::invalid_value);
  const auto bad_range = transaction::sdr_validate(
      Bandwidth::id, *Hertz::from_integer(2'000'001));
  assert(!bad_range.resolvable &&
         bad_range.diagnostics.errors == transaction::range_error);
  const auto bad_precision = transaction::sdr_validate(
      SampleRate::id, Hertz{(1'000'000ll << 20) + 1});
  assert(!bad_precision.resolvable &&
         bad_precision.diagnostics.warnings == transaction::precision);
  const auto bad_gain =
      transaction::sdr_validate(Gain::id, GainStages{0, 1});
  assert(!bad_gain.resolvable &&
         bad_gain.diagnostics.errors == transaction::invalid_value);
}
