#include <vita/codec/packet.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
using namespace vita;
using namespace vita::codec;
Result<void> sentence(void*,std::span<const char> text) noexcept {
    if(text.size()<3 || text.front()!='$' || text.back()!='\n')return std::unexpected(Error{ErrorCode::invalid_argument});
    return {};
}
int main() {
    static_assert(sizeof(SemanticValue)==16);
    static_assert(sizeof(PacketSnapshot<BodyKind::values>)==sizeof(PacketSnapshot<BodyKind::values,16,0>));
    GeolocationValue gps;gps.fix={2,2,20,123};gps.latitude_q22=1<<22;gps.longitude_q22=-(2<<22);
    gps.altitude_q5=32;gps.speed_q16=65536;gps.heading_q22=90<<22;
    EphemerisValue ecef;ecef.fix={3,1,99,UINT64_MAX};ecef.position_q5[0]=-32;ecef.velocity_q16[2]=-1;
    std::array<char,7> text{'$','F','I','X','\r','\n','\n'};
    std::array<std::uint32_t,2> source{1,2},vector{3,3},async{4,5},tags{40,50};
    NativeContextPacket<8192> p;
    assert(p.set<FormattedGPS>(gps));assert(p.set<FormattedINS>(gps));
    assert(p.set<ECEFEphemeris>(ecef));assert(p.set<RelativeEphemeris>(ecef));
    assert(p.set<GPSASCII>({0x123456,text,{nullptr,sentence}}));
    assert(p.set<ContextAssociationLists>({source,{},vector,async,tags,true}));
    auto frozen=p.freeze();auto duplicate=frozen;
    auto ascii=frozen.get<GPSASCII>();assert(ascii&&ascii->text.size()==7);
    text[1]='X';source[0]=777;gps.latitude_q22=0;
    assert(*frozen.get<FormattedGPS>()->latitude_q22==(1<<22));
    assert(*frozen.get<ContextAssociationLists>()->source.at(0)==1);
    assert(ascii->text[1]=='F');
    assert(p.replace<FormattedGPS>(gps));
    assert(*frozen.get<FormattedGPS>()->latitude_q22==(1<<22));
    for(const auto& entry:frozen.fields())for(const auto& value:entry.values)if(value && std::holds_alternative<NativeSlice>(*value)) {
        assert(!p.set_value(entry.id,*value));
        assert(!duplicate.native_value(*value));
        const AttributeValue transplant{entry.id,Attribute::current,*value};
        assert(!p.with_attributes(attribute_bit(Attribute::current),{&transplant,1}));
    }
    assert(p.with_attributes(attribute_bit(Attribute::current)));
    const auto generation=p.generation();assert(!p.with_attributes(attribute_bit(Attribute::maximum)));assert(p.generation()==generation);
    auto original_measure=measure(frozen);assert(original_measure);
    assert(validate_measure(duplicate,*original_measure));
    std::array<std::byte,512> output{};Envelope header;header.type=PacketType::context;header.stream_id=1;
    header.timestamp={Tsi::gps,Tsf::picoseconds,500,0};
    auto encoded=encode_packet(header,frozen,output);assert(encoded);
    auto decoded=decode_packet(Bytes{output}.first(*encoded));assert(decoded&&decoded->fields.size()==6);
    assert(decoded->fields[0].bytes.size()==44&&decoded->fields[2].bytes.size()==52&&decoded->fields[3].bytes.size()==52);
    assert(decoded->fields[0].geolocation()->fix.seconds==20);
    assert(decoded->fields[2].ephemeris()->fix.fractional==UINT64_MAX);
    assert(decoded->fields[4].gps_ascii()->text[1]=='F');
    assert(decoded->fields[5].associations()->tags_present);
    NativeContextPacket<> restored;
    for(std::size_t i=0;i<decoded->fields.size();++i)assert(decoded->fields[i].materialize_into(restored,{nullptr,sentence}));
    std::array<std::byte,512> encoded_again{};auto again=encode_packet(header,restored.freeze(),encoded_again);assert(again&&*again==*encoded);
    assert(std::equal(output.begin(),output.begin()+*encoded,encoded_again.begin()));
    for(std::size_t n=0;n<*encoded;++n) {
        unsigned callbacks=0;
        assert(!decode_and_visit(Bytes{output}.first(n),{},[&](const FieldView&) noexcept -> Result<void>{++callbacks;return {};}));assert(callbacks==0);
    }
    for(std::size_t n=0;n<*encoded;++n) {
        encoded_again.fill(std::byte{0x5a});
        assert(!encode_packet(header,frozen,MutableBytes{encoded_again}.first(n)));
        assert(std::all_of(encoded_again.begin(),encoded_again.end(),[](auto b){return b==std::byte{0x5a};}));
    }
    // Replacement compacts live native data; no repeated-edit arena leak.
    NativeContextPacket<sizeof(GeolocationValue)> small;assert(small.set<FormattedGPS>({}));
    for(int i=0;i<100;++i){GeolocationValue value;value.altitude_q5=i;assert(small.replace<FormattedGPS>(value));}
    const auto before=small.freeze();assert(!small.set<FormattedINS>({}));assert(small.generation()==before.generation());
    assert(small.remove<FormattedGPS>());assert(small.freeze().native_size()==0);assert(small.set<FormattedINS>({}));
    NativeContextPacket<8> impossible;
    unsigned validator_calls=0;
    auto count_validator=[](void* context,std::span<const char>) noexcept -> Result<void> {
        ++*static_cast<unsigned*>(context);return {};
    };
    assert(!impossible.set<GPSASCII>({0xffffff,text,{&validator_calls,count_validator}}));
    assert(validator_calls==0);
    NativeContextPacket<> empty;
    assert(!empty.set<GPSASCII>({0xffffff,std::span<const char>{text},{}}));
    GeolocationValue bad;bad.latitude_q22=91<<22;assert(!empty.set<FormattedGPS>(bad));
    bad={};bad.speed_q16=-1;assert(!empty.set<FormattedGPS>(bad));
    bad={};bad.fix.seconds=0;assert(!empty.set<FormattedGPS>(bad));
    assert(!empty.set<ContextAssociationLists>({{},{},{},async,{},true}));
    // Per-list boundaries distinguish equal-total layout signatures.
    NativeContextPacket<> left,right;
    assert(left.set<ContextAssociationLists>({source,{},{},{},{},false}));
    assert(right.set<ContextAssociationLists>({{},source,{},{},{},false}));
    assert(measure(left.freeze())->bytes==measure(right.freeze())->bytes);
    assert(layout_signature(left.freeze())!=layout_signature(right.freeze()));
    // Bounds count actual tagged entries and padded ASCII units.
    std::array<std::uint32_t,513> too_many{};
    assert(!empty.set<ContextAssociationLists>({{},{},{},too_many,too_many,true}));
    std::array<char,4094> long_text{};long_text.fill('x');long_text.front()='$';long_text.back()='\n';
    assert(empty.set<GPSASCII>({0xffffff,long_text,{nullptr,sentence}}));
    auto work=measure(empty.freeze());assert(!work&&work.error().code==ErrorCode::resource_limit);
}
