#include <vita/codec/packet.hpp>
#include <cassert>
#include <cstdlib>
#include <new>
using namespace vita;using namespace vita::codec;
static std::size_t allocs=0;
void* operator new(std::size_t n){++allocs;if(auto p=std::malloc(n?n:1))return p;std::abort();}void* operator new[](std::size_t n){return ::operator new(n);}void* operator new(std::size_t n,std::align_val_t a){++allocs;void*p=nullptr;if(!posix_memalign(&p,std::size_t(a),n?n:1))return p;std::abort();}void* operator new[](std::size_t n,std::align_val_t a){return ::operator new(n,a);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}

int main(){Envelope e;e.type=PacketType::context;e.stream_id=1;e.timestamp={Tsi::gps,Tsf::picoseconds,1,0};std::array<std::byte,128>out{};auto before=allocs;
for(unsigned i=0;i<1000;++i){NativeContextPacket<32> b;assert(b.bind_timestamp_format(2,2));assert(b.set<Age>(StateDurationValue{UINT64_MAX,i,2,2}));assert(b.set<ShelfLife>(StateDurationValue{UINT64_MAX-i,UINT32_MAX,2,2}));auto original=b.freeze();auto copy=original;assert(copy.get<Age>()==original.get<Age>());auto m=measure(copy);assert(m);auto n=encode_packet(e,copy,out);assert(n);auto p=decode_packet(Bytes{out}.first(*n));assert(p);NativeContextPacket<32> dest;assert(dest.bind_timestamp_format(2,2));for(std::size_t j=0;j<p->fields.size();++j)assert(p->fields[j].materialize_into(dest));auto s=dest.freeze();assert(s.get<Age>()==original.get<Age>());assert(b.remove<Age>());assert(b.replace<ShelfLife>(StateDurationValue{0,0,2,2}));assert(original.get<ShelfLife>()->seconds==UINT32_MAX);}
assert(allocs==before);auto*p=::operator new(32);auto*q=::operator new(64,std::align_val_t{64});assert(allocs==before+2);::operator delete(p);::operator delete(q,std::align_val_t{64});}
