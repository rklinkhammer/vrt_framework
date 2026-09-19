#pragma once
#include <vita/core/error.hpp>
#include <vita/fields/structured.hpp>
#include <vita/fields/cif1.hpp>
#include <vita/fields/cif3.hpp>
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
// Exact wire-scale values; conversion and stream-specific interpretation are explicit.
struct DecibelsQ7 {
    std::int16_t q7 = 0;
    friend constexpr bool operator==(DecibelsQ7, DecibelsQ7) = default;
};
struct GainStages {
    std::int16_t stage1_q7 = 0, stage2_q7 = 0;
    friend constexpr bool operator==(GainStages, GainStages) = default;
};
struct Femtoseconds {
    std::int64_t count = 0;
    friend constexpr bool operator==(Femtoseconds, Femtoseconds) = default;
};
struct CelsiusQ6 {
    std::int16_t q6 = 0;
    friend constexpr bool operator==(CelsiusQ6, CelsiusQ6) = default;
};
struct DeviceIdentifierValue {
    std::uint32_t oui = 0;
    std::uint16_t device_code = 0;
    friend constexpr bool operator==(DeviceIdentifierValue, DeviceIdentifierValue) = default;
};
struct FieldId {
    std::uint8_t cif = 0, bit = 0;
    friend constexpr bool operator==(FieldId, FieldId) = default;
};
enum class BodyKind;
template<BodyKind, std::size_t, std::size_t> class PacketBuilder;
class NativeSlice {
    std::uint32_t offset_ = 0, bytes_ = 0;
    constexpr NativeSlice(std::uint32_t offset,std::uint32_t bytes) noexcept : offset_(offset),bytes_(bytes) {}
    template<BodyKind, std::size_t, std::size_t> friend class PacketBuilder;
public:
    constexpr std::uint32_t offset() const noexcept { return offset_; }
    constexpr std::uint32_t bytes() const noexcept { return bytes_; }
    friend constexpr bool operator==(NativeSlice,NativeSlice) = default;
};
struct FormattedGPS { using value_type=GeolocationValue; using view_type=GeolocationValue; static constexpr FieldId id{0,14}; };
struct FormattedINS { using value_type=GeolocationValue; using view_type=GeolocationValue; static constexpr FieldId id{0,13}; };
struct ECEFEphemeris { using value_type=EphemerisValue; using view_type=EphemerisValue; static constexpr FieldId id{0,12}; };
struct RelativeEphemeris { using value_type=EphemerisValue; using view_type=EphemerisValue; static constexpr FieldId id{0,11}; };
struct GPSASCII { using value_type=GpsAsciiInput; using view_type=GpsAsciiView; static constexpr FieldId id{0,9}; };
struct ContextAssociationLists { using value_type=AssociationListsInput; using view_type=AssociationListsView; static constexpr FieldId id{0,8}; };
constexpr bool structured_field(FieldId id) noexcept {
    return (id.cif==0 && ((id.bit>=11 && id.bit<=14) || id.bit==9 || id.bit==8)) ||
           (id.cif==2 && (id.bit==24 || id.bit==22)) ||
           (id.cif==3 && (id.bit==17 || id.bit==16)) ||
           (id.cif==1 && (id.bit==28 || id.bit==10 || id.bit==9 || id.bit==7));
}
struct ReferencePoint { using value_type = std::uint32_t; static constexpr FieldId id{0,30}; };
struct SampleRate { using value_type = Hertz; static constexpr FieldId id{0,21}; };
struct StateEvent { using value_type = std::uint32_t; static constexpr FieldId id{0,16}; };
struct DataPayloadFormat { using value_type = PayloadFormat; static constexpr FieldId id{0,15}; };
struct Bandwidth { using value_type = Hertz; static constexpr FieldId id{0,29}; };
struct IFReferenceFrequency { using value_type = Hertz; static constexpr FieldId id{0,28}; };
struct RFReferenceFrequency { using value_type = Hertz; static constexpr FieldId id{0,27}; };
struct RFReferenceFrequencyOffset { using value_type = Hertz; static constexpr FieldId id{0,26}; };
struct IFBandOffset { using value_type = Hertz; static constexpr FieldId id{0,25}; };
struct ReferenceLevel { using value_type = DecibelsQ7; static constexpr FieldId id{0,24}; };
struct Gain { using value_type = GainStages; static constexpr FieldId id{0,23}; };
struct OverRangeCount { using value_type = std::uint32_t; static constexpr FieldId id{0,22}; };
struct TimestampAdjustment { using value_type = Femtoseconds; static constexpr FieldId id{0,20}; };
// Seconds in the enclosing packet's TSI epoch, not an implicit GPS timestamp.
struct TimestampCalibrationTime { using value_type = std::uint32_t; static constexpr FieldId id{0,19}; };
struct Temperature { using value_type = CelsiusQ6; static constexpr FieldId id{0,18}; };
struct DeviceIdentifier { using value_type = DeviceIdentifierValue; static constexpr FieldId id{0,17}; };
struct EphemerisReferenceId { using value_type = std::uint32_t; static constexpr FieldId id{0,10}; };
struct PhaseOffset { using value_type=RadiansQ7; static constexpr FieldId id{1,31}; };
struct Polarization { using value_type=PolarizationAngles; static constexpr FieldId id{1,30}; };
struct PointingVector3D { using value_type=PointingAngles; static constexpr FieldId id{1,29}; };
struct SpatialScanType { using value_type=std::uint32_t; static constexpr FieldId id{1,27}; };
struct SpatialReferenceType { using value_type=SpatialReferenceValue; static constexpr FieldId id{1,26}; };
struct BeamWidth { using value_type=BeamWidthCode; static constexpr FieldId id{1,25}; };
struct Range { using value_type=MetresQ6; static constexpr FieldId id{1,24}; };
struct EbNoBer { using value_type=EbNoBerValue; static constexpr FieldId id{1,20}; };
struct Threshold { using value_type=ThresholdValue; static constexpr FieldId id{1,19}; };
struct CompressionPoint { using value_type=DecibelsQ7; static constexpr FieldId id{1,18}; };
struct InterceptPoints { using value_type=InterceptPointsValue; static constexpr FieldId id{1,17}; };
struct SnrNoiseFigure { using value_type=SnrNoiseFigureValue; static constexpr FieldId id{1,16}; };
struct AuxiliaryFrequency { using value_type=Hertz; static constexpr FieldId id{1,15}; };
struct AuxiliaryGain { using value_type=GainStages; static constexpr FieldId id{1,14}; };
struct AuxiliaryBandwidth { using value_type=Hertz; static constexpr FieldId id{1,13}; };
struct DiscreteIO32 { using value_type=std::uint32_t; static constexpr FieldId id{1,6}; };
struct DiscreteIO64 { using value_type=Unsigned64Bits; static constexpr FieldId id{1,5}; };
struct HealthStatus { using value_type=std::uint32_t; static constexpr FieldId id{1,4}; };
struct V49SpecCompliance { using value_type=std::uint32_t; static constexpr FieldId id{1,3}; };
struct VersionBuild { using value_type=VersionBuildValue; static constexpr FieldId id{1,2}; };
struct BufferSize { using value_type=BufferSizeValue; static constexpr FieldId id{1,1}; };
struct PointingVectorStructure {using value_type=PointingVectorInput;using view_type=PointingVectorView;static constexpr FieldId id{1,28};};
struct IndexList {using value_type=IndexListInput;using view_type=IndexListView;static constexpr FieldId id{1,7};};
struct Spectrum {using value_type=SpectrumValue;using view_type=SpectrumValue;static constexpr FieldId id{1,10};};
struct SectorStepScan {using value_type=SectorStepScanInput;using view_type=SectorStepScanView;static constexpr FieldId id{1,9};};
struct Bind { using value_type=std::uint32_t; static constexpr FieldId id{2,31}; };
struct CitedSID { using value_type=std::uint32_t; static constexpr FieldId id{2,30}; };
struct SiblingSID { using value_type=std::uint32_t; static constexpr FieldId id{2,29}; };
struct ParentSID { using value_type=std::uint32_t; static constexpr FieldId id{2,28}; };
struct ChildSID { using value_type=std::uint32_t; static constexpr FieldId id{2,27}; };
struct CitedMessageId { using value_type=std::uint32_t; static constexpr FieldId id{2,26}; };
struct ControlleeId { using value_type=std::uint32_t; static constexpr FieldId id{2,25}; };
struct ControlleeUUID { using value_type=UuidValue; using view_type=UuidValue; static constexpr FieldId id{2,24}; };
struct ControllerId { using value_type=std::uint32_t; static constexpr FieldId id{2,23}; };
struct ControllerUUID { using value_type=UuidValue; using view_type=UuidValue; static constexpr FieldId id{2,22}; };
struct InformationSource { using value_type=std::uint32_t; static constexpr FieldId id{2,21}; };
struct TrackId { using value_type=std::uint32_t; static constexpr FieldId id{2,20}; };
struct CountryCode { using value_type=CountryCodeValue; static constexpr FieldId id{2,19}; };
struct OperatorId { using value_type=std::uint32_t; static constexpr FieldId id{2,18}; };
struct PlatformClass { using value_type=std::uint32_t; static constexpr FieldId id{2,17}; };
struct PlatformInstance { using value_type=std::uint32_t; static constexpr FieldId id{2,16}; };
struct PlatformDisplay { using value_type=std::uint32_t; static constexpr FieldId id{2,15}; };
struct EmsDeviceClass { using value_type=EmsDeviceClassValue; static constexpr FieldId id{2,14}; };
struct EmsDeviceType { using value_type=std::uint32_t; static constexpr FieldId id{2,13}; };
struct EmsDeviceInstance { using value_type=std::uint32_t; static constexpr FieldId id{2,12}; };
struct ModulationClass { using value_type=std::uint32_t; static constexpr FieldId id{2,11}; };
struct ModulationType { using value_type=std::uint32_t; static constexpr FieldId id{2,10}; };
struct FunctionId { using value_type=std::uint32_t; static constexpr FieldId id{2,9}; };
struct ModeId { using value_type=std::uint32_t; static constexpr FieldId id{2,8}; };
struct EventId { using value_type=std::uint32_t; static constexpr FieldId id{2,7}; };
struct FunctionPriority { using value_type=std::uint32_t; static constexpr FieldId id{2,6}; };
struct CommunicationPriority { using value_type=std::uint32_t; static constexpr FieldId id{2,5}; };
struct RFFootprint { using value_type=std::uint32_t; static constexpr FieldId id{2,4}; };
struct RFFootprintRange { using value_type=std::uint32_t; static constexpr FieldId id{2,3}; };
struct TimestampDetails { using value_type=TimestampDetailsValue; static constexpr FieldId id{3,31}; };
struct TimestampSkew { using value_type=Femtoseconds; static constexpr FieldId id{3,30}; };
struct RiseTime { using value_type=Femtoseconds; static constexpr FieldId id{3,27}; };
struct FallTime { using value_type=Femtoseconds; static constexpr FieldId id{3,26}; };
struct OffsetTime { using value_type=Femtoseconds; static constexpr FieldId id{3,25}; };
struct PulseWidth { using value_type=Femtoseconds; static constexpr FieldId id{3,24}; };
struct Period { using value_type=Femtoseconds; static constexpr FieldId id{3,23}; };
struct Duration { using value_type=Femtoseconds; static constexpr FieldId id{3,22}; };
struct Dwell { using value_type=Femtoseconds; static constexpr FieldId id{3,21}; };
struct Jitter { using value_type=Femtoseconds; static constexpr FieldId id{3,20}; };
struct Age { using value_type=StateDurationValue; using view_type=StateDurationValue; static constexpr FieldId id{3,17}; };
struct ShelfLife { using value_type=StateDurationValue; using view_type=StateDurationValue; static constexpr FieldId id{3,16}; };
struct AirTemperature { using value_type=CelsiusQ6; static constexpr FieldId id{3,7}; };
struct SeaGroundTemperature { using value_type=CelsiusQ6; static constexpr FieldId id{3,6}; };
struct Humidity { using value_type=HumidityCode; static constexpr FieldId id{3,5}; };
struct BarometricPressure { using value_type=BarometricPressureCode; static constexpr FieldId id{3,4}; };
struct SeaSwellState { using value_type=SeaSwellValue; static constexpr FieldId id{3,3}; };
struct TroposphericState { using value_type=std::uint32_t; static constexpr FieldId id{3,2}; };
struct NetworkId { using value_type=std::uint32_t; static constexpr FieldId id{3,1}; };
constexpr bool temporal_duration_field(FieldId id) noexcept { return id.cif==3 && (id.bit==17||id.bit==16); }
constexpr bool cif2_generic16_field(FieldId id) noexcept {
    return id.cif==2 && (id.bit==18 || (id.bit>=6 && id.bit<=11));
}
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
struct ProbabilityCode { std::uint8_t value=0,function=0; friend constexpr bool operator==(ProbabilityCode,ProbabilityCode)=default; };
struct BeliefCode { std::uint8_t value=0; friend constexpr bool operator==(BeliefCode,BeliefCode)=default; };
using SemanticValue = std::variant<std::uint32_t, Hertz, PayloadFormat, DecibelsQ7,
                                   GainStages, Femtoseconds, CelsiusQ6, DeviceIdentifierValue, NativeSlice,
                                   RadiansQ7, PolarizationAngles, PointingAngles, SpatialReferenceValue,
                                   BeamWidthCode, MetresQ6, EbNoBerValue, ThresholdValue,
                                   InterceptPointsValue, SnrNoiseFigureValue, Unsigned64Bits,
                                   VersionBuildValue, BufferSizeValue, CountryCodeValue, EmsDeviceClassValue, HumidityCode, BarometricPressureCode, SeaSwellValue, TimestampDetailsValue, ProbabilityCode, BeliefCode>;
