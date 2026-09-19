#include <vita/codec/packet.hpp>
#include <cassert>
#include <vector>
using namespace vita;using namespace vita::codec;
static void word(std::vector<std::byte>&b,std::uint32_t v){for(int i=3;i>=0;--i)b.push_back(std::byte(v>>(8*i)));}
static void fix(std::vector<std::byte>&b){auto v=0x40000000u|static_cast<unsigned>(b.size()/4);for(unsigned i=0;i<4;++i)b[i]=std::byte(v>>(24-8*i));}
static std::vector<std::byte> index(unsigned n,unsigned attributes=0){std::vector<std::byte>b;for(auto v:{0u,1u,2u|(attributes?0x80u:0u),1u<<7})word(b,v);if(attributes)word(b,attributes);unsigned siblings=attributes?2:1;for(unsigned a=0;a<siblings;++a){word(b,2+n);word(b,0x40000000|n);for(unsigned i=0;i<n;++i)word(b,i+a);}fix(b);return b;}
static std::vector<std::byte> pointing(unsigned n){std::vector<std::byte>b;for(auto v:{0u,1u,2u,1u<<28,3+n,0x03001000u|n,0x40000000u})word(b,v);for(unsigned i=0;i<n;++i)word(b,i);fix(b);return b;}
static void rejected(Bytes b,DecodeOptions o={}){unsigned callbacks=0;auto r=decode_and_visit(b,o,[&](FieldView)noexcept->Result<void>{++callbacks;return {};});assert(!r&&callbacks==0);}
int main(){
 auto at_index=index(1024),over_index=index(1025);assert(decode_packet(at_index));rejected(over_index);DecodeOptions expanded;expanded.limits.index_entries=1025;assert(decode_packet(over_index,expanded));
 auto at_records=pointing(256),over_records=pointing(257);assert(decode_packet(at_records));rejected(over_records);expanded={};expanded.limits.records=257;assert(decode_packet(over_records,expanded));
 auto siblings=index(1024,0xc0000000);assert(decode_packet(siblings));DecodeOptions exact;exact.limits.work_units=2052;assert(decode_packet(siblings,exact));exact.limits.work_units=2051;rejected(siblings,exact);
 auto native_limit=pointing(4095);DecodeOptions large;large.limits.records=4095;large.limits.work_units=4098;assert(decode_packet(native_limit,large));large.limits.work_units=4097;rejected(native_limit,large);
 // Declared shape corruption remains malformed even when a resource limit is lower.
 auto malformed=over_records;malformed.pop_back();expanded.limits.records=0;rejected(malformed,expanded);
 // Every strict prefix rejects before visitor callbacks, including count/header boundary splits.
 for(auto*sample:{&at_index,&at_records})for(std::size_t n=0;n<sample->size();++n)rejected(Bytes{*sample}.first(n));
}
