#pragma once
#include <vita/core/error.hpp>
#include <cstdint>
#include <optional>

namespace vita {
struct RadiansQ7 {
    std::int16_t q7 = 0;
    friend constexpr bool operator==(RadiansQ7,RadiansQ7) = default;
};
struct PolarizationAngles {
    std::int16_t tilt_q7 = 0, ellipticity_q7 = 0;
    friend constexpr bool operator==(PolarizationAngles,PolarizationAngles) = default;
};
struct PointingAngles {
    std::uint16_t azimuth_q7 = 0;
    std::int16_t elevation_q7 = 0;
    friend constexpr bool operator==(PointingAngles,PointingAngles) = default;
};
struct SpatialReferenceValue {
    std::uint16_t user_id = 0;
    std::uint8_t reference = 0, beam = 0;
    friend constexpr bool operator==(SpatialReferenceValue,SpatialReferenceValue) = default;
};
// D-P14-1: lossless component codes only. No degree conversion or signedness
// interpretation is selected for the contradictory Beam Width specification.
struct BeamWidthCode {
    std::uint16_t horizontal_code = 0, vertical_code = 0;
    friend constexpr bool operator==(BeamWidthCode,BeamWidthCode) = default;
};
struct MetresQ6 {
    std::uint32_t q6 = 0;
    friend constexpr bool operator==(MetresQ6,MetresQ6) = default;
};
// A small explicit nullable value avoids implementation-specific std::optional
// empty-subobject interactions inside std::variant while retaining known/sentinel
// distinction. Dereference has the same engaged precondition as optional.
class OptionalQ7 {
    std::int16_t value_ = 0;
    bool known_ = false;
public:
    constexpr OptionalQ7() noexcept = default;
    constexpr OptionalQ7(std::nullopt_t) noexcept {}
    constexpr OptionalQ7(std::int16_t value) noexcept : value_(value),known_(true) {}
    constexpr OptionalQ7(std::optional<std::int16_t> value) noexcept
        : value_(value.value_or(0)),known_(value.has_value()) {}
    constexpr bool has_value() const noexcept { return known_; }
    constexpr explicit operator bool() const noexcept { return known_; }
    constexpr const std::int16_t& operator*() const noexcept { return value_; }
    constexpr std::int16_t value_or(std::int16_t fallback) const noexcept { return known_?value_:fallback; }
    constexpr Result<std::int16_t> get() const noexcept {
        if(!known_)return std::unexpected(Error{ErrorCode::invalid_state});return value_;
    }
    friend constexpr bool operator==(OptionalQ7 left,OptionalQ7 right) noexcept {
        return left.known_==right.known_ && (!left.known_ || left.value_==right.value_);
    }
};
static_assert(sizeof(OptionalQ7)<=4);
struct EbNoBerValue {
    OptionalQ7 ebno_q7{}, ber_q7{};
    friend constexpr bool operator==(EbNoBerValue,EbNoBerValue) = default;
};
// Units and single/window interpretation require Packet Class documentation.
struct ThresholdValue {
    std::int16_t stage1_q7 = 0, stage2_q7 = 0;
    friend constexpr bool operator==(ThresholdValue,ThresholdValue) = default;
};
enum class ThresholdMode { single_db, single_dbm, window_db, window_dbm };
constexpr Result<void> validate_threshold(ThresholdValue value,ThresholdMode mode) noexcept {
    switch(mode) {
    case ThresholdMode::single_db:
        if(value.stage2_q7==0)return {};break;
    case ThresholdMode::single_dbm:
        if(value.stage2_q7==INT16_MIN)return {};break;
    case ThresholdMode::window_db:case ThresholdMode::window_dbm:
        if(value.stage2_q7>value.stage1_q7)return {};break;
    }
    return std::unexpected(Error{ErrorCode::invalid_argument});
}
struct InterceptPointsValue {
    OptionalQ7 second_q7{}, third_q7{};
    friend constexpr bool operator==(InterceptPointsValue,InterceptPointsValue) = default;
};
struct SnrNoiseFigureValue {
    OptionalQ7 snr_q7{}, noise_figure_q7{};
    friend constexpr bool operator==(SnrNoiseFigureValue,SnrNoiseFigureValue) = default;
};
struct Unsigned64Bits {
    std::uint64_t bits = 0;
    friend constexpr bool operator==(Unsigned64Bits,Unsigned64Bits) = default;
};
struct VersionBuildValue {
    std::uint8_t year_since2000 = 0;
    std::uint16_t day = 1;
    std::uint8_t revision = 0;
    std::uint16_t user = 0;
    friend constexpr bool operator==(VersionBuildValue,VersionBuildValue) = default;
};
struct BufferSizeValue {
    std::uint32_t capacity_bytes = 0;
    std::uint8_t level = 0, status = 0;
    friend constexpr bool operator==(BufferSizeValue,BufferSizeValue) = default;
};
} // namespace vita