static_assert(sizeof(SemanticValue) == 16, "Scalar registry must preserve the reference semantic-value budget");
inline constexpr std::uint32_t all_attributes=0xfff80000u;
constexpr bool base_attribute(Attribute a) noexcept { return static_cast<unsigned>(a)<11; }
struct FieldDescriptor { FieldId id; std::size_t words; std::size_t variant_index; std::uint32_t attributes; };
inline constexpr std::array baseline_descriptors{
    FieldDescriptor{ReferencePoint::id,1,0,all_attributes},
    FieldDescriptor{SampleRate::id,2,1,all_attributes},
    FieldDescriptor{StateEvent::id,1,0,all_attributes},
    FieldDescriptor{DataPayloadFormat::id,2,2,all_attributes} };
inline constexpr std::array scalar_descriptors{
    FieldDescriptor{Bandwidth::id,2,1,all_attributes},
    FieldDescriptor{IFReferenceFrequency::id,2,1,all_attributes},
    FieldDescriptor{RFReferenceFrequency::id,2,1,all_attributes},
    FieldDescriptor{RFReferenceFrequencyOffset::id,2,1,all_attributes},
    FieldDescriptor{IFBandOffset::id,2,1,all_attributes},
    FieldDescriptor{ReferenceLevel::id,1,3,all_attributes},
    FieldDescriptor{Gain::id,1,4,all_attributes},
    FieldDescriptor{OverRangeCount::id,1,0,all_attributes},
    FieldDescriptor{TimestampAdjustment::id,2,5,all_attributes},
    FieldDescriptor{TimestampCalibrationTime::id,1,0,all_attributes},
    FieldDescriptor{Temperature::id,1,6,all_attributes},
    FieldDescriptor{DeviceIdentifier::id,2,7,all_attributes},
    FieldDescriptor{EphemerisReferenceId::id,1,0,all_attributes}};
