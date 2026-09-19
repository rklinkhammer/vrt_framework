#include <vita/codec/packet.hpp>
#include <array>
#include <cassert>
#include <cstdlib>
#include <new>
#include <type_traits>
using namespace vita;using namespace vita::codec;
static_assert(sizeof(SemanticValue)==16);
static_assert(sizeof(LayoutContext)==48);
static_assert(sizeof(FieldView)==32);
static_assert(sizeof(decltype(ContextPacket{}.freeze()))==5312);
static_assert(!std::is_constructible_v<NativeRecordView<SectorRecord>,Bytes>);
static_assert(!std::is_constructible_v<NativeRecordView<PointingVectorRecord>,Bytes>);
static_assert(std::is_copy_constructible_v<NativeRecordView<SectorRecord>>);
static unsigned allocations=0;
void* operator new(std::size_t n){++allocations;auto*p=std::malloc(n?n:1);if(!p)std::abort();return p;}
void* operator new[](std::size_t n){return ::operator new(n);}
void* operator new(std::size_t n,std::align_val_t a){++allocations;void*p=nullptr;if(posix_memalign(&p,static_cast<std::size_t>(a),n?n:1))std::abort();return p;}
void* operator new[](std::size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}
int main(){
 assert(!NativeRecordView<SectorRecord>{}.at(0));assert(!NativeRecordView<PointingVectorRecord>{}.at(0));
 std::array<std::uint32_t,3> entries{7,1,255};
 std::array<PointingVectorRecord,2> records{{{{128,256},PointingReference{1,2,1}},{{384,-256},PointingReference{2,0,0}}}};
 SpectrumValue spectrum;spectrum.spectrum_type=128;spectrum.window_type=100;spectrum.weighting_raw=0xdeadbeef;spectrum.resolution_q20=1;spectrum.span_q20=2;
 SectorRecord sector;sector.sector=77;sector.f1_q20=123;sector.dwell_fs=1000;
 NativeContextPacket<8192> b;assert(b.set<IndexList>(IndexListInput{1,entries}));assert(b.set<PointingVectorStructure>(PointingVectorInput{PointingReference{0,3,2},true,records}));assert(b.set<Spectrum>(spectrum));assert(b.set<SectorStepScan>(SectorStepScanInput{0xc0800000,{},std::span{&sector,1}}));
 auto initial=b.freeze();entries.fill(0);records[0].vector.azimuth_q7=0;sector.sector=0;spectrum.weighting_raw=0;
 auto indices=initial.get<IndexList>();assert(indices&&indices->at(0)==7&&indices->at(2)==255&&!indices->at(3));
 auto vectors=initial.get<PointingVectorStructure>();assert(vectors&&vectors->global==(PointingReference{0,3,2})&&vectors->records.at(0)->vector.azimuth_q7==128&&!vectors->records.at(2));
 assert(initial.get<Spectrum>()->weighting_raw==0xdeadbeef);auto sectors=initial.get<SectorStepScan>();assert(sectors&&sectors->records.at(0)->sector==77);
 Envelope e;e.type=PacketType::context;e.stream_id=1;std::array<std::byte,4096> original{},after{};auto length=encode_packet(e,initial,original);assert(length);
 // Sibling attributes have independent ownership and shape, including empty Index List.
 std::array<AttributeInput,4> inputs{{{IndexList::id,Attribute::minimum,IndexListInput{4,{}}},{PointingVectorStructure::id,Attribute::minimum,PointingVectorInput{}},{Spectrum::id,Attribute::minimum,spectrum},{SectorStepScan::id,Attribute::minimum,SectorStepScanInput{}}}};
 EditWorkspace<BodyKind::values,16,8192> workspace;assert(b.with_attribute_inputs(attribute_bit(Attribute::current)|attribute_bit(Attribute::minimum),inputs,workspace));
 auto expanded=b.freeze();assert(expanded.get<IndexList>(Attribute::minimum)->size()==0);assert(expanded.get<IndexList>()->at(0)==7);assert(expanded.get<PointingVectorStructure>(Attribute::minimum)->records.size()==0);
 assert(encode_packet(e,initial,after));assert(original==after);
 auto generation=expanded.generation();std::array<std::uint32_t,1> too_wide{256};assert(!b.replace_attribute<IndexList>(Attribute::minimum,IndexListInput{1,too_wide}));assert(b.freeze().generation()==generation);
 std::array<std::uint32_t,1025> too_many{};assert(!b.replace_attribute<IndexList>(Attribute::minimum,IndexListInput{4,too_many}));assert(b.freeze().generation()==generation);
 SpectrumValue invalid;invalid.averaging=32;assert(!b.replace_attribute<Spectrum>(Attribute::current,invalid));assert(b.freeze().generation()==generation);assert(b.replace_attribute<Spectrum>(Attribute::minimum,invalid));
 auto stable=b.freeze();auto parsed=decode_packet(Bytes{original}.first(*length));assert(parsed);NativeContextPacket<8192> restored;for(std::size_t i=0;i<parsed->fields.size();++i)assert(parsed->fields[i].materialize_into(restored));std::array<std::byte,4096> rebuilt{};assert(encode_packet(e,restored.freeze(),rebuilt));assert(rebuilt==original);
 NativeContextPacket<1> no_room;assert(!no_room.set<IndexList>(IndexListInput{4,too_wide}));assert(no_room.freeze().fields().empty());
 // Probability/Belief do not instantiate base native layouts or Sector timestamp binding.
 ContextPacket codes;assert(codes.with_attributes(attribute_bit(Attribute::probability)|attribute_bit(Attribute::belief)));
 auto insert=[&]<class Field>(){std::array<AttributeInput,2> attrs{{{Field::id,Attribute::probability,SemanticValue{ProbabilityCode{17,33}}},{Field::id,Attribute::belief,SemanticValue{BeliefCode{99}}}}};assert(codes.set_field_attributes<Field>(attrs));};
 insert.template operator()<IndexList>();insert.template operator()<PointingVectorStructure>();insert.template operator()<Spectrum>();insert.template operator()<SectorStepScan>();assert(measure(codes.freeze()));assert(encode_packet(e,codes.freeze(),rebuilt));
 assert(stable.get<IndexList>()->at(0)==7);assert(allocations==0);auto*p=::operator new(5);::operator delete(p);p=::operator new(64,std::align_val_t{64});::operator delete(p,std::align_val_t{64});assert(allocations==2);
}
