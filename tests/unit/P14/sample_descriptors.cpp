#include <vita/codec/segmented_samples.hpp>
#include <array>
#include <cassert>
#include <cstdlib>
#include <new>
using namespace vita;
using namespace vita::codec;
static unsigned allocations=0;
void* operator new(std::size_t n){++allocations;if(auto p=std::malloc(n))return p;std::abort();}
void operator delete(void*p) noexcept{std::free(p);}
void operator delete(void*p,std::size_t) noexcept{std::free(p);}
static PayloadFormat dpf(unsigned code,unsigned n,unsigned width,unsigned kind=0,unsigned fraction=0,
                         unsigned repeats=1,unsigned vectors=1,bool component=false,bool link=true,unsigned channel=0,unsigned event=0){
    std::uint32_t high=(std::uint32_t(link)<<31)|(kind<<29)|(code<<24)|(unsigned(component)<<23)
        |(event<<20)|(channel<<16)|(fraction<<12)|((width-1)<<6)|(n-1);
    return {std::uint64_t(high)<<32|((repeats-1)<<16)|(vectors-1)};
}
static constexpr samples::PayloadBinding omitted(std::size_t count){return {count,{samples::PadReporting::omitted_by_class,{}}};}
int main(){
    const auto before=allocations;
    // Independently literal IQ16 descriptor: link,Cartesian,signed fixed,16-bit field/item.
    auto iq=samples::descriptor(PayloadFormat{0xa00003cf00000000ULL},samples::SignalDomain::time);
    assert(iq && iq->packing().kind==general::SampleKind::cartesian && iq->packing().item_bits==16);
    for(unsigned code=0;code<32;++code){
        const unsigned width=code==13?16:code==15?64:32;
        auto result=samples::descriptor(dpf(code,width,width),samples::SignalDomain::spectral);
        const bool valid=code<=7||(code>=13&&code<=23);
        assert(bool(result)==valid);
    }
    assert(!samples::descriptor(dpf(13,32,32),samples::SignalDomain::time));
    assert(!samples::descriptor(dpf(0,16,16,3),samples::SignalDomain::time));
    assert(!samples::descriptor(dpf(7,16,16),samples::SignalDomain::time));
    assert(!samples::descriptor(dpf(16,16,16,0,1),samples::SignalDomain::spectral));
    assert(!samples::descriptor(dpf(7,3,3,0,3),samples::SignalDomain::spectral));
    assert(!samples::descriptor(dpf(6,6,6),samples::SignalDomain::spectral));
    assert(!samples::descriptor(dpf(0,16,16,0,0,1,65536),samples::SignalDomain::time));
    assert(!samples::descriptor(dpf(0,16,16,1,0,1,1,true),samples::SignalDomain::time));
    assert(!samples::descriptor(dpf(0,16,16,0,0,2,1,true),samples::SignalDomain::time));
    assert(!samples::descriptor(dpf(0,16,16,0,0,1,1,false,true,1),samples::SignalDomain::time));
    auto unsupported=samples::descriptor(dpf(15,64,64,0,0,1,1,false,false),samples::SignalDomain::time);
    assert(!unsupported && unsupported.error().code==ErrorCode::unsupported_layout);
    assert(samples::descriptor(dpf(0,16,16),samples::SignalDomain::spectral_log_power));
    assert(!samples::descriptor(dpf(16,16,16),samples::SignalDomain::spectral_log_power));
    assert(samples::descriptor(dpf(23,8,8,0,3),samples::SignalDomain::spectral_log_power));
    assert(!samples::descriptor(dpf(7,17,17),samples::SignalDomain::spectral_log_power));
    auto polar=samples::descriptor(dpf(0,8,8,2),samples::SignalDomain::time);assert(polar);
    assert(*samples::component_unit(*polar,1)==samples::ComponentUnit::pi_multiple);
    polar=samples::descriptor(dpf(23,8,8,2),samples::SignalDomain::spectral);assert(polar);
    assert(*samples::component_unit(*polar,1)==samples::ComponentUnit::unspecified);
    assert(!samples::component_unit(*polar,2));
    // Literal three 3-bit items 001,010,111: logical bits001010111 +23 pad bits.
    auto three=samples::descriptor(dpf(16,3,3),samples::SignalDomain::time);assert(three);
    std::array<std::byte,4> literal{std::byte{0x2b},std::byte{0x80},std::byte{0},std::byte{0}};
    auto binding=omitted(3);
    for(unsigned split=0;split<=4;++split){
        std::array<Bytes,2> pieces{Bytes(literal).first(split),Bytes(literal).subspan(split)};
        auto segmented=samples::SegmentedSamples<2>::create(pieces,*three,binding);assert(segmented);
        pieces={}; // copied segment descriptors do not borrow this array.
        assert(segmented->at(0)->data_bits==1 && segmented->at(1)->data_bits==2 && segmented->at(2)->data_bits==7);
        assert(!segmented->at(3));
    }
    assert(samples::contiguous(literal,*three,{3,{samples::PadReporting::exact,23}}));
    assert(!samples::contiguous(literal,*three,{3,{samples::PadReporting::exact,0}}));
    assert(!samples::contiguous(literal,*three,{3,{samples::PadReporting::exact,{}}}));
    assert(!samples::contiguous(literal,*three,{3,{samples::PadReporting::allow_zero_when_implied,0}}));
    auto fifteen=samples::descriptor(dpf(0,15,15),samples::SignalDomain::time);assert(fifteen);
    assert(samples::contiguous(literal,*fifteen,{2,{samples::PadReporting::allow_zero_when_implied,0}}));
    assert(!samples::contiguous(literal,*fifteen,{2,{samples::PadReporting::omitted_by_class,0}}));
    std::array<Bytes,2> excess{};assert(!samples::SegmentedSamples<1>::create(excess,*three,omitted(0)));
    assert(samples::SegmentedSamples<0>::create({},*three,omitted(0)));
    // Every byte boundary, all wide packing cases, processing mode, tags and complex repeated groups.
    for(unsigned n:{1u,3u,12u,17u,31u,33u,63u,64u})for(bool link:{false,true}){
        if(!link&&n>32)continue;
        const unsigned tags=n<=60?4:0,packing=n+tags;
        if(!link&&packing>32)continue;
        auto desc=samples::descriptor(dpf(0,n,packing,1,0,2,2,true,link,tags/2,tags/2),samples::SignalDomain::time);assert(desc);
        std::array<general::Item,8> items{};
        const auto mask=n==64?UINT64_MAX:(std::uint64_t{1}<<n)-1;
        for(unsigned i=0;i<8;++i)items[i]={std::uint64_t(0x91+i*23)&mask,tags?(i%4):0,tags?((i+1)%4):0};
        std::array<std::byte,80> storage{};
        auto written=general::pack(desc->packing(),1,items,storage);assert(written);
        auto wire=Bytes(storage).first(*written);
        auto view=samples::contiguous(wire,*desc,omitted(1));assert(view);
        for(std::size_t cut=0;cut<=wire.size();++cut){
            std::array<Bytes,3> pieces{wire.first(cut),{},wire.subspan(cut)};
            auto scattered=samples::SegmentedSamples<3>::create(pieces,*desc,omitted(1));assert(scattered);
            for(unsigned i=0;i<8;++i)assert(*scattered->at(i)==items[i] && *view->at(i)==items[i]);
        }
        std::array<Bytes,80> bytes{};for(std::size_t i=0;i<wire.size();++i)bytes[i]=wire.subspan(i,1);
        auto singles=samples::SegmentedSamples<80>::create(std::span(bytes).first(wire.size()),*desc,omitted(1));assert(singles);
        for(unsigned i=0;i<8;++i)assert(*singles->at(i)==items[i]);
    }
    assert(allocations==before);
}
