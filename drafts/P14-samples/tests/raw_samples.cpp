#include <vita/codec/general_samples.hpp>
#include <array>
#include <cassert>
#include <cstdlib>
#include <new>
using namespace vita;
using namespace vita::codec::general;
static bool monitor=false;
static unsigned allocations=0;
void* operator new(std::size_t n) { if(monitor)++allocations; if(auto p=std::malloc(n?n:1))return p;std::abort(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
template<std::size_t N> void equals(Bytes bytes,const std::array<unsigned,N>& expected) {
    assert(bytes.size()==N);for(std::size_t i=0;i<N;++i)assert(std::to_integer<unsigned>(bytes[i])==expected[i]);
}
int main() {
    monitor=true;
    std::array<std::byte,64> out{};
    PackingSpec s;s.item_bits=3;s.packing_bits=3;
    std::array<Item,3> a{{{1},{2},{7}}};
    assert(pack(s,3,a,out)==4);
    equals(Bytes{out}.first(4),std::array<unsigned,4>{0x2b,0x80,0,0});
    auto v=PackedSamples::create(Bytes{out}.first(4),s,3);assert(v && v->at(2)==a[2]);
    assert(!v->at(3));assert(!PackedSamples::create(Bytes{out}.first(3),s,3));assert(!PackedSamples::create(Bytes{out}.first(8),s,3));
    s.packing_bits=8;s.channel_tag_bits=2;s.event_tag_bits=1;
    std::array<Item,2> tagged{{{5,2,1},{2,1,0}}};
    assert(pack(s,2,tagged,out)==4);
    equals(Bytes{out}.first(4),std::array<unsigned,4>{0xa6,0x41,0,0});
    v=PackedSamples::create(Bytes{out}.first(4),s,2);assert(v && v->at(0)==tagged[0] && v->at(1)==tagged[1]);
    // Invalid late input and short output must preserve every output byte.
    for(auto& x:out)x=std::byte{0x55};tagged[1].event_tag=2;
    assert(!pack(s,2,tagged,out));for(auto x:out)assert(x==std::byte{0x55});
    tagged[1].event_tag=0;assert(!pack(s,2,tagged,MutableBytes{out}.first(3)));for(auto x:out)assert(x==std::byte{0x55});
    s={};s.item_bits=3;s.packing_bits=3;s.packing=PackingMode::processing;
    std::array<Item,11> p{};for(auto& x:p)x.data_bits=7;
    assert(pack(s,11,p,out)==8);
    equals(Bytes{out}.first(8),std::array<unsigned,8>{255,255,255,252,224,0,0,0});
    // Processing unused bits are recommended zeros, not mandatory malformed input.
    out[3]|=std::byte{3};out[7]=std::byte{255};
    v=PackedSamples::create(Bytes{out}.first(8),s,11);assert(v && v->at(10)->data_bits==7);
    s.packing_bits=33;s.item_bits=33;assert(measure(s,1).error().code==ErrorCode::unsupported_layout);
    s.packing=PackingMode::link;
    std::array<Item,2> wide{{{0x1ffffffffULL},{0x100000000ULL}}};
    assert(pack(s,2,wide,out)==12);
    equals(Bytes{out}.first(12),std::array<unsigned,12>{255,255,255,255,192,0,0,0,0,0,0,0});
    v=PackedSamples::create(Bytes{out}.first(12),s,2);assert(v && v->at(0)==wide[0] && v->at(1)==wide[1]);
    s.item_bits=s.packing_bits=64;
    wide={Item{0x7ff8000000000001ULL},Item{0x8000000000000000ULL}};
    assert(pack(s,2,wide,out)==16);
    equals(Bytes{out}.first(16),std::array<unsigned,16>{127,248,0,0,0,0,0,1,128,0,0,0,0,0,0,0});
    v=PackedSamples::create(Bytes{out}.first(16),s,2);assert(v && v->at(0)==wide[0] && v->at(1)==wide[1]);
    // Independent coordinate lists for one vector2/repeat2 complex structure.
    s={};s.kind=SampleKind::cartesian;s.vector_size=2;s.repeat_count=2;s.repeating=RepeatMode::component;
    std::array<Item,8> items{};for(unsigned i=0;i<8;++i)items[i].data_bits=i;
    assert(pack(s,1,items,out)==16);v=PackedSamples::create(Bytes{out}.first(16),s,1);assert(v);
    const std::array<Coordinates,8> expected{{{0,0,0,0},{0,0,1,0},{0,0,0,1},{0,0,1,1},{0,1,0,0},{0,1,1,0},{0,1,0,1},{0,1,1,1}}};
    for(unsigned i=0;i<8;++i){assert(v->coordinates(i)==expected[i]);assert(v->index(expected[i])==i);}
    s.repeating=RepeatMode::channel;v=PackedSamples::create(Bytes{out}.first(16),s,1);assert(v);
    const std::array<Coordinates,8> ch{{{0,0,0,0},{0,0,0,1},{0,1,0,0},{0,1,0,1},{0,0,1,0},{0,0,1,1},{0,1,1,0},{0,1,1,1}}};
    for(unsigned i=0;i<8;++i){assert(v->coordinates(i)==ch[i]);assert(v->index(ch[i])==i);}
    assert(!v->index({1,0,0,0}));assert(!v->index({0,2,0,0}));
    // Bounded exhaustive packing dimensions: asymmetric tags and all bit positions.
    std::array<Item,5> sweep{};
    for(unsigned width=1;width<=64;++width)for(auto mode:{PackingMode::link,PackingMode::processing}) {
        if(mode==PackingMode::processing && width>32)continue;
        s={};s.item_bits=width;s.packing_bits=width;s.packing=mode;
        const auto mask=width==64?~std::uint64_t{0}:(std::uint64_t{1}<<width)-1;
        for(unsigned j=0;j<5;++j)sweep[j].data_bits=(0xfedcba9876543210ULL>>(j*3))&mask;
        const auto n=pack(s,5,sweep,out);assert(n);
        auto view=PackedSamples::create(Bytes{out}.first(*n),s,5);assert(view);
        for(unsigned j=0;j<5;++j)assert(view->at(j)==sweep[j]);
    }
    s={};assert(measure(s,4097).error().code==ErrorCode::resource_limit);
    assert(measure(s,std::numeric_limits<std::size_t>::max(),{std::numeric_limits<std::size_t>::max(),std::numeric_limits<std::size_t>::max()}).error().code==ErrorCode::overflow);
    s.channel_tag_bits=15;s.event_tag_bits=7;assert(!measure(s,1));
    s={};s.vector_size=65536;assert(!measure(s,1));s.vector_size=0;assert(!measure(s,1));
    s={};s.kind=static_cast<SampleKind>(42);assert(!measure(s,1));
    s={};s.repeating=RepeatMode::component;assert(!measure(s,1));
    s={};assert(pack(s,0,{},out)==0);assert(PackedSamples::create({},s,0));
    // Overlapping source/output is rejected before object storage mutation.
    std::array<Item,1> alias{{{3}}};
    auto bytes=MutableBytes{reinterpret_cast<std::byte*>(alias.data()),sizeof(alias)};
    assert(!pack(s,1,alias,bytes));assert(alias[0].data_bits==3);
    monitor=false;assert(allocations==0);
}
