#include <vita/codec/packet.hpp>
#include <algorithm>
#include <array>
#include <cassert>
using namespace vita;
using namespace vita::codec;
struct Vector { FieldId id; SemanticValue value; std::uint64_t bits; };
const std::array vectors{
    Vector{PhaseOffset::id,RadiansQ7{-128},0x0000ff80},
    Vector{Polarization::id,PolarizationAngles{128,-64},0x0080ffc0},
    Vector{PointingVector3D::id,PointingAngles{65535,-11520},0xd300ffff},
    Vector{SpatialScanType::id,std::uint32_t{0xffff},0x0000ffff},
    Vector{SpatialReferenceType::id,SpatialReferenceValue{0x1234,3,2},0x1234000e},
    Vector{BeamWidth::id,BeamWidthCode{0xb400,0xffff},0xb400ffff},
    Vector{Range::id,MetresQ6{UINT32_MAX},0xffffffff},
    Vector{EbNoBer::id,EbNoBerValue{128,-128},0x0080ff80},
    Vector{Threshold::id,ThresholdValue{-128,256},0x0100ff80},
    Vector{CompressionPoint::id,DecibelsQ7{-128},0x0000ff80},
    Vector{InterceptPoints::id,InterceptPointsValue{std::nullopt,-128},0x7fffff80},
    Vector{SnrNoiseFigure::id,SnrNoiseFigureValue{std::nullopt,128},0x7fff0080},
    Vector{AuxiliaryFrequency::id,Hertz{-1048576},0xfffffffffff00000ull},
    Vector{AuxiliaryGain::id,GainStages{-128,128},0x0080ff80},
    Vector{AuxiliaryBandwidth::id,Hertz{1},1},
    Vector{DiscreteIO32::id,std::uint32_t{0xdeadbeef},0xdeadbeef},
    Vector{DiscreteIO64::id,Unsigned64Bits{0x0123456789abcdefull},0x0123456789abcdefull},
    Vector{HealthStatus::id,std::uint32_t{0x1234},0x00001234},
    Vector{V49SpecCompliance::id,std::uint32_t{4},4},
    Vector{VersionBuild::id,VersionBuildValue{127,366,63,1023},0xff6effff},
    Vector{BufferSize::id,BufferSizeValue{0x12345678,0x80,0xab},0x12345678000080abull}};

