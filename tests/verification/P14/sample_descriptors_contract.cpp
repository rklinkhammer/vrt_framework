#include <vita/codec/segmented_samples.hpp>
#include <array>
#include <cassert>
#include <cstdio>
using namespace vita;
namespace S=vita::codec::samples;
namespace G=vita::codec::general;
static PayloadFormat format(unsigned code,unsigned width,unsigned packing,unsigned kind=0,unsigned frac=0,unsigned tags=0,bool processing=false) {
    return {((std::uint64_t(!processing)<<31 | std::uint64_t(kind)<<29 | std::uint64_t(code)<<24 | std::uint64_t(tags)<<16 | std::uint64_t(frac)<<12 | (packing-1)<<6 | (width-1))<<32)};
}
static void write(std::span<std::byte> out,size_t bit,unsigned width,uint64_t value) {
    for(unsigned i=0;i<width;++i) if((value>>(width-i-1))&1)out[(bit+i)/8]|=std::byte(128u>>((bit+i)%8));
}
int main() {
    size_t cases=0;
    for(unsigned code=0;code<32;++code)for(unsigned width=1;width<=64;++width)for(unsigned frac=0;frac<16;++frac) {
        const bool fixed=code==0||code==16, non=code==7||code==23, vrt=(code>=1&&code<=6)||(code>=17&&code<=22);
        const bool floating=(code==13&&width==16)||(code==14&&width==32)||(code==15&&width==64);
        const bool valid=(fixed&&frac==0)||(non&&frac<width)||(vrt&&frac==0&&(code%16)<width)||(floating&&frac==0);
        auto f=format(code,width,width);
        f.bits|=std::uint64_t(frac)<<44;
        auto d=S::descriptor(f,S::SignalDomain::spectral);assert(bool(d)==valid);
        auto t=S::descriptor(f,S::SignalDomain::time);assert(bool(t)==(valid&&!non));
        auto l=S::descriptor(f,S::SignalDomain::spectral_log_power);assert(bool(l)==(valid&&width<=16&&(code==0||non)));
        ++cases;
    }
    // Independent bit writer and literal layout equations; no production pack/offset oracle.
    for(bool processing:{false,true})for(unsigned width=1;width<=64;++width)for(unsigned tags:{0u,1u,3u}) {
        const unsigned packing=width+tags;if(packing>64||(processing&&packing>32))continue;
        auto d=S::descriptor(format(0,width,packing,0,0,tags,processing),S::SignalDomain::time);assert(d);
        constexpr size_t count=9;
        const auto used=processing?((count-1)/(32/packing))*32+((count-1)%(32/packing)+1)*packing:count*packing;
        const auto bytes=((used+31)/32)*4;
        std::array<std::byte,80> wire{};
        std::array<G::Item,count> expected{};
        const auto mask=width==64?~uint64_t(0):(uint64_t(1)<<width)-1;
        for(size_t i=0;i<count;++i) {
            expected[i]={((0xa531f007a99573e1ULL*(i+1))&mask),tags?(i&((1u<<tags)-1)):0,0};
            const auto offset=processing?(i/(32/packing))*32+(i%(32/packing))*packing:i*packing;
            write(wire,offset,width,expected[i].data_bits);write(wire,offset+width,tags,expected[i].channel_tag);
        }
        const S::PayloadBinding binding{count,{S::PadReporting::exact,std::uint8_t(bytes*8-used)}};
        auto c=S::contiguous(Bytes(wire).first(bytes),*d,binding);assert(c);
        for(size_t split=0;split<=bytes;++split) {
            std::array<Bytes,3> segments{Bytes(wire).first(split),Bytes{},Bytes(wire).subspan(split,bytes-split)};
            auto view=S::SegmentedSamples<3>::create(segments,*d,binding);assert(view);
            segments={};
            for(size_t i=0;i<count;++i){assert(view->at(i)==expected[i]);assert(c->at(i)==expected[i]);}
            assert(!view->at(count));++cases;
        }
        auto bad=binding;bad.padding.class_id_pad_bits=std::uint8_t((bytes*8-used+1)%32);
        assert(!S::measure_payload(*d,bad));
        std::array<Bytes,4> excessive{};assert(!S::SegmentedSamples<3>::create(excessive,*d,binding));
        auto implied=binding;implied.padding={S::PadReporting::allow_zero_when_implied,0};
        assert(bool(S::measure_payload(*d,implied))==(bytes*8-used<width));
        auto absent=binding;absent.padding={S::PadReporting::omitted_by_class,std::nullopt};assert(S::measure_payload(*d,absent));
        absent.padding.class_id_pad_bits=0;assert(!S::measure_payload(*d,absent));
    }
    auto iq=S::descriptor({0xa00003cf00000000ULL},S::SignalDomain::time);assert(iq);
    assert(iq->packing().item_bits==16&&iq->packing().kind==G::SampleKind::cartesian);
    auto repeated=iq->wire();repeated.bits|=std::uint64_t(1)<<55;assert(!S::descriptor(repeated,S::SignalDomain::time));
    repeated.bits|=std::uint64_t(1)<<16;assert(S::descriptor(repeated,S::SignalDomain::time));
    auto huge=iq->wire();huge.bits|=65535;assert(!S::descriptor(huge,S::SignalDomain::time));
    auto empty=S::SegmentedSamples<0>::create({},*iq,{0,{S::PadReporting::exact,0}});assert(empty&&!empty->at(0));
    std::printf("independent DPF/segmented contract PASS: %zu matrix/fragment cases\n",cases);
}
