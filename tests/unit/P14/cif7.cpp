#include <vita/codec/packet.hpp>
#include <array>
#include <cassert>
using namespace vita;
using namespace vita::codec;
static Result<void> sentence(void*,std::span<const char> text) noexcept {
    return text.empty()?std::unexpected(Error{ErrorCode::invalid_argument}):Result<void>{};
}
int main() {
    static_assert(sizeof(SemanticValue)==16&&sizeof(FieldEntry)==328&&sizeof(LayoutContext)==48&&sizeof(FieldView)==32);
    static_assert(sizeof(PacketSnapshot<BodyKind::values>)==5312);
    std::array<AttributeValue,13> attrs{};
    for(unsigned a=0;a<13;++a)attrs[a]={SampleRate::id,static_cast<Attribute>(a),a==11?SemanticValue{ProbabilityCode{19,7}}:a==12?SemanticValue{BeliefCode{23}}:SemanticValue{Hertz{(a==0||a==4||a==5)?1:-std::int64_t(a)}}};
    ContextPacket packet;assert(packet.set<SampleRate>({1}));const auto old=packet.freeze();
    assert(packet.with_attributes(all_attributes,attrs));assert(packet.generation()==old.generation()+1);
    std::array<std::byte,4096> wire{};Envelope e;e.type=PacketType::context;e.stream_id=1;
    auto n=encode_packet(e,packet.freeze(),wire);assert(n&&*n==112);
    auto decoded=decode_packet(Bytes{wire}.first(*n));assert(decoded&&decoded->fields.size()==13);
    for(unsigned a=0;a<13;++a){assert(decoded->fields[a].attribute==static_cast<Attribute>(a));assert(*decoded->fields[a].value()==attrs[a].value);}
    assert(std::get<Hertz>(*old.fields()[0].values[0]).q20==1);
    assert(packet.set_attribute<SampleRate>(Attribute::accuracy,{-99}));
    assert(packet.set_probability(SampleRate::id,{42,255}));assert(packet.freeze().probability(SampleRate::id)->value==42);
    const auto before=packet.freeze();auto bad=attrs;bad[12]=bad[11];assert(!packet.with_attributes(all_attributes,bad));assert(packet.generation()==before.generation());
    assert(!packet.with_attributes(1,attrs));assert(!packet.set_attribute<Bandwidth>(Attribute::accuracy,{1}));
    // Probability-only fields need neither a base arena nor a timestamp binding.
    ContextPacket probabilities;assert(probabilities.with_attributes(attribute_bit(Attribute::probability)));
    const AttributeInput uuid{ControllerUUID::id,Attribute::probability,SemanticValue{ProbabilityCode{128,2}}};
    assert(probabilities.set_field_attributes<ControllerUUID>({&uuid,1}));
    const AttributeInput age{Age::id,Attribute::probability,SemanticValue{ProbabilityCode{255,1}}};
    assert(probabilities.set_field_attributes<Age>({&age,1}));
    const AttributeInput associations{ContextAssociationLists::id,Attribute::probability,SemanticValue{ProbabilityCode{1,0}}};
    assert(probabilities.set_field_attributes<ContextAssociationLists>({&associations,1}));
    n=encode_packet(e,probabilities.freeze(),wire);assert(n);decoded=decode_packet(Bytes{wire}.first(*n));assert(decoded&&decoded->fields.size()==3);
    for(unsigned i=0;i<3;++i){assert(decoded->fields[i].value());assert(!decoded->fields[i].uuid());assert(!decoded->fields[i].duration());}
    assert(!probabilities.set_field_attributes<ShelfLife>({&uuid,1}));
    assert(!probabilities.set_field_attributes<ShelfLife>({}));
    // Different ASCII lengths for each attribute, transactional arena compaction.
    NativeContextPacket<64> text;assert(text.set<GPSASCII>({1,std::span<const char>{"ABC",3},{nullptr,sentence}}));
    const auto text_old=text.freeze();auto borrowed=text_old.get<GPSASCII>();assert(borrowed);
    const std::array<AttributeInput,2> strings{{{GPSASCII::id,Attribute::current,GpsAsciiInput{1,borrowed->text,{nullptr,sentence}}},
        {GPSASCII::id,Attribute::average,GpsAsciiInput{2,std::span<const char>{"1234567",7},{nullptr,sentence}}}}};
    EditWorkspace<BodyKind::values,16,64> workspace;
    const auto mask=attribute_bit(Attribute::current)|attribute_bit(Attribute::average);
    assert(text.with_attribute_inputs(mask,strings,workspace));
    auto current=text.freeze();assert(current.get<GPSASCII>(Attribute::average)->text.size()==7);
    assert(text_old.get<GPSASCII>()->text.size()==3);
    n=encode_packet(e,current,wire);assert(n);decoded=decode_packet(Bytes{wire}.first(*n));assert(decoded&&decoded->fields.size()==2);
    assert(decoded->fields[0].bytes.size()==12&&decoded->fields[1].bytes.size()==16);
    assert(decoded->fields[1].materialize_into(text,{nullptr,sentence}));
    for(unsigned i=0;i<100;++i)assert(text.set_attribute<GPSASCII>(Attribute::average,{3,std::span<const char>{"ABCDE",5},{nullptr,sentence}}));
    current=text.freeze();assert(current.native_size()==24);
    const auto raw=*current.fields()[0].values[0];const AttributeInput transplant{GPSASCII::id,Attribute::average,SemanticValue{raw}};
    const auto generation=text.generation();assert(!text.with_attribute_inputs(mask,{&transplant,1},workspace));assert(text.generation()==generation);
    std::array<char,80> huge{};huge.fill('X');assert(!text.set_attribute<GPSASCII>(Attribute::average,{1,huge,{nullptr,sentence}}));assert(text.generation()==generation);
    // Explicit generic selection and flat view bounds use the same parser.
    TypedPacket<BodyKind::selectors,PacketSubtype::query,128> query;
    for(unsigned b=3;b<23;++b)assert(query.select({2,static_cast<std::uint8_t>(b)}));assert(query.with_attributes(all_attributes));
    Envelope command;command.type=PacketType::command;command.stream_id=1;command.command=Command{0xa0040000,1,Identifier::short_id(2),Identifier::short_id(3)};
    n=encode_packet(command,query.freeze(),wire);assert(n);
    assert(!decode_packet(Bytes{wire}.first(*n)));
    auto generic=decode_packet_bounded<128,1664>(Bytes{wire}.first(*n));assert(generic&&generic->fields.size()==260);
    assert((!decode_packet_bounded<128,259>(Bytes{wire}.first(*n))));assert((decode_packet_bounded<128,260>(Bytes{wire}.first(*n))));
    unsigned calls=0;assert((!decode_and_visit_bounded<128,259>(Bytes{wire}.first(*n),{},[&](const FieldView&) noexcept -> Result<void>{++calls;return {};})));assert(!calls);
}
