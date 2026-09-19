#pragma once
#include <vita/core/error.hpp>
#include <array>
#include <cstdint>
#include <limits>
#include <variant>
namespace vita {
struct Hertz {
    std::int64_t q20 = 0;
    static constexpr Result<Hertz> from_integer(std::uint64_t hz) noexcept {
        if (hz > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) / (1u << 20))
            return std::unexpected(Error{ErrorCode::overflow});
        return Hertz{static_cast<std::int64_t>(hz << 20)};
    }
    friend constexpr bool operator==(Hertz, Hertz) = default;
};
struct PayloadFormat { std::uint64_t bits = 0; friend constexpr bool operator==(PayloadFormat, PayloadFormat) = default; };
struct FieldId {
    std::uint8_t cif = 0, bit = 0;
    friend constexpr bool operator==(FieldId, FieldId) = default;
};
struct ReferencePoint { using value_type = std::uint32_t; static constexpr FieldId id{0,30}; };
struct SampleRate { using value_type = Hertz; static constexpr FieldId id{0,21}; };
struct StateEvent { using value_type = std::uint32_t; static constexpr FieldId id{0,16}; };
struct DataPayloadFormat { using value_type = PayloadFormat; static constexpr FieldId id{0,15}; };
enum class Attribute : std::uint8_t { current, average, median, standard_deviation, maximum, minimum, precision, accuracy, first_derivative, second_derivative, third_derivative, probability, belief };
constexpr std::uint32_t attribute_bit(Attribute a) noexcept {
    auto index = static_cast<unsigned>(a);
    return index < 13 ? std::uint32_t{1} << (31 - index) : 0;
}
inline constexpr std::uint32_t supported_scalar_attributes = attribute_bit(Attribute::current) | attribute_bit(Attribute::minimum) | attribute_bit(Attribute::maximum);
struct VariableValue {
    std::array<std::uint32_t,16> words{};
    std::size_t size = 0;
    friend constexpr bool operator==(const VariableValue&, const VariableValue&) = default;
};
using SemanticValue = std::variant<std::uint32_t, Hertz, PayloadFormat>;
struct FieldDescriptor { FieldId id; std::size_t words; std::size_t variant_index; std::uint32_t attributes; };
inline constexpr std::array baseline_descriptors{
    FieldDescriptor{ReferencePoint::id,1,0,attribute_bit(Attribute::current)},
    FieldDescriptor{SampleRate::id,2,1,supported_scalar_attributes},
    FieldDescriptor{StateEvent::id,1,0,attribute_bit(Attribute::current)},
    FieldDescriptor{DataPayloadFormat::id,2,2,attribute_bit(Attribute::current)} };
constexpr const FieldDescriptor* descriptor(FieldId id) noexcept {
    for (const auto& d : baseline_descriptors) if (d.id == id) return &d;
    return nullptr;
}
// CIF presence and Context change indicators are not field selectors.
constexpr bool valid_field_selector(FieldId id) noexcept {
    if (id.cif > 3 || id.bit > 31) return false;
    if (id.cif == 0) return id.bit >= 8 && id.bit <= 30;
    // Unknown generic field extents remain distinguishable from malformed IDs.
    return true;
}
constexpr bool field_before(FieldId a, FieldId b) noexcept { return a.cif < b.cif || (a.cif == b.cif && a.bit > b.bit); }
constexpr Result<void> validate_value(FieldId id, const SemanticValue& value) noexcept {
    const auto* d = descriptor(id);
    if (!d) return std::unexpected(Error{ErrorCode::unsupported_layout});
    if (value.index() != d->variant_index) return std::unexpected(Error{ErrorCode::invalid_argument});
    if (id == SampleRate::id && std::get<Hertz>(value).q20 < 0) return std::unexpected(Error{ErrorCode::invalid_argument});
    return {};
}
} // namespace vita