inline constexpr std::array structured_descriptors{
    FieldDescriptor{FormattedGPS::id,11,8,all_attributes},
    FieldDescriptor{FormattedINS::id,11,8,all_attributes},
    FieldDescriptor{ECEFEphemeris::id,13,8,all_attributes},
    FieldDescriptor{RelativeEphemeris::id,13,8,all_attributes},
    FieldDescriptor{GPSASCII::id,0,8,all_attributes},
    FieldDescriptor{ContextAssociationLists::id,0,8,all_attributes}};
inline constexpr std::array cif1_fixed_descriptors{
    FieldDescriptor{PhaseOffset::id,1,9,all_attributes},
    FieldDescriptor{Polarization::id,1,10,all_attributes},
    FieldDescriptor{PointingVector3D::id,1,11,all_attributes},
    FieldDescriptor{SpatialScanType::id,1,0,all_attributes},
    FieldDescriptor{SpatialReferenceType::id,1,12,all_attributes},
    FieldDescriptor{BeamWidth::id,1,13,all_attributes},
    FieldDescriptor{Range::id,1,14,all_attributes},
    FieldDescriptor{EbNoBer::id,1,15,all_attributes},
    FieldDescriptor{Threshold::id,1,16,all_attributes},
    FieldDescriptor{CompressionPoint::id,1,3,all_attributes},
    FieldDescriptor{InterceptPoints::id,1,17,all_attributes},
    FieldDescriptor{SnrNoiseFigure::id,1,18,all_attributes},
    FieldDescriptor{AuxiliaryFrequency::id,2,1,all_attributes},
    FieldDescriptor{AuxiliaryGain::id,1,4,all_attributes},
    FieldDescriptor{AuxiliaryBandwidth::id,2,1,all_attributes},
    FieldDescriptor{DiscreteIO32::id,1,0,all_attributes},
    FieldDescriptor{DiscreteIO64::id,2,19,all_attributes},
    FieldDescriptor{HealthStatus::id,1,0,all_attributes},
    FieldDescriptor{V49SpecCompliance::id,1,0,all_attributes},
    FieldDescriptor{VersionBuild::id,1,20,all_attributes},
    FieldDescriptor{BufferSize::id,2,21,all_attributes}};
