#include <vita/codec/packet.hpp>
#include <cstdlib>
#include <new>
#include <cassert>
using namespace vita;
static std::size_t allocations=0;
void* operator new(std::size_t n){++allocations;if(void*p=std::malloc(n?n:1))return p;std::abort();}
void* operator new[](std::size_t n){return ::operator new(n);}
void* operator new(std::size_t n,std::align_val_t a){++allocations;void*p=nullptr;if(!posix_memalign(&p,std::size_t(a),n?n:1))return p;std::abort();}
void* operator new[](std::size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}
static Result<void> sentence(void*,std::span<const char>) noexcept{return {};}
int main(){
 const auto before=allocations;
 for(unsigned i=0;i<1000;++i){NativeContextPacket<> p;std::array<std::uint32_t,16> ids{};for(unsigned n=0;n<ids.size();++n)ids[n]=i+n;const std::array<char,4> text{'$','X','\r','\n'};
 assert(p.set<FormattedGPS>(GeolocationValue{}));assert(p.set<ECEFEphemeris>(EphemerisValue{}));assert(p.set<GPSASCII>(GpsAsciiInput{1,text,{nullptr,sentence}}));assert(p.set<ContextAssociationLists>(AssociationListsInput{ids,{},{},{},{},false}));
 auto snapshot=p.freeze();assert(snapshot.get<GPSASCII>());assert(snapshot.get<ContextAssociationLists>());assert(p.replace<GPSASCII>(GpsAsciiInput{2,text,{nullptr,sentence}}));assert(p.remove<FormattedGPS>());
 std::array<std::byte,1024> out{};codec::Envelope e;e.type=codec::PacketType::context;e.stream_id=1;auto encoded=codec::encode_packet(e,snapshot,out);assert(encoded);auto parsed=codec::decode_packet(Bytes{out}.first(*encoded));assert(parsed&&parsed->fields.size()==4);
 }
 assert(allocations==before);
 // Deliberately exercise each instrumented allocation form after the measured path.
 auto* a=::operator new(32);auto* b=::operator new(64,std::align_val_t{64});assert(allocations==before+2);::operator delete(a);::operator delete(b,std::align_val_t{64});
}
