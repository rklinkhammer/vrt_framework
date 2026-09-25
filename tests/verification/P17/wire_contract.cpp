#include <array>
#include <cassert>
#include <cstring>
#include <vita/codec/packet.hpp>
#include <vita/codec/prologue.hpp>
#include <vita/profiles/iq/Sdr.hpp>
#include <vita/profiles/iq/profile.hpp>
#include <vita/runtime/context/publisher.hpp>
#include <vita/runtime/stream/counters.hpp>
#include <vita/runtime/stream/routing.hpp>
#include <vita/runtime/transaction/outcomes.hpp>

using namespace vita;
using namespace vita::codec;
using namespace vita::profiles::iq;

template <std::size_t N>
static auto words(const std::array<std::uint32_t, N> &input) {
  std::array<std::byte, N * 4> output{};
  for (std::size_t i = 0; i < N; ++i)
    for (unsigned byte = 0; byte < 4; ++byte)
      output[i * 4 + byte] = std::byte(input[i] >> (24 - 8 * byte));
  return output;
}

template <std::size_t Pairs>
static auto literal_data(std::uint32_t header, SampleFrame frame) {
  std::array<std::uint32_t, Pairs + 8> packet{};
  packet[0] = header;
  packet[1] = 1;
  packet[2] = 0x00ffffff;
  packet[3] = 0;
  packet[4] = 1000;
  packet[5] = 0;
  packet[6] = 0;
  for (std::size_t i = 0; i < Pairs; ++i)
    packet[7 + i] = 0x7fff8000;
  packet.back() = 0x00c00000u | (static_cast<std::uint32_t>(frame) << 10);
  return words(packet);
}

template <std::size_t Pairs>
static void verify_data(std::uint32_t header, SampleFrame frame) {
  const auto expected = literal_data<Pairs>(header, frame);
  const auto decoded = decode_envelope(expected);
  assert(decoded && decoded->envelope.type == PacketType::signal);
  assert(decoded->envelope.stream_id == 1);
  assert((decoded->envelope.class_id == ClassId{0x00ffffff, 0, 0}));
  assert(decoded->envelope.timestamp.tsi == Tsi::utc);
  assert(decoded->envelope.timestamp.tsf == Tsf::picoseconds);
  assert(decoded->payload.size() == Pairs * 4);
  assert(decoded->trailer && *decoded->trailer == sdr_trailer(frame));

  Envelope envelope;
  envelope.type = PacketType::signal;
  envelope.stream_id = 1;
  envelope.class_id = ClassId{0x00ffffff, 0, 0};
  envelope.timestamp = {Tsi::utc, Tsf::picoseconds, 1000, 0};
  envelope.trailer = true;
  envelope.packet_count = static_cast<std::uint8_t>((header >> 16) & 0xf);
  std::array<std::byte, 28> prologue{};
  const auto encoded = encode_prologue(envelope, Pairs * 4, prologue);
  assert(encoded && *encoded == prologue.size());
  assert(std::memcmp(prologue.data(), expected.data(), prologue.size()) == 0);

  auto mismatch = decode_packet(
      expected, DecodeOptions{.expected_class = ClassId{0x000000, 0, 0}});
  assert(!mismatch &&
         mismatch.error().code == ErrorCode::unsupported_capability);
}