inline constexpr std::array cif1_structure_descriptors{
    FieldDescriptor{PointingVectorStructure::id,0,8,all_attributes},FieldDescriptor{IndexList::id,0,8,all_attributes},
    FieldDescriptor{Spectrum::id,13,8,all_attributes},FieldDescriptor{SectorStepScan::id,0,8,all_attributes}};
inline constexpr std::array cif2_descriptors{
    FieldDescriptor{Bind::id,1,0,all_attributes},
    FieldDescriptor{CitedSID::id,1,0,all_attributes},
    FieldDescriptor{SiblingSID::id,1,0,all_attributes},
    FieldDescriptor{ParentSID::id,1,0,all_attributes},
    FieldDescriptor{ChildSID::id,1,0,all_attributes},
    FieldDescriptor{CitedMessageId::id,1,0,all_attributes},
    FieldDescriptor{ControlleeId::id,1,0,all_attributes},
    FieldDescriptor{ControlleeUUID::id,4,8,all_attributes},
    FieldDescriptor{ControllerId::id,1,0,all_attributes},
    FieldDescriptor{ControllerUUID::id,4,8,all_attributes},
    FieldDescriptor{InformationSource::id,1,0,all_attributes},
    FieldDescriptor{TrackId::id,1,0,all_attributes},
    FieldDescriptor{CountryCode::id,1,22,all_attributes},
    FieldDescriptor{OperatorId::id,1,0,all_attributes},
    FieldDescriptor{PlatformClass::id,1,0,all_attributes},
    FieldDescriptor{PlatformInstance::id,1,0,all_attributes},
    FieldDescriptor{PlatformDisplay::id,1,0,all_attributes},
    FieldDescriptor{EmsDeviceClass::id,1,23,all_attributes},
    FieldDescriptor{EmsDeviceType::id,1,0,all_attributes},
    FieldDescriptor{EmsDeviceInstance::id,1,0,all_attributes},
    FieldDescriptor{ModulationClass::id,1,0,all_attributes},
    FieldDescriptor{ModulationType::id,1,0,all_attributes},
    FieldDescriptor{FunctionId::id,1,0,all_attributes},
    FieldDescriptor{ModeId::id,1,0,all_attributes},
    FieldDescriptor{EventId::id,1,0,all_attributes},
    FieldDescriptor{FunctionPriority::id,1,0,all_attributes},
    FieldDescriptor{CommunicationPriority::id,1,0,all_attributes},
    FieldDescriptor{RFFootprint::id,1,0,all_attributes},
    FieldDescriptor{RFFootprintRange::id,1,0,all_attributes}};
