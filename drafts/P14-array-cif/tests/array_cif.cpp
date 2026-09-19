#include <vita/codec/array_cif.hpp>
#include <array>
#include <cassert>
#include <cstdlib>
#include <new>
using namespace vita;
using namespace vita::codec;
namespace ac= vita::codec::array_cif;
static unsigned allocations=0;
void* operator new(std::size_t n){++allocations;if(auto p=std::malloc(n))return p;std::abort();}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}
struct Buffer {
    std::array<std::byte,20000> data{};std::size_t words=0;
    void put(std::size_t index,std::uint32_t value){for(unsigned b=0;b<4;++b)data[index*4+b]=std::byte((value>>(24-b*8))&255);}
    Bytes bytes()const{return Bytes(data).first(words*4);}
};
constexpr ac::Options options{ac::Dialect::i9_five_cifs_header7,{}};
static Buffer simple(unsigned records,bool field=true){
    Buffer b;const unsigned width=field?2:1;b.words=8+records*width;
    b.put(0,b.words);b.put(1,0x07000000|(width<<12)|records);b.put(3,field?0x40000000:0);
    for(unsigned i=0;i<records;++i){b.put(8+i*width,90+i);if(field)b.put(9+i*width,1234+i);}return b;
}
static Buffer wrap(const Buffer& child){
    Buffer b;b.words=9+child.words;b.put(0,b.words);b.put(1,0x07000001|((child.words+1)<<12));
    b.put(4,1u<<11);b.put(8,44);for(std::size_t i=0;i<child.words*4;++i)b.data[36+i]=child.data[i];return b;
}
int main(){
    const auto before=allocations;
    auto literal=simple(2);ac::Budget budget;
    budget.fields=budget.max_fields;
    auto short_root=ac::validate(literal.bytes().first(8),options,budget);
    assert(!short_root&&short_root.error().code==ErrorCode::short_input);
    budget={};
    auto view=ac::validate(literal.bytes(),options,budget);assert(view);
    assert(budget.fields==3&&budget.views==2&&budget.work==5&&view->record_count()==2);
    assert(!view->record(2));
    unsigned visited=0;
    assert(view->visit([&](const ac::Element& e)noexcept->Result<void>{
        assert(e.field.id==ReferencePoint::id&&e.path.depth==1);
        assert(e.path.steps[0].ordinal==visited&&e.path.steps[0].index==90+visited);
        assert(codec::detail::load32(e.field.bytes,0)==1234+visited);++visited;return {};
    }));assert(visited==2);
    // Fixed five words stay present even with no ordinary CIF enable bits.
    literal.put(3,0xc000000e);budget={};assert(ac::validate(literal.bytes(),options,budget));
    literal.put(3,0x40000080);budget={};auto unsupported=ac::validate(literal.bytes(),options,budget);
    assert(!unsupported&&unsupported.error().code==ErrorCode::unsupported_capability&&budget.fields==0);
    literal.put(7,0x80000000);assert(ac::validate(literal.bytes(),options,budget));
    literal.put(3,0x40000000);budget={};assert(!ac::validate(literal.bytes(),options,budget));
    literal=simple(2);literal.put(0,literal.words+2);budget={};assert(!ac::validate(literal.bytes(),options,budget));
    literal=simple(2);literal.put(1,0x05002002);assert(!ac::validate(literal.bytes(),options,budget));
    literal=simple(2);literal.put(2,1);assert(!ac::validate(literal.bytes(),options,budget));
    literal=simple(2);literal.put(3,0x40000010);assert(!ac::validate(literal.bytes(),options,budget));
    literal=simple(2);literal.put(4,1u<<23);assert(!ac::validate(literal.bytes(),options,budget));
    literal=simple(0);budget={};assert(ac::validate(literal.bytes(),options,budget));
    literal.put(4,1u<<23);assert(!ac::validate(literal.bytes(),options,budget)); // schema still checked
    literal=simple(256,false);budget={};assert(ac::validate(literal.bytes(),options,budget));
    literal=simple(257,false);budget={};assert(!ac::validate(literal.bytes(),options,budget));
    literal=simple(127);budget={};assert(ac::validate(literal.bytes(),options,budget));assert(budget.fields==128);
    literal=simple(128);budget={};assert(!ac::validate(literal.bytes(),options,budget));
    literal=simple(1);budget={};budget.work=4094;assert(!ac::validate(literal.bytes(),options,budget));assert(budget.work==4094);
    budget={};budget.work=4093;assert(ac::validate(literal.bytes(),options,budget));assert(budget.work==4096);
    assert(!ac::validate(literal.bytes(),options,budget)); // shared sibling budget not reset
    auto nested=simple(1);for(unsigned depth=1;depth<4;++depth)nested=wrap(nested);
    budget={};view=ac::validate(nested.bytes(),options,budget);assert(view);visited=0;
    assert(view->visit([&](const ac::Element&e)noexcept->Result<void>{if(e.field.id==ReferencePoint::id){assert(e.path.depth==4);++visited;}return {};}));assert(visited==1);
    nested=wrap(nested);budget={};auto depthfail=ac::validate(nested.bytes(),options,budget);assert(!depthfail&&depthfail.error().code==ErrorCode::resource_limit);
    // Two siblings independently valid cannot each spend a fresh shared work budget.
    auto child=simple(1);Buffer siblings;const auto width=1+child.words;siblings.words=8+2*width;
    siblings.put(0,siblings.words);siblings.put(1,0x07000002|(width<<12));siblings.put(4,1u<<11);
    for(unsigned r=0;r<2;++r){siblings.put(8+r*width,r);for(std::size_t i=0;i<child.words*4;++i)siblings.data[(9+r*width)*4+i]=child.data[i];}
    budget={};budget.max_work=8;assert(!ac::validate(siblings.bytes(),options,budget));
    budget.max_work=9;assert(ac::validate(siblings.bytes(),options,budget));
    // Array's Probability/Belief are one-word attributes, not nested headers.
    Buffer probability;probability.words=10;probability.put(0,10);probability.put(1,0x07002001);
    probability.put(3,0x80);probability.put(4,1u<<11);probability.put(7,attribute_bit(Attribute::belief));
    probability.put(8,3);probability.put(9,100);budget={};assert(ac::validate(probability.bytes(),options,budget));
    // A temporal child uses inherited packet timestamp format, never guessed words.
    Buffer age;age.words=10;age.put(0,10);age.put(1,0x07002001);age.put(6,1u<<17);age.put(8,7);age.put(9,3);
    budget={};assert(!ac::validate(age.bytes(),options,budget));
    budget={};assert(ac::validate(age.bytes(),{ac::Dialect::i9_five_cifs_header7,{1,0,true}},budget));
    // Attribute materialization shares the same view ceiling across records.
    Buffer attributes;attributes.words=36;attributes.put(0,36);attributes.put(1,0x0700e002);
    attributes.put(3,0x40000080);attributes.put(7,all_attributes);
    for(unsigned r=0;r<2;++r){attributes.put(8+r*14,r);for(unsigned a=0;a<13;++a)attributes.put(9+r*14+a,0);}
    budget={};budget.max_views=25;assert(!ac::validate(attributes.bytes(),options,budget));
    budget.max_views=26;assert(ac::validate(attributes.bytes(),options,budget));
    assert(budget.views==26);
    Buffer indices;indices.words=12;indices.put(0,12);indices.put(1,0x07004001);
    indices.put(4,1u<<7);indices.put(8,1);indices.put(9,3);indices.put(10,0x40000001);indices.put(11,77);
    budget={};budget.field_limits.index_entries=0;assert(!ac::validate(indices.bytes(),options,budget));
    budget.field_limits.index_entries=1;assert(ac::validate(indices.bytes(),options,budget));
    // Default resolver path remains the ordinary packet parser.
    std::array<std::byte,16> packet{std::byte{0x40},std::byte{0},std::byte{0},std::byte{4},
        std::byte{0},std::byte{0},std::byte{0},std::byte{1},
        std::byte{0x40},std::byte{0},std::byte{0},std::byte{0},std::byte{0},std::byte{0},std::byte{0},std::byte{9}};
    auto ordinary=decode_packet(packet);assert(ordinary&&ordinary->fields.size()==1);
    assert(allocations==before);
}
