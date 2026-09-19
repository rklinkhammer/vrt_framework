#include <vita/codec/general_samples.hpp>
#include <array>
#include <cassert>
#include <string>
#include <vector>
using namespace vita;
using namespace vita::codec::general;
// Independent oracle builds field bit strings and complete 32-bit words, rather
// than using the candidate's index/bit-offset helpers.
static std::string bits(std::uint64_t v,unsigned n) {
    std::string result(n,'0');for(unsigned i=0;i<n;++i)if((v>>i)&1)result[n-1-i]='1';return result;
}
static std::vector<std::byte> oracle(PackingSpec s,const std::vector<Item>& input) {
    std::string stream,word;
    for(auto item:input) {
        std::string field=bits(item.data_bits,s.item_bits)+std::string(s.packing_bits-s.item_bits-s.channel_tag_bits-s.event_tag_bits,'0')+
            bits(item.event_tag,s.event_tag_bits)+bits(item.channel_tag,s.channel_tag_bits);
        if(s.packing==PackingMode::processing && word.size()+field.size()>32){word.resize(32,'0');stream+=word;word.clear();}
        if(s.packing==PackingMode::link)stream+=field;else word+=field;
    }
    stream+=word;while(stream.size()%32)stream+='0';std::vector<std::byte> out;
    for(std::size_t i=0;i<stream.size();i+=8){unsigned v=0;for(unsigned b=0;b<8;++b)v=2*v+(stream[i+b]=='1');out.push_back(std::byte(v));}return out;
}
int main() {
    for(unsigned width=1;width<=64;++width)for(auto mode:{PackingMode::link,PackingMode::processing}) {
        if(mode==PackingMode::processing&&width>32)continue;
        PackingSpec s;s.packing_bits=width;s.item_bits=width;s.packing=mode;
        if(width>=9){s.item_bits=width-7;s.channel_tag_bits=3;s.event_tag_bits=2;}
        std::vector<Item> items;
        for(unsigned i=0;i<37;++i)items.push_back({s.item_bits==64?UINT64_MAX-i:((UINT64_MAX-i)&((std::uint64_t{1}<<s.item_bits)-1)),s.channel_tag_bits?i%8:0,s.event_tag_bits?i%4:0});
        auto expected=oracle(s,items);std::array<std::byte,512> out;out.fill(std::byte{0xaa});
        auto n=pack(s,items.size(),items,out);assert(n&&*n==expected.size());
        for(std::size_t i=0;i<expected.size();++i)assert(out[i]==expected[i]);for(std::size_t i=expected.size();i<out.size();++i)assert(out[i]==std::byte{0xaa});
        auto view=PackedSamples::create(Bytes{out}.first(*n),s,items.size());assert(view);
        for(std::size_t i=0;i<items.size();++i)assert(*view->at(i)==items[i]);assert(!view->at(items.size()));
        const auto saved=out;items.back().channel_tag=8;assert(!pack(s,items.size(),items,out));assert(out==saved);
        assert(!PackedSamples::create(Bytes{out}.first(*n-1),s,items.size()));
    }
    // Explicit component groups of three cross a two-channel vector boundary.
    PackingSpec s;s.kind=SampleKind::cartesian;s.repeating=RepeatMode::component;s.repeat_count=3;s.vector_size=2;
    std::array<std::byte,24> wire{};auto v=PackedSamples::create(wire,s,1);assert(v);
    const std::array<Coordinates,12> order{{{0,0,0,0},{0,0,1,0},{0,1,0,0},{0,0,0,1},{0,0,1,1},{0,1,0,1},
       {0,1,1,0},{0,2,0,0},{0,2,1,0},{0,1,1,1},{0,2,0,1},{0,2,1,1}}};
    for(unsigned i=0;i<12;++i){assert(*v->coordinates(i)==order[i]);assert(*v->index(order[i])==i);}
    s.repeating=RepeatMode::channel;v=PackedSamples::create(wire,s,1);assert(v);
    unsigned i=0;for(unsigned channel=0;channel<2;++channel)for(unsigned time=0;time<3;++time)for(unsigned component=0;component<2;++component){Coordinates c{0,time,channel,component};assert(*v->coordinates(i)==c);assert(*v->index(c)==i++);}
    assert(!v->index({0,3,0,0}));assert(!v->index({1,0,0,0}));
    s={};assert(measure(s,4096));assert(!measure(s,4097));assert(!measure(s,SIZE_MAX,Limits{SIZE_MAX,SIZE_MAX}));
    s.packing=PackingMode::processing;s.item_bits=s.packing_bits=33;auto unsupported=measure(s,1);assert(!unsupported&&unsupported.error().code==ErrorCode::unsupported_layout);
    s={};s.vector_size=65536;assert(!measure(s,0));s={};s.channel_tag_bits=16;assert(!measure(s,0));
    s={};std::array<Item,2> items{{{1,0,0},{2,0,0}}};
    auto overlapping=MutableBytes{reinterpret_cast<std::byte*>(items.data()),sizeof(items)};auto original=items;
    assert(!pack(s,2,items,overlapping));assert(items==original);
    auto empty=PackedSamples::create({},s,0);assert(empty&&!empty->at(0)&&!empty->coordinates(0));
    // Borrowed view reflects caller storage; copying the view does not claim ownership.
    std::array<std::byte,4> borrowed{std::byte{0x12},std::byte{0x34},std::byte{},std::byte{}};
    auto borrow=PackedSamples::create(borrowed,s,1);assert(borrow);auto copy=*borrow;borrowed[0]=std::byte{0xab};assert(copy.at(0)->data_bits==0xab34);
}