inline constexpr std::array cif3_descriptors{
    FieldDescriptor{TimestampDetails::id,2,27,all_attributes},
    FieldDescriptor{TimestampSkew::id,2,5,all_attributes},
    FieldDescriptor{RiseTime::id,2,5,all_attributes},
    FieldDescriptor{FallTime::id,2,5,all_attributes},
    FieldDescriptor{OffsetTime::id,2,5,all_attributes},
    FieldDescriptor{PulseWidth::id,2,5,all_attributes},
    FieldDescriptor{Period::id,2,5,all_attributes},
    FieldDescriptor{Duration::id,2,5,all_attributes},
    FieldDescriptor{Dwell::id,2,5,all_attributes},
    FieldDescriptor{Jitter::id,2,5,all_attributes},
    FieldDescriptor{Age::id,0,8,all_attributes},
    FieldDescriptor{ShelfLife::id,0,8,all_attributes},
    FieldDescriptor{AirTemperature::id,1,6,all_attributes},
    FieldDescriptor{SeaGroundTemperature::id,1,6,all_attributes},
    FieldDescriptor{Humidity::id,1,24,all_attributes},
    FieldDescriptor{BarometricPressure::id,1,25,all_attributes},
    FieldDescriptor{SeaSwellState::id,1,26,all_attributes},
    FieldDescriptor{TroposphericState::id,1,0,all_attributes},
    FieldDescriptor{NetworkId::id,1,0,all_attributes}};
