#include <vita/codec/packet.hpp>
#include <cassert>
#include <vector>
using namespace vita;using namespace vita::codec;
static void word(std::vector<std::byte>& bytes,std::size_t n,std::uint32_t w){for(unsigned i=0;i<4;++i)bytes[4*n+i]=std::byte((w>>(24-i*8))&255);}
static std::vector<std::byte> association(unsigned count){std::vector<std::byte> b((5+count)*4);word(b,0,0x40000000u|(5+count));word(b,1,1);word(b,2,1u<<8);word(b,4,count<<16);for(unsigned i=0;i<count;++i)word(b,5+i,i);return b;}
static Result<void> sentence(void*,std::span<const char>)noexcept{return {};}
int main(){
 for(unsigned n:{0u,1u,1024u}){auto wire=association(n);auto p=decode_packet(wire);assert(p);auto v=p->fields[0].associations();assert(v&&v->vector.size()==n);if(n)assert(*v->vector.at(n-1)==n-1);}
 // Largest single-vector list fitting a65535-word Context remains structurally bounded.
 auto max_wire=association(65530);auto default_max=decode_packet(max_wire);assert(!default_max&&default_max.error().code==ErrorCode::resource_limit);DecodeOptions max_limits;max_limits.limits={65530,65532};auto full_max=decode_packet(max_wire,max_limits);assert(full_max&&*full_max->fields[0].associations()->vector.at(65529)==65529);
 auto too_many=association(1025);unsigned calls=0;auto fail=decode_and_visit(too_many,{},[&](FieldView)noexcept->Result<void>{++calls;return {};});assert(!fail&&fail.error().code==ErrorCode::resource_limit&&calls==0);
 DecodeOptions expanded;expanded.limits.association_entries=1025;auto big=decode_packet(too_many,expanded);assert(big);NativeContextPacket<> materialized;assert(!big->fields[0].materialize_into(materialized));
 // Tags are included in the same1024-entry budget, not a separate allowance.
 auto tagged=association(1024);word(tagged,4,0x00008200);assert(decode_packet(tagged));auto extra=association(1026);word(extra,4,0x00008201);auto tagged_fail=decode_packet(extra);assert(!tagged_fail&&tagged_fail.error().code==ErrorCode::resource_limit);
 for(unsigned n:{4092u,4093u,4094u}){std::vector<char> text(n,'X');NativeContextPacket<> native;assert(native.set<GPSASCII>(GpsAsciiInput{1,text,{nullptr,sentence}}));auto snap=native.freeze();std::vector<std::byte> out(5000);Envelope e;e.type=PacketType::context;e.stream_id=1;auto encoded=encode_packet(e,snap,out);if(n==4092){assert(encoded&&decode_packet(Bytes{out}.first(*encoded)));}else{assert(!encoded&&encoded.error().code==ErrorCode::resource_limit);}
  const auto padded=(n+3)&~3u;std::vector<std::byte> wire(20+padded);word(wire,0,0x40000000u|static_cast<unsigned>(wire.size()/4));word(wire,1,1);word(wire,2,1u<<9);word(wire,3,1);word(wire,4,padded/4);for(unsigned i=0;i<n;++i)wire[20+i]=std::byte{'X'};auto decoded=decode_packet(wire);assert(bool(decoded)==(n==4092));if(n!=4092){assert(decoded.error().code==ErrorCode::resource_limit);DecodeOptions more;more.limits.work_units=4098;auto accepted=decode_packet(wire,more);assert(accepted&&accepted->fields[0].gps_ascii()->text.size()==n);}
 }
}