int main() {
    static_assert(sizeof(SemanticValue)==16);
    static_assert(std::is_same_v<std::variant_alternative_t<8,SemanticValue>,NativeSlice>);
    static_assert(sizeof(PacketSnapshot<BodyKind::values>)==5312);
    static_assert(cif1_fixed_descriptors.size()==21);
    std::array<std::byte,512> storage{};Envelope envelope;envelope.type=PacketType::context;envelope.stream_id=1;
    for(const auto& vector:vectors) {
        ContextPacket packet;assert(packet.set_value(vector.id,vector.value));
        auto encoded=encode_packet(envelope,packet.freeze(),storage);assert(encoded);
        const auto bytes=descriptor(vector.id)->words*4;
        assert(*encoded==16+bytes); // Header/SID + CIF0/CIF1
        assert(storage[11]==std::byte{2});
        for(std::size_t i=0;i<bytes;++i)assert(storage[16+i]==std::byte((vector.bits>>(8*(bytes-1-i)))&255));
        auto decoded=decode_packet(Bytes{storage}.first(*encoded));assert(decoded&&decoded->fields.size()==1);
        assert(*decoded->fields[0].value()==vector.value);
        ContextPacket materialized;assert(decoded->fields[0].materialize_into(materialized));
        for(std::size_t size=0;size<*encoded;++size) {
            assert(!decode_packet(Bytes{storage}.first(size)));
            std::array<std::byte,512> output;output.fill(std::byte{0x5a});
            assert(!encode_packet(envelope,packet.freeze(),MutableBytes{output}.first(size)));
            assert(std::all_of(output.begin(),output.end(),[](auto v){return v==std::byte{0x5a};}));
        }
        assert(packet.with_attributes(attribute_bit(Attribute::current)));
        assert(!packet.with_attributes(attribute_bit(Attribute::maximum)));
    }
    // Both sides of each signed/code boundary; sentinel meaning remains explicit.
    const std::array boundaries{
        Vector{BeamWidth::id,BeamWidthCode{0x8000,0xb400},0x8000b400},
        Vector{PhaseOffset::id,RadiansQ7{INT16_MIN},0x00008000},
        Vector{Polarization::id,PolarizationAngles{INT16_MAX,INT16_MIN},0x7fff8000},
        Vector{EbNoBer::id,EbNoBerValue{std::nullopt,std::nullopt},0x7fff7fff},
        Vector{SnrNoiseFigure::id,SnrNoiseFigureValue{0,std::nullopt},0},
        Vector{SnrNoiseFigure::id,SnrNoiseFigureValue{-128,INT16_MAX},0xff807fff},
        Vector{Threshold::id,ThresholdValue{INT16_MAX,INT16_MIN},0x80007fff},
        Vector{InterceptPoints::id,InterceptPointsValue{INT16_MIN,32766},0x80007ffe},
        Vector{AuxiliaryBandwidth::id,Hertz{INT64_MAX},0x7fffffffffffffffull}};
    for(auto v:boundaries){ContextPacket p;assert(p.set_value(v.id,v.value));auto n=encode_packet(envelope,p.freeze(),storage);assert(n);auto d=decode_packet(Bytes{storage}.first(*n));assert(d&&*d->fields[0].value()==v.value);}
    ContextPacket invalid;
    assert(!invalid.set<PointingVector3D>(PointingAngles{0,11521}));
    assert(!invalid.set<SpatialReferenceType>(SpatialReferenceValue{0,0,3}));
    assert(!invalid.set<SpatialScanType>(65536));assert(!invalid.set<HealthStatus>(65536));
    assert(!invalid.set<EbNoBer>(EbNoBerValue{0,1}));assert(!invalid.set<EbNoBer>(EbNoBerValue{INT16_MAX,0}));
    assert(!invalid.set<InterceptPoints>(InterceptPointsValue{INT16_MAX,0}));
    assert(!invalid.set<SnrNoiseFigure>(SnrNoiseFigureValue{0,0}));
    assert(!invalid.set<SnrNoiseFigure>(SnrNoiseFigureValue{0,-1}));
    assert(!invalid.set<AuxiliaryBandwidth>(Hertz{-1}));
    assert(!invalid.set<V49SpecCompliance>(0));assert(!invalid.set<V49SpecCompliance>(5));
    assert(!invalid.set<VersionBuild>(VersionBuildValue{0,0,0,0}));
    assert(!invalid.set<VersionBuild>(VersionBuildValue{0,367,0,0}));
    assert(!invalid.set<VersionBuild>(VersionBuildValue{128,1,0,0}));
    assert(!invalid.set<VersionBuild>(VersionBuildValue{0,1,64,0}));
    assert(!invalid.set<VersionBuild>(VersionBuildValue{0,1,0,1024}));
    assert(validate_threshold({1,0},ThresholdMode::single_db));
    assert(validate_threshold({1,INT16_MIN},ThresholdMode::single_dbm));
    assert(validate_threshold({-1,0},ThresholdMode::window_db));
    assert(!validate_threshold({1,1},ThresholdMode::window_dbm));
    assert(!validate_threshold({1,0},ThresholdMode::single_dbm));
    // Decoder preserves readable semantic-invalid values; reserved bits do not pass.
    for(const auto id:{PhaseOffset::id,CompressionPoint::id,SpatialScanType::id,HealthStatus::id,SpatialReferenceType::id,BufferSize::id}) {
        ContextPacket p;assert(p.set_value(id,placeholder(id)));auto n=encode_packet(envelope,p.freeze(),storage);assert(n);
        auto offset=id==BufferSize::id?20u:id==SpatialReferenceType::id?18u:16u;
        storage[offset]=std::byte{0x80};assert(!decode_packet(Bytes{storage}.first(*n)));
    }
    const std::array semantic_invalid{
        Vector{EbNoBer::id,EbNoBerValue{},1},Vector{SnrNoiseFigure::id,SnrNoiseFigureValue{},0x0000ffff},
        Vector{PointingVector3D::id,PointingAngles{},0x2d010000},
        Vector{AuxiliaryBandwidth::id,Hertz{},UINT64_MAX},
        Vector{V49SpecCompliance::id,std::uint32_t{1},5},
        Vector{VersionBuild::id,VersionBuildValue{},0}};
    for(const auto& v:semantic_invalid) {
        ContextPacket p;assert(p.set_value(v.id,v.value));auto n=encode_packet(envelope,p.freeze(),storage);assert(n);
        auto bytes=descriptor(v.id)->words*4;
        for(std::size_t i=0;i<bytes;++i)storage[16+i]=std::byte((v.bits>>(8*(bytes-1-i)))&255);
        auto d=decode_packet(Bytes{storage}.first(*n));assert(d);auto value=d->fields[0].value();assert(value&&!validate_value(v.id,*value));
    }
    // Preserve paired field order across CIF0/CIF1 and two-word status extents.
    ContextPacket mixed;assert(mixed.set<BufferSize>(BufferSizeValue{1024,255,1}));assert(mixed.set<ReferencePoint>(9));
    assert(mixed.set<PhaseOffset>(RadiansQ7{-1}));auto n=encode_packet(envelope,mixed.freeze(),storage);assert(n);
    auto parsed=decode_packet(Bytes{storage}.first(*n));assert(parsed&&parsed->fields.size()==3);
    assert(parsed->fields[0].id==ReferencePoint::id&&parsed->fields[1].id==PhaseOffset::id&&parsed->fields[2].bytes.size()==8);
    QueryPacket query;assert(query.select<DiscreteIO64>());assert(query.select<BufferSize>());
    Envelope command;command.type=PacketType::command;command.stream_id=1;
    command.command=Command{0xa0040000,1,Identifier::short_id(2),Identifier::short_id(3)};
    n=encode_packet(command,query.freeze(),storage);assert(n&&*n==32);
    parsed=decode_packet(Bytes{storage}.first(*n));assert(parsed&&parsed->fields.size()==2&&parsed->body_kind==BodyKind::selectors);
    DiagnosticAck warnings,errors;assert(warnings.diagnostic(BeamWidth::id,1u<<27));assert(errors.diagnostic(BufferSize::id,1u<<29));
    command.ack=true;command.command->cam=0xa90b0c00;
    n=encode_diagnostic(command,warnings.freeze(),errors.freeze(),RequestContext{0xa90b0000},storage);assert(n);
    parsed=decode_packet(Bytes{storage}.first(*n),DecodeOptions{RequestContext{0xa90b0000}});assert(parsed&&parsed->fields.size()==2);
    assert(parsed->fields[1].bytes.size()==4);
}
