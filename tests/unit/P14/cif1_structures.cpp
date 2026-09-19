#include <vita/codec/packet.hpp>
#include <array>
#include <cassert>
#include <cstdio>
using namespace vita;
using namespace vita::codec;
static void literal(Bytes bytes,std::span<const std::uint32_t> words) {
    assert(bytes.size()==words.size()*4);
    for(std::size_t i=0;i<words.size();++i)for(unsigned b=0;b<4;++b)assert(bytes[i*4+b]==std::byte((words[i]>>(24-8*b))&255));
}
int main() {
    static_assert(sizeof(SemanticValue)==16&&sizeof(PacketSnapshot<BodyKind::values>)==5312&&sizeof(FieldView)==32);
    std::array<std::byte,4096> wire{};Envelope envelope;envelope.type=PacketType::context;envelope.stream_id=1;
    for(std::uint8_t width:{1,2,4}) {
        const std::array<std::uint32_t,3> entries{1,2,255};NativeContextPacket<> indices;
        assert(indices.set<IndexList>({width,entries}));auto old=indices.freeze();
        auto n=encode_packet(envelope,old,wire);assert(n);auto decoded=decode_packet(Bytes{wire}.first(*n));assert(decoded);
        auto view=decoded->fields[0].indices();assert(view&&view->size()==3);for(unsigned i=0;i<3;++i)assert(*view->at(i)==entries[i]);assert(!view->at(3));
        NativeContextPacket<> materialized;assert(decoded->fields[0].materialize_into(materialized));auto copy=materialized.freeze();assert(copy.get<IndexList>()->entry_bytes==width);
        const auto generation=indices.generation();const std::array<std::uint32_t,1> too_large{256};if(width==1){assert(!indices.replace<IndexList>({1,too_large}));assert(indices.generation()==generation);}
        for(std::size_t length=0;length<*n;++length)assert(!decode_packet(Bytes{wire}.first(length)));
        assert(indices.replace<IndexList>({width,{}}));auto empty=indices.freeze();assert(empty.get<IndexList>()->size()==0);assert(old.get<IndexList>()->size()==3);
    }
    PointingVectorRecord pointing{{0x1234,-128},PointingReference{0x1234,3,2}};
    NativeContextPacket<> vectors;assert(vectors.set<PointingVectorStructure>({PointingReference{0,2,1},true,{&pointing,1}}));
    auto n=encode_packet(envelope,vectors.freeze(),wire);assert(n);auto decoded=decode_packet(Bytes{wire}.first(*n));assert(decoded);
    const std::array<std::uint32_t,6> vector_words{6,0x04002001,0xc0000000,9,0x1234000e,0xff801234};literal(decoded->fields[0].bytes,vector_words);
    auto points=decoded->fields[0].pointing_vectors();assert(points&&*points->size()==1&&*points->at(0)==pointing);assert((*points->global())->reference==2);
    NativeContextPacket<> materialized;assert(decoded->fields[0].materialize_into(materialized));assert(!vectors.replace<PointingVectorStructure>({PointingReference{1,0,0},true,{&pointing,1}}));
    assert(vectors.replace<PointingVectorStructure>({{},false,{}}));
    SpectrumValue spectrum{1,3,1,43,1024,512,1<<20,2<<20,4,0xdeadbeef,-3,7,std::bit_cast<std::uint32_t>(-4096)};
    NativeContextPacket<> spectral;assert(spectral.set<Spectrum>(spectrum));n=encode_packet(envelope,spectral.freeze(),wire);assert(n);decoded=decode_packet(Bytes{wire}.first(*n));assert(decoded);
    const std::array<std::uint32_t,13> spectrum_words{0x00010301,43,1024,512,0,0x00100000,0,0x00200000,4,0xdeadbeef,0xfffffffd,7,0xfffff000};literal(decoded->fields[0].bytes,spectrum_words);assert(*decoded->fields[0].spectrum()==spectrum);
    spectrum.averaging=32;assert(!spectral.replace<Spectrum>(spectrum));spectrum.averaging=3;spectrum.resolution_q20=-1;assert(!spectral.replace<Spectrum>(spectrum));
    // Raw semantic violations remain readable and fail Current materialization.
    auto raw=decoded->fields[0].bytes;const auto offset=static_cast<std::size_t>(raw.data()-wire.data());wire[offset+2]=std::byte{32};decoded=decode_packet(Bytes{wire}.first(*n));assert(decoded);NativeContextPacket<> reject;assert(!decoded->fields[0].materialize_into(reject));
    SectorRecord sector;sector.sector=7;sector.f1_q20=1<<20;sector.f2_q20=2<<20;sector.bandwidth_q20=3<<20;sector.step_q20=4<<20;sector.points=5;sector.gain_q7={{-1,2}};sector.threshold_q7={{3,-4}};sector.dwell_fs=6;sector.start=SectorStartTime{0x5566778899aabbccull,0x11223344,2,2};sector.time3_fs=-7;sector.time4_fs=8;
    NativeContextPacket<> sectors;assert(sectors.set<SectorStepScan>({0xfff00000,{2,2,true},{&sector,1}}));assert(!measure(sectors.freeze()));assert(sectors.bind_timestamp_format(2,2));
    envelope.timestamp={Tsi::gps,Tsf::picoseconds,0,0};n=encode_packet(envelope,sectors.freeze(),wire);assert(n);decoded=decode_packet(Bytes{wire}.first(*n));assert(decoded);
    const std::array<std::uint32_t,24> sector_words{24,0x00015001,0xfff00000,7,0,0x00100000,0,0x00200000,0,0x00300000,0,0x00400000,5,0x0002ffff,0xfffc0003,0,6,0x11223344,0x55667788,0x99aabbcc,0xffffffff,0xfffffff9,0,8};literal(decoded->fields[0].bytes,sector_words);
    auto sector_view=decoded->fields[0].sectors();assert(sector_view&&*sector_view->size()==1&&*sector_view->at(0)==sector);
    NativeContextPacket<> copied;assert(decoded->fields[0].materialize_into(copied));assert(!measure(copied.freeze()));assert(copied.bind_timestamp_format(2,2));assert(measure(copied.freeze()));
    const auto before=sectors.freeze();assert(!sectors.bind_timestamp_format(1,2));assert(sectors.generation()==before.generation());
    std::array<std::byte,4096> output{};output.fill(std::byte{0xa5});auto untouched=output;envelope.timestamp.tsi=Tsi::utc;assert(!encode_packet(envelope,sectors.freeze(),output));assert(output==untouched);
    // Full source-defined optional-format matrix, including representation rejection00.
    for(unsigned tsi=0;tsi<4;++tsi)for(unsigned tsf=0;tsf<4;++tsf){SectorRecord one;one.start=SectorStartTime{tsf?UINT64_MAX:0,tsi?1u:0,static_cast<std::uint8_t>(tsi),static_cast<std::uint8_t>(tsf)};NativeContextPacket<> p;
        auto set=p.set<SectorStepScan>({0xc0400000,{static_cast<std::uint8_t>(tsi),static_cast<std::uint8_t>(tsf),true},{&one,1}});if(!tsi&&!tsf){assert(!set);continue;}assert(set);assert(p.bind_timestamp_format(tsi,tsf));envelope.timestamp={static_cast<Tsi>(tsi),static_cast<Tsf>(tsf),0,0};auto size=encode_packet(envelope,p.freeze(),wire);assert(size);auto d=decode_packet(Bytes{wire}.first(*size));assert(d&&*d->fields[0].sectors()->at(0)==one);}
    std::printf("IndexHeader=%zu PointingHeader=%zu SectorHeader=%zu PointingRecord=%zu SectorRecord=%zu Spectrum=%zu Input=%zu\n",sizeof(vita::detail::NativeIndexHeader),sizeof(vita::detail::NativePointingHeader),sizeof(vita::detail::NativeSectorHeader),sizeof(PointingVectorRecord),sizeof(SectorRecord),sizeof(SpectrumValue),sizeof(AttributeInput));
}
