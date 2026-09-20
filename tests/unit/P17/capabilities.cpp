#include <cassert>
#include <vita/codec/packet.hpp>
#include <vita/profiles/iq/graphx.hpp>
using namespace vita;
using namespace vita::codec;
using namespace vita::profiles::iq;
static bool bandwidth_rule(void *, Hertz bandwidth, Hertz rate) noexcept {
  return bandwidth.q20 <= rate.q20;
}
int main() {
  GraphxCapabilities capabilities;
  capabilities.sample_rate_values.step = 1'000ll << 20;
  assert(!capabilities.supports(*Hertz::from_integer(500),
                                *Hertz::from_integer(1'001)));
  capabilities.bandwidth_supported = bandwidth_rule;
  assert(capabilities.validate());
  auto invalid_domain = capabilities;
  invalid_domain.sample_rate_values.choices[0] = 0;
  invalid_domain.sample_rate_values.count = 1;
  assert(!invalid_domain.validate());
  invalid_domain = capabilities;
  invalid_domain.gain_values.step = 7;
  assert(!invalid_domain.validate());
  auto discrete = capabilities;
  discrete.sample_rate_values.count = 2;
  discrete.sample_rate_values.choices[0] = discrete.sample_rate.minimum.q20;
  discrete.sample_rate_values.choices[1] = discrete.sample_rate.maximum.q20;
  assert(discrete.validate());
  assert(!discrete.supports(*Hertz::from_integer(500),
                            *Hertz::from_integer(10'000)));

  assert(capabilities.supports(*Hertz::from_integer(800'000),
                               *Hertz::from_integer(1'000'000)));
  assert(!capabilities.supports(*Hertz::from_integer(1'100'000),
                                *Hertz::from_integer(1'000'000)));
  constexpr std::array selectors{Bandwidth::id, RFReferenceFrequency::id,
                                 Gain::id, SampleRate::id};
  auto response = graphx_capability_response(capabilities, selectors);
  assert(response);
  Envelope envelope;
  envelope.type = PacketType::context;
  envelope.stream_id = 1;
  envelope.timestamp = {Tsi::utc, Tsf::picoseconds, 1000, 0};
  std::array<std::byte, 256> wire{};
  auto size = encode_packet(envelope, response->freeze(), wire);
  assert(size);
  auto decoded = decode_packet(Bytes{wire}.first(*size));
  assert(decoded && decoded->fields.size() == 8);
  for (auto selector : selectors) {
    bool minimum = false, maximum = false;
    for (std::size_t i = 0; i < decoded->fields.size(); ++i) {
      const auto &field = decoded->fields[i];
      if (field.id == selector) {
        minimum |= field.attribute == Attribute::minimum;
        maximum |= field.attribute == Attribute::maximum;
      }
    }
    assert(minimum && maximum);
  }
  constexpr std::array unknown{StateEvent::id};
  auto rejected = graphx_capability_response(capabilities, unknown);
  assert(!rejected &&
         rejected.error().code == ErrorCode::unsupported_capability);
}
