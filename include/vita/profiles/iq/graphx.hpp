#pragma once
#include <array>
#include <span>
#include <vita/fields/packet.hpp>
#include <vita/profiles/iq/profile.hpp>

namespace vita::profiles::iq {
template <class Value> struct CapabilityRange {
  Value minimum{};
  Value maximum{};
  friend constexpr bool operator==(const CapabilityRange &,
                                   const CapabilityRange &) = default;
};
// Values use the field's native fixed-point units (Q20 Hz or Q7 dB).
// Nonzero step is anchored at origin; a nonempty choices list further restricts
// admission. These are local device contracts, not CIF7 Precision attributes.
template <class Rep> struct SupportedValues {
  Rep step{}, origin{};
  std::array<Rep, 16> choices{};
  std::size_t count = 0;
  bool on_grid(Rep value) const noexcept {
    if (step <= 0)
      return step == 0;
    auto remainder = value % step, base = origin % step;
    if (remainder < 0)
      remainder += step;
    if (base < 0)
      base += step;
    return remainder == base;
  }
  bool valid() const noexcept {
    if (step < 0 || count > choices.size())
      return false;
    for (std::size_t i = 0; i < count; ++i) {
      if (!on_grid(choices[i]))
        return false;
      for (std::size_t j = 0; j < i; ++j)
        if (choices[j] == choices[i])
          return false;
    }
    return true;
  }
  bool accepts(Rep value) const noexcept {
    if (!valid() || !on_grid(value))
      return false;
    if (!count)
      return true;
    for (std::size_t i = 0; i < count; ++i)
      if (value == choices[i])
        return true;
    return false;
  }
};
struct GraphxCapabilities {
  CapabilityRange<Hertz> center_frequency{
      *Hertz::from_integer(minimum_center_hz),
      *Hertz::from_integer(maximum_center_hz)};
  CapabilityRange<Hertz> sample_rate{*Hertz::from_integer(1'000),
                                     *Hertz::from_integer(2'000'000)};
  CapabilityRange<Hertz> bandwidth{*Hertz::from_integer(1),
                                   *Hertz::from_integer(2'000'000)};
  CapabilityRange<GainStages> gain{{-60 * 128, 0}, {60 * 128, 0}};
  SupportedValues<std::int64_t> center_values{}, sample_rate_values{1ll << 20},
      bandwidth_values{};
  SupportedValues<std::int16_t> gain_values{};
  void *constraint_context = nullptr;
  bool (*bandwidth_supported)(void *, Hertz, Hertz) noexcept = nullptr;

  Result<void> validate() const noexcept {
    const auto ordered = [](Hertz range_minimum, Hertz range_maximum) {
      return range_minimum.q20 >= 0 && range_minimum.q20 <= range_maximum.q20;
    };
    if (!ordered(center_frequency.minimum, center_frequency.maximum) ||
        !ordered(sample_rate.minimum, sample_rate.maximum) ||
        !ordered(bandwidth.minimum, bandwidth.maximum) ||
        gain.minimum.stage1_q7 > gain.maximum.stage1_q7 ||
        gain.minimum.stage2_q7 || gain.maximum.stage2_q7 ||
        !center_values.valid() || !sample_rate_values.valid() ||
        !bandwidth_values.valid() || !gain_values.valid())
      return std::unexpected(Error{ErrorCode::invalid_argument});
    constexpr std::int64_t unit = 1ll << 20;
    if (center_frequency.minimum.q20 <
            static_cast<std::int64_t>(minimum_center_hz) * unit ||
        center_frequency.maximum.q20 >
            static_cast<std::int64_t>(maximum_center_hz) * unit ||
        sample_rate.minimum.q20 < 1000 * unit ||
        sample_rate.maximum.q20 > 2'000'000 * unit ||
        bandwidth.minimum.q20 <= 0 ||
        bandwidth.maximum.q20 > 2'000'000 * unit ||
        gain.minimum.stage1_q7 < -60 * 128 || gain.maximum.stage1_q7 > 60 * 128)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    const auto domain = [](const auto &values, auto minimum,
                           auto maximum) noexcept {
      if (!values.accepts(minimum) || !values.accepts(maximum))
        return false;
      for (std::size_t i = 0; i < values.count; ++i)
        if (values.choices[i] < minimum || values.choices[i] > maximum)
          return false;
      return true;
    };
    if (!domain(center_values, center_frequency.minimum.q20,
                center_frequency.maximum.q20) ||
        !domain(sample_rate_values, sample_rate.minimum.q20,
                sample_rate.maximum.q20) ||
        !domain(bandwidth_values, bandwidth.minimum.q20,
                bandwidth.maximum.q20) ||
        !domain(gain_values, gain.minimum.stage1_q7, gain.maximum.stage1_q7))
      return std::unexpected(Error{ErrorCode::invalid_argument});
    return {};
  }
  bool supports(Hertz requested_bandwidth,
                Hertz requested_sample_rate) const noexcept {
    if (requested_bandwidth.q20 > requested_sample_rate.q20 ||
        !bandwidth_values.accepts(requested_bandwidth.q20) ||
        !sample_rate_values.accepts(requested_sample_rate.q20) ||
        requested_bandwidth.q20 < bandwidth.minimum.q20 ||
        requested_bandwidth.q20 > bandwidth.maximum.q20 ||
        requested_sample_rate.q20 < sample_rate.minimum.q20 ||
        requested_sample_rate.q20 > sample_rate.maximum.q20)
      return false;
    return !bandwidth_supported ||
           bandwidth_supported(constraint_context, requested_bandwidth,
                               requested_sample_rate);
  }
};
constexpr bool graphx_capability_field(FieldId selector) noexcept {
  return selector == RFReferenceFrequency::id || selector == SampleRate::id ||
         selector == Bandwidth::id || selector == Gain::id;
}
template <class Packet>
inline Result<void> populate_graphx_capability_response(
    Packet &response, const GraphxCapabilities &capabilities,
    std::span<const FieldId> selectors) noexcept {
  if (selectors.empty() || selectors.size() > 4)
    return std::unexpected(Error{ErrorCode::invalid_argument});
  std::array<AttributeValue, 8> values{};
  std::size_t count = 0;
  for (const auto selector : selectors) {
    SemanticValue minimum, maximum;
    if (selector == RFReferenceFrequency::id) {
      minimum = capabilities.center_frequency.minimum;
      maximum = capabilities.center_frequency.maximum;
    } else if (selector == SampleRate::id) {
      minimum = capabilities.sample_rate.minimum;
      maximum = capabilities.sample_rate.maximum;
    } else if (selector == Bandwidth::id) {
      minimum = capabilities.bandwidth.minimum;
      maximum = capabilities.bandwidth.maximum;
    } else if (selector == Gain::id) {
      minimum = capabilities.gain.minimum;
      maximum = capabilities.gain.maximum;
    } else {
      return std::unexpected(Error{ErrorCode::unsupported_capability});
    }
    auto seeded = response.set_value(selector, minimum);
    if (!seeded)
      return std::unexpected(seeded.error());
    values[count++] = {selector, Attribute::minimum, minimum};
    values[count++] = {selector, Attribute::maximum, maximum};
  }
  return response.with_attributes(
      attribute_bit(Attribute::minimum) | attribute_bit(Attribute::maximum),
      std::span<const AttributeValue>{values}.first(count));
}
inline Result<ContextPacket>
graphx_capability_response(const GraphxCapabilities &capabilities,
                           std::span<const FieldId> selectors) noexcept {
  auto valid = capabilities.validate();
  if (!valid)
    return std::unexpected(valid.error());
  ContextPacket response;
  auto attributed =
      populate_graphx_capability_response(response, capabilities, selectors);
  if (!attributed)
    return std::unexpected(attributed.error());
  return response;
}
} // namespace vita::profiles::iq
