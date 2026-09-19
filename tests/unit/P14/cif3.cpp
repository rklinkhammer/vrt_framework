#include <vita/codec/packet.hpp>
#include <array>
#include <cassert>
#include <type_traits>
using namespace vita;
using namespace vita::codec;
static_assert(sizeof(SemanticValue)==16);
static_assert(sizeof(LayoutContext)==48);
static_assert(sizeof(FieldView)==32 && std::is_standard_layout_v<FieldView>);
static_assert(sizeof(PacketSnapshot<BodyKind::values>)==5312);
int main() {
    std::array<std::byte,256> wire{};Envelope envelope;envelope.type=PacketType::context;envelope.stream_id=1;
    for(unsigned tsi=0;tsi<4;++tsi)for(unsigned tsf=0;tsf<4;++tsf) {
        NativeContextPacket<32> builder;
        StateDurationValue value{tsf?UINT64_MAX:0,tsi?0x12345678u:0,static_cast<std::uint8_t>(tsi),static_cast<std::uint8_t>(tsf)};
        if(!tsi&&!tsf){assert(!builder.set<Age>(value));continue;}
        assert(builder.set<Age>(value));assert(builder.set<ShelfLife>(value));
        assert(!measure(builder.freeze()));
        assert(builder.bind_timestamp_format(tsi,tsf));auto snapshot=builder.freeze();
        const auto body=measure(snapshot);assert(body&&body->bytes==8+2*((tsi?4:0)+(tsf?8:0)));
        envelope.timestamp={static_cast<Tsi>(tsi),static_cast<Tsf>(tsf),1,UINT64_MAX};
        auto n=encode_packet(envelope,snapshot,wire);assert(n);
        auto decoded=decode_packet(Bytes{wire}.first(*n));assert(decoded&&decoded->fields.size()==2);
        for(unsigned i=0;i<2;++i)assert(*decoded->fields[i].duration()==value);
        NativeContextPacket<32> copy;assert(decoded->fields[0].materialize_into(copy));assert(!measure(copy.freeze()));
        assert(copy.bind_timestamp_format(tsi,tsf));assert(measure(copy.freeze()));
        const auto generation=builder.generation();assert(!builder.bind_timestamp_format((tsi+1)%4,tsf));assert(builder.generation()==generation);
        std::array<std::byte,256> untouched{};untouched.fill(std::byte{0xa5});auto before=untouched;
        envelope.timestamp.tsi=static_cast<Tsi>((tsi+1)%4);assert(!encode_packet(envelope,snapshot,untouched));assert(untouched==before);
        envelope.timestamp.tsi=static_cast<Tsi>(tsi);assert(!encode_packet(envelope,snapshot,MutableBytes{untouched}.first(*n-1)));assert(untouched==before);
        for(std::size_t size=0;size<*n;++size)assert(!decode_packet(Bytes{wire}.first(size)));
        auto bad=decoded->fields[0];bad.timestamp_format={0,0,true};assert(!bad.duration());bad=decoded->fields[0];bad.bytes=bad.bytes.first(bad.bytes.size()-1);assert(!bad.duration());
    }
    ContextPacket scalars;
    assert(scalars.set<TimestampDetails>({0x8100c0ff,0xfedcba98}));
    assert(scalars.set<TimestampSkew>({-42}));assert(scalars.set<OffsetTime>({-1}));
    assert(scalars.set<AirTemperature>({-17481}));assert(scalars.set<SeaGroundTemperature>({64}));
    assert(scalars.set<Humidity>({65535}));assert(scalars.set<BarometricPressure>({0x1ffff}));
    assert(scalars.set<SeaSwellState>({9,9,63}));assert(scalars.set<TroposphericState>(65535));assert(scalars.set<NetworkId>(0xffffffff));
    envelope.timestamp={};auto n=encode_packet(envelope,scalars.freeze(),wire);assert(n);
    auto decoded=decode_packet(Bytes{wire}.first(*n));assert(decoded&&decoded->fields.size()==10);
    assert(std::get<TimestampDetailsValue>(*decoded->fields[0].value()).epoch==0xfedcba98);
    // Literal single-field body and reserved-bit rejection before visitor delivery.
    ContextPacket pressure;assert(pressure.set<BarometricPressure>({0x12345}));
    auto pressure_size=encode_packet(envelope,pressure.freeze(),wire);assert(pressure_size&&*pressure_size==20);
    const std::array<unsigned char,12> pressure_literal{0,0,0,8,0,0,0,16,0,1,0x23,0x45};
    for(unsigned i=0;i<12;++i)assert(wire[8+i]==std::byte{pressure_literal[i]});
    wire[16]=std::byte{0x80};unsigned callbacks=0;
    assert(!decode_and_visit(Bytes{wire}.first(*pressure_size),{},[&](const FieldView&) noexcept -> Result<void>{++callbacks;return {};}));assert(callbacks==0);
    // A structurally valid but semantically negative interval remains readable.
    ContextPacket rise;assert(rise.set<RiseTime>({1}));auto rise_size=encode_packet(envelope,rise.freeze(),wire);assert(rise_size);
    for(unsigned i=16;i<24;++i)wire[i]=std::byte{0xff};auto negative=decode_packet(Bytes{wire}.first(*rise_size));assert(negative);
    assert(std::get<Femtoseconds>(*negative->fields[0].value()).count==-1);
    ContextPacket replacement;assert(!negative->fields[0].materialize_into(replacement));assert(replacement.freeze().fields().empty());
    assert(!scalars.set<BarometricPressure>({0x20000}));assert(!scalars.set<SeaSwellState>({10,0,0}));assert(!scalars.set<TroposphericState>(65536));
    assert(!scalars.set<AirTemperature>({-17482}));assert(!scalars.set<RiseTime>({-1}));assert(!scalars.set<TimestampDetails>({1u<<19,0}));
    assert(!make_timestamp_details(1u<<12,0));assert(make_timestamp_details(2u<<12,0));
    TimestampDetailsScope scope{2,true,255,true,0};
    assert(*validate_timestamp_details({1u<<16,0},scope)==TimestampScopeStatus::complete);
    assert(!validate_timestamp_details({2u<<16,0},scope));scope.observed_tsi_mask=4;
    assert(validate_timestamp_details({1u<<16,315964811},scope));assert(!validate_timestamp_details({1u<<16,315964800},scope));
    scope.observed_tsi_mask=6;assert(!validate_timestamp_details({3u<<16,0},scope));assert(validate_timestamp_details({0,UINT32_MAX},scope));
    scope={1,true,0,false,4};assert(*validate_timestamp_details({},scope)==TimestampScopeStatus::complete);
    scope.observed_tsf_mask=1;assert(*validate_timestamp_details({},scope)==TimestampScopeStatus::not_applicable);
    scope.scope_complete=false;assert(*validate_timestamp_details({},scope)==TimestampScopeStatus::incomplete);
    scope.scope_complete=true;assert(*validate_timestamp_details({6u<<9,0},scope)==TimestampScopeStatus::incomplete);
    QueryPacket query;assert(query.select<Age>());assert(query.select<ShelfLife>());assert(measure(query.freeze())->bytes==8);
    DiagnosticAck diag;assert(diag.diagnostic(Age::id,0));assert(diag.diagnostic(ShelfLife::id,0));assert(measure(diag.freeze())->bytes==16);
    NativeContextPacket<16> a,b;assert(a.bind_timestamp_format(1,1));assert(b.bind_timestamp_format(1,2));assert(a.set<Age>({1,2,1,1}));assert(b.set<Age>({1,2,1,2}));
    assert(measure(a.freeze())->bytes==measure(b.freeze())->bytes);assert(!validate_measure(b.freeze(),*measure(a.freeze())));
}
