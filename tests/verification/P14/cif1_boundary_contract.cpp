#include <vita/codec/packet.hpp>
#include <cassert>
#include <vector>
using namespace vita;using namespace vita::codec;
static std::vector<std::byte> packet(unsigned bit,std::initializer_list<std::uint32_t> value){std::vector<std::uint32_t> words{0x40000000u|static_cast<unsigned>(4+value.size()),1,2,1u<<bit};words.insert(words.end(),value);std::vector<std::byte> b;for(auto w:words)for(int i=3;i>=0;--i)b.push_back(std::byte((w>>(8*i))&255));return b;}
static void reserved(unsigned bit,std::initializer_list<std::uint32_t> values){auto b=packet(bit,values);unsigned calls=0;auto r=decode_and_visit(b,{},[&](FieldView)noexcept->Result<void>{++calls;return {};});assert(!r&&calls==0);}
int main(){
 OptionalQ7 empty;assert(!empty.has_value()&&!empty.get()&&empty.value_or(7)==7);OptionalQ7 known{-128};assert(known.has_value()&&*known.get()==-128&&known.value_or(7)==-128);
 ContextPacket p;assert(p.set<PhaseOffset>(RadiansQ7{INT16_MIN}));assert(p.replace<PhaseOffset>(RadiansQ7{INT16_MAX}));assert(p.set<Polarization>(PolarizationAngles{INT16_MIN,INT16_MAX}));
 assert(p.set<PointingVector3D>(PointingAngles{65535,11520}));auto before=p.freeze();assert(!p.replace<PointingVector3D>(PointingAngles{0,11521}));assert(p.freeze().generation()==before.generation());
 assert(!validate_value(SpatialReferenceType::id,SpatialReferenceValue{0,0,3}));
 for(auto bit:{31u,27u,18u,4u})reserved(bit,{0x00010000});reserved(26,{0x00000010});reserved(26,{3});reserved(1,{1,0x00010000});
 for(auto [bit,raw]:std::array<std::pair<unsigned,std::uint32_t>,4>{{{29,0x2d010000},{20,1},{16,0x0000ffff},{3,0}}}){auto bytes=packet(bit,{raw});auto parsed=decode_packet(bytes);assert(parsed);auto v=parsed->fields[0].value();assert(v&&!validate_value({1,static_cast<std::uint8_t>(bit)},*v));ContextPacket target;assert(!parsed->fields[0].materialize_into(target));assert(target.freeze().fields().empty());}
 auto bw_bytes=packet(13,{0xffffffff,0xffffffff});auto bw=decode_packet(bw_bytes);assert(bw);assert(!validate_value(AuxiliaryBandwidth::id,*bw->fields[0].value()));
 auto nfbytes=packet(16,{0x7fff7fff});auto nf=decode_packet(nfbytes);assert(nf);auto decoded=std::get<SnrNoiseFigureValue>(*nf->fields[0].value());assert(!decoded.snr_q7&&decoded.noise_figure_q7==INT16_MAX&&validate_value(SnrNoiseFigure::id,decoded));
 auto zero_bytes=packet(16,{0});auto zero=decode_packet(zero_bytes);auto zs=std::get<SnrNoiseFigureValue>(*zero->fields[0].value());assert(zs.snr_q7==0&&!zs.noise_figure_q7);assert(!validate_value(SnrNoiseFigure::id,SnrNoiseFigureValue{0,0}));
 assert(!validate_value(EbNoBer::id,EbNoBerValue{INT16_MAX,0}));assert(!validate_value(InterceptPoints::id,InterceptPointsValue{0,INT16_MAX}));assert(validate_value(EbNoBer::id,EbNoBerValue{0,std::nullopt}));
 // Generic Threshold pair is preserved; only the explicit class mode applies usage restrictions.
 assert(validate_value(Threshold::id,ThresholdValue{10,-10}));assert(validate_threshold({10,0},ThresholdMode::single_db));assert(!validate_threshold({10,-32768},ThresholdMode::single_db));assert(validate_threshold({10,-32768},ThresholdMode::single_dbm));assert(!validate_threshold({10,0},ThresholdMode::single_dbm));
 for(auto mode:{ThresholdMode::window_db,ThresholdMode::window_dbm}){assert(validate_threshold({10,11},mode));assert(!validate_threshold({10,10},mode));assert(!validate_threshold({10,9},mode));}assert(!validate_threshold({0,0},static_cast<ThresholdMode>(99)));
 for(auto bad:std::array<VersionBuildValue,5>{{{128,1,0,0},{0,0,0,0},{0,367,0,0},{0,1,64,0},{0,1,0,1024}}})assert(!validate_value(VersionBuild::id,bad));assert(validate_value(VersionBuild::id,VersionBuildValue{1,366,0,0}));
 // No beam engineering interpretation and no assumed buffer-fullness threshold.
 assert(validate_value(BeamWidth::id,BeamWidthCode{0xffff,0xb400}));for(unsigned level:{0u,1u,128u,255u})assert(validate_value(BufferSize::id,BufferSizeValue{4096,static_cast<std::uint8_t>(level),0xff}));
}