constexpr const FieldDescriptor* descriptor(FieldId id) noexcept {
    for (const auto& d : baseline_descriptors) if (d.id == id) return &d;
    for (const auto& d : scalar_descriptors) if (d.id == id) return &d;
    for (const auto& d : structured_descriptors) if (d.id == id) return &d;
    for (const auto& d : cif1_fixed_descriptors) if (d.id == id) return &d;
    for (const auto& d : cif1_structure_descriptors) if (d.id == id) return &d;
    for (const auto& d : cif2_descriptors) if (d.id == id) return &d;
    for (const auto& d : cif3_descriptors) if (d.id == id) return &d;
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
constexpr Result<void> validate_value(FieldId id, const SemanticValue& value, bool current=true) noexcept {
    if (std::holds_alternative<NativeSlice>(value)) return std::unexpected(Error{ErrorCode::invalid_argument});
    const auto* d = descriptor(id);
    if (!d) return std::unexpected(Error{ErrorCode::unsupported_layout});
    if (value.index() != d->variant_index) return std::unexpected(Error{ErrorCode::invalid_argument});
    if (current && (id == SampleRate::id || id == Bandwidth::id || id == AuxiliaryBandwidth::id) && std::get<Hertz>(value).q20 < 0) return std::unexpected(Error{ErrorCode::invalid_argument});
    if (current && (id == Temperature::id || id==AirTemperature::id || id==SeaGroundTemperature::id) && std::get<CelsiusQ6>(value).q6 < -17481)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if (id == DeviceIdentifier::id && std::get<DeviceIdentifierValue>(value).oui > 0xffffffu)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if (id == SpatialScanType::id || id == HealthStatus::id || id==TroposphericState::id || cif2_generic16_field(id)) {
        if (std::get<std::uint32_t>(value)>0xffffu)return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    if (current && id == V49SpecCompliance::id) {
        const auto code=std::get<std::uint32_t>(value);
        if(code<1 || code>4)return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    if (current && id == PointingVector3D::id) {
        const auto elevation=std::get<PointingAngles>(value).elevation_q7;
        if(elevation < -90*128 || elevation > 90*128)return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    if (id == SpatialReferenceType::id) {
        const auto ref=std::get<SpatialReferenceValue>(value);
        if(ref.reference>3 || ref.beam>2)return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    const auto nonsentinel=[](const OptionalQ7& v) constexpr noexcept { return !v || *v!=INT16_MAX; };
    if (id == EbNoBer::id) {
        const auto& v=std::get<EbNoBerValue>(value);
        if(!nonsentinel(v.ebno_q7) || !nonsentinel(v.ber_q7) || (current && v.ber_q7 && *v.ber_q7>0))
            return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    if (id == InterceptPoints::id) {
        const auto& v=std::get<InterceptPointsValue>(value);
        if(!nonsentinel(v.second_q7) || !nonsentinel(v.third_q7))return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    if (id == SnrNoiseFigure::id) {
        const auto& v=std::get<SnrNoiseFigureValue>(value);
        if(!nonsentinel(v.snr_q7) || (current && v.noise_figure_q7 && *v.noise_figure_q7<=0))
            return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    if (id == VersionBuild::id) {
        const auto v=std::get<VersionBuildValue>(value);
        if(v.year_since2000>127 || (current ? (v.day<1 || v.day>366) : v.day>511) || v.revision>63 || v.user>1023)
            return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    if(current && id==Bind::id && std::get<std::uint32_t>(value)>1)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(id==CountryCode::id && std::get<CountryCodeValue>(value).code>0x7ff)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if(id==EmsDeviceClass::id) {
        const auto v=std::get<EmsDeviceClassValue>(value);
        if(v.class_code>0xfff || v.organization>2)return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    if(current && id.cif==3 && (id.bit==27||id.bit==26||(id.bit>=20&&id.bit<=24)) && std::get<Femtoseconds>(value).count<0)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if(id==BarometricPressure::id && std::get<BarometricPressureCode>(value).raw17>0x1ffff)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if(id==SeaSwellState::id) {
        const auto v=std::get<SeaSwellValue>(value);
        if(v.sea>(current?9:31)||v.swell>(current?9:31)||v.user>63)return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    if(current && id==TimestampDetails::id)return validate_timestamp_details_intrinsic(std::get<TimestampDetailsValue>(value));
    return {};
}
constexpr Result<void> validate_attribute_value(FieldId id,Attribute attribute,const SemanticValue& value) noexcept {
    if(!descriptor(id))return std::unexpected(Error{ErrorCode::unsupported_layout});
    if(static_cast<unsigned>(attribute)>=13 || std::holds_alternative<NativeSlice>(value))return std::unexpected(Error{ErrorCode::invalid_argument});
    if(attribute==Attribute::probability)return std::holds_alternative<ProbabilityCode>(value)?Result<void>{}:std::unexpected(Error{ErrorCode::invalid_argument});
    if(attribute==Attribute::belief)return std::holds_alternative<BeliefCode>(value)?Result<void>{}:std::unexpected(Error{ErrorCode::invalid_argument});
    return validate_value(id,value,attribute==Attribute::current || (id==SampleRate::id&&(attribute==Attribute::minimum||attribute==Attribute::maximum)));
}
inline Result<std::optional<TimestampFormatBinding>> native_timestamp_binding(FieldId id,Bytes bytes) noexcept {
    if(temporal_duration_field(id)) {
        if(bytes.size()!=sizeof(StateDurationValue))return std::unexpected(Error{ErrorCode::invalid_argument});
        auto v=detail::native_object<StateDurationValue>(bytes);if(!v)return std::unexpected(v.error());
        return TimestampFormatBinding{v->tsi,v->tsf,true};
    }
    if(id==SectorStepScan::id) {
        auto v=detail::native_sectors(bytes);if(!v)return std::unexpected(v.error());
        if(v->selectors&(1u<<22))return v->start_format;
    }
    return std::optional<TimestampFormatBinding>{};
}
} // namespace vita
