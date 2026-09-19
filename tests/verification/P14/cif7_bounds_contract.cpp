#include <vita/codec/packet.hpp>
#include <cassert>
#include <vector>
using namespace vita;using namespace vita::codec;
static void put(std::vector<std::byte>&b,std::uint32_t w){for(int i=3;i>=0;--i)b.push_back(std::byte((w>>(8*i))&255));}
static auto packet(bool extra){std::uint32_t cif1=0;for(unsigned b:{31u,30u,29u,27u,26u,25u,24u,20u,19u,18u,17u,16u,15u,14u,13u,6u,5u,4u,3u,2u,1u})cif1|=1u<<b;std::uint32_t cif3=0;for(unsigned b:{31u,30u,27u,26u,25u,24u,23u,22u,21u,20u,17u,16u,7u,6u})cif3|=1u<<b;std::vector<std::byte>raw;for(auto w:{0x64000000u|(142u+extra),1u,0xa90b0400u,9u,2u,3u})put(raw,w);for(auto w:{14u,cif1,0xfffffff8u,cif3|(extra?32u:0u),14u,cif1,0xfffffff8u,cif3})put(raw,w);for(unsigned i=0;i<128+extra;++i)put(raw,i<64+extra?0x80000000:0x40000000);return raw;}
int main(){DecodeOptions options{RequestContext{0xa90b0000}};auto exact=packet(false);auto parsed=decode_packet_bounded<128,1664>(exact,options);assert(parsed&&parsed->fields.size()==128);for(unsigned i=0;i<128;++i){assert(parsed->fields[i].group==(i<64?DiagnosticGroup::warning:DiagnosticGroup::error));assert(*parsed->fields[i].diagnostic()==(i<64?0x80000000:0x40000000));}assert((decode_packet_bounded<128,128>(exact,options)));assert((!decode_packet_bounded<127,1664>(exact,options)));assert((!decode_packet_bounded<128,127>(exact,options)));unsigned callbacks=0;auto ordinary=decode_and_visit(exact,options,[&](FieldView)noexcept->Result<void>{++callbacks;return {};});assert(!ordinary&&!callbacks);
 auto over=packet(true);auto limited=decode_packet_bounded<128,1664>(over,options);assert(!limited&&(limited.error().code==ErrorCode::resource_limit||limited.error().code==ErrorCode::capacity_exhausted));auto enlarged=decode_packet_bounded<129,129>(over,options);assert(enlarged&&enlarged->fields.size()==129);options.limits.work_units=127;assert((!decode_packet_bounded<128,1664>(exact,options)));
 // Correlation absence remains opaque, not a guessed second group, even at large capacity.
 auto opaque=decode_packet_bounded<128,1664>(exact);assert(opaque&&opaque->opaque&&opaque->requires_request_context&&opaque->fields.empty());
}