int main() {
  verify_data<1>(0x1c600009, SampleFrame::single);
  verify_data<2>(0x1c61000a, SampleFrame::first);
  verify_data<1023>(0x1c620407, SampleFrame::middle);
  verify_data<1024>(0x1c630408, SampleFrame::final);

  assert(sdr_sample_frame(0, sdr_burst_pairs) == SampleFrame::single);
  assert(sdr_sample_frame(0, 1024) == SampleFrame::first);
  assert(sdr_sample_frame(1024, 1024) == SampleFrame::middle);
  assert(sdr_packet_pairs(sdr_burst_pairs - 1, 1024) == 1);
  assert(sdr_sample_frame(sdr_burst_pairs - 1, 1) == SampleFrame::final);
  std::uint64_t ordinal = 0;
  std::size_t packets = 0;
  while (ordinal < sdr_burst_pairs) {
    const auto pairs = sdr_packet_pairs(ordinal, 1024);
    const auto expected = ordinal == 0 ? SampleFrame::first
                          : ordinal + pairs == sdr_burst_pairs
                              ? SampleFrame::final
                              : SampleFrame::middle;
    assert(sdr_sample_frame(ordinal, pairs) == expected);
    ordinal += pairs;
    ++packets;
  }
  assert(ordinal == sdr_burst_pairs && packets == 256);

  ordinal = sdr_burst_pairs - 1025;
  assert(sdr_packet_pairs(ordinal, 1024) == 1024);
  assert(sdr_sample_frame(ordinal, 1024) == SampleFrame::middle);
  ordinal += 1024;
  assert(sdr_packet_pairs(ordinal, 1024) == 1);
  assert(sdr_sample_frame(ordinal, 1) == SampleFrame::final);

  const auto context = words(std::array<std::uint32_t, 13>{
      0x4060000d, 1, 1000, 0, 0, 0x28a00000, 0x000000c3, 0x50000000, 0x00005f5e,
      0x10000000, 0x00000500, 0x000000f4, 0x24000000});
  const auto current = decode_packet(context);
  assert(current && !current->envelope.envelope.class_id &&
         current->fields.size() == 4);
  constexpr std::array context_fields{Bandwidth::id, RFReferenceFrequency::id,
                                      Gain::id, SampleRate::id};
  for (std::size_t i = 0; i < context_fields.size(); ++i)
    assert(current->fields[i].id == context_fields[i]);

  runtime::context::ContextFrame frame;
  frame.state.profile = profiles::iq::Profile::sdr_radio;
  frame.time = {1000, 0};
  frame.epoch = Tsi::utc;
  frame.time_known = true;
  frame.valid = true;
  frame.change = false;
  for (std::size_t i = 0; i < current->fields.size(); ++i) {
    auto id = current->fields[i].id;
    frame.state.fields[runtime::field_index(id)] = {
        id, *current->fields[i].value(), runtime::Validity::known};
  }
  Envelope context_envelope;
  context_envelope.type = PacketType::context;
  context_envelope.stream_id = 1;
  std::array<std::byte, 256> actual_context{};
  auto context_size =
      runtime::context::encode_context(frame, context_envelope, actual_context);
  assert(context_size && *context_size == context.size() &&
         std::memcmp(actual_context.data(), context.data(), context.size()) ==
             0);

  const auto configure = words(std::array<std::uint32_t, 17>{
      0x60600011, 1, 1000, 0, 0, 0xa11f0000, 0x11223344, 1, 1, 0x28a00000,
      0x000000c3, 0x50000000, 0x00005f5e, 0x10000000, 0x00000500, 0x000001e8,
      0x48000000});
  const auto configured = decode_packet(configure);
  assert(configured && !configured->envelope.envelope.class_id &&
         configured->fields.size() == 4);
  constexpr std::array configure_fields{Bandwidth::id, RFReferenceFrequency::id,
                                        Gain::id, SampleRate::id};
  for (std::size_t i = 0; i < configure_fields.size(); ++i)
    assert(configured->fields[i].id == configure_fields[i]);

  const auto start = words(
      std::array<std::uint32_t, 12>{0x6060000c, 1, 1000, 0x0000000b, 0xa43b7400,
                                    0xa11f1000, 0x11223345, 1, 1, 2, 64, 3});
  const auto started = decode_packet(start);
  assert(started && !started->envelope.envelope.class_id &&
         started->fields.size() == 1);
  assert(started->fields[0].id == DiscreteIO32::id);
  assert(std::get<std::uint32_t>(*started->fields[0].value()) == 3);

  const auto stop = words(std::array<std::uint32_t, 12>{
      0x6060000c, 1, 1000, 0, 0, 0xa11f0000, 0x11223346, 1, 1, 2, 64, 2});
  const auto stopped = decode_packet(stop);
  assert(stopped && !stopped->envelope.envelope.class_id &&
         stopped->fields.size() == 1);
  assert(stopped->fields[0].id == DiscreteIO32::id);
  assert(std::get<std::uint32_t>(*stopped->fields[0].value()) == 2);

  const auto status_query = words(std::array<std::uint32_t, 11>{
      0x6060000b, 1, 1000, 0, 0, 0xa0040000, 0x11223344, 1, 2, 2, 0x40});
  const auto queried = decode_packet(status_query);
  assert(queried && queried->fields.size() == 1 &&
         queried->fields[0].id == DiscreteIO32::id &&
         queried->fields[0].kind == BodyKind::selectors);

  constexpr RequestContext sdr_request{0xa11f0000};
  const auto execution_ack = words(std::array<std::uint32_t, 9>{
      0x64630009, 1, 1000, 0, 0, 0xa1080400, 0x11223344, 1, 2});
  const auto execution =
      decode_packet(execution_ack, DecodeOptions{sdr_request});
  assert(execution && execution->envelope.envelope.ack &&
         !execution->envelope.envelope.class_id && execution->fields.empty());

  const auto status_ack = words(std::array<std::uint32_t, 12>{
      0x6464000c, 1, 1000, 0, 1, 0xa1040000, 0x11223344, 1, 2, 2, 0x40, 3});
  const auto acknowledged =
      decode_packet(status_ack, DecodeOptions{sdr_request});
  assert(acknowledged && acknowledged->fields.size() == 1 &&
         acknowledged->fields[0].id == DiscreteIO32::id &&
         std::get<std::uint32_t>(*acknowledged->fields[0].value()) == 3);

  const auto diagnostic_ack = words(
      std::array<std::uint32_t, 11>{0x6465000b, 1, 1000, 0, 0, 0xa1110000,
                                    0x11223344, 1, 2, 0x20000000, 0x90000000});
  const auto diagnostic =
      decode_packet(diagnostic_ack, DecodeOptions{sdr_request});
  assert(diagnostic && diagnostic->fields.size() == 1 &&
         diagnostic->fields[0].id == Bandwidth::id &&
         *diagnostic->fields[0].diagnostic() ==
             (runtime::transaction::not_executed |
              runtime::transaction::range_error));

  const auto capability_query = words(
      std::array<std::uint32_t, 11>{0x6060000b, 1, 1000, 0, 0, 0xa0040000,
                                    0x11223344, 1, 2, 0x28a00080, 0x0c000000});
  const auto capability_request = decode_packet(capability_query);
  assert(capability_request && capability_request->fields.size() == 8);
  const auto capability_response = words(std::array<std::uint32_t, 25>{
      0x64600019, 1,          1000,       0,          0,
      0xa0040400, 0x11223344, 1,          2,          0x28a00080,
      0x0c000000, 0x000001e8, 0x48000000, 0,          0x00100000,
      0x00165a0b, 0xc0000000, 0x000000f4, 0x24000000, 0x00001e00,
      0x0000e200, 0x000001e8, 0x48000000, 0,          0x3e800000});
  const auto capabilities = decode_packet(
      capability_response, DecodeOptions{RequestContext{0xa0040000}});
  assert(capabilities && !capabilities->envelope.envelope.class_id &&
         capabilities->fields.size() == 8);
  for (const auto selector : configure_fields) {
    bool minimum = false, maximum = false;
    for (std::size_t i = 0; i < capabilities->fields.size(); ++i)
      if (capabilities->fields[i].id == selector) {
        minimum |= capabilities->fields[i].attribute == Attribute::minimum;
        maximum |= capabilities->fields[i].attribute == Attribute::maximum;
      }
    assert(minimum && maximum);
  }

  profiles::iq::SdrCapabilities supported;
  runtime::transaction::AckRecord ranges;
  ranges.request = capability_request->envelope.envelope;
  ranges.cam = *runtime::transaction::Cam::parse(
      ranges.request, runtime::transaction::Profile::sdr_radio);
  ranges.kind = runtime::transaction::AckKind::state;
  ranges.selected_mask = 0x72;
  ranges.sdr_capabilities = &supported;
  ranges.time_known = true;
  ranges.epoch = Tsi::utc;
  ranges.time = {1000, 0};
  ranges.scheduled_or_executed = true;
  std::array<std::byte, 256> actual_ranges{};
  auto range_size =
      runtime::transaction::encode_response(ranges, actual_ranges);
  assert(range_size && *range_size == capability_response.size() &&
         std::memcmp(actual_ranges.data(), capability_response.data(),
                     capability_response.size()) == 0);

  runtime::RouteRegistry<4> routes;
  runtime::Route data_route;
  data_route.key = {
      {7, 1}, 1, PacketType::signal, ClassId{sdr_unknown_oui, 0, 0}};
  data_route.receive = [](void *, const PacketView &,
                          const memory::RxEnvelope &) noexcept {};
  assert(routes.add(data_route));
  auto context_route = data_route;
  context_route.key.type = PacketType::context;
  context_route.key.packet_class.reset();
  assert(routes.add(context_route));
  routes.freeze();
  Envelope routed_data;
  routed_data.type = PacketType::signal;
  routed_data.stream_id = 1;
  routed_data.class_id = ClassId{sdr_unknown_oui, 0, 0};
  assert(routes.lookup({7, 1}, routed_data));
  routed_data.class_id.reset();
  assert(!routes.lookup({7, 1}, routed_data));
  routed_data.class_id = ClassId{0, 0, 0};
  assert(!routes.lookup({7, 1}, routed_data));
  Envelope routed_context;
  routed_context.type = PacketType::context;
  routed_context.stream_id = 1;
  assert(routes.lookup({7, 1}, routed_context));
  routed_context.class_id = ClassId{sdr_unknown_oui, 0, 0};
  assert(!routes.lookup({7, 1}, routed_context));

  runtime::CounterRegistry<4> counters;
  constexpr runtime::CounterKey data_counter{7, 1, PacketType::signal};
  constexpr runtime::CounterKey context_counter{7, 1, PacketType::context};
  constexpr runtime::CounterKey command_counter{8, 1, PacketType::command};
  constexpr std::array counter_keys{data_counter, context_counter,
                                    command_counter};
  assert(counters.install(counter_keys));
  counters.freeze();
  for (unsigned count = 0; count < 17; ++count) {
    assert(*counters.next(data_counter) == (count & 15));
    assert(counters.accept(data_counter, count & 15));
  }
  assert(*counters.next(data_counter) == 1);
  assert(*counters.next(context_counter) == 0);
  assert(*counters.next(command_counter) == 0);
  assert(counters.accept(context_counter, 0));
  assert(*counters.next(context_counter) == 1);
  assert(*counters.next(command_counter) == 0);
}
