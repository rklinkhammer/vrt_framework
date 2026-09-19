#include <vita/codec/packet.hpp>
#include <cassert>
using namespace vita;using namespace vita::codec;
static auto packet(std::uint32_t flags,std::uint32_t epoch){std::array<std::uint32_t,6>w{0x40000006,1,8,0x80000000,flags,epoch};std::array<std::byte,24>b{};for(unsigned i=0;i<6;++i)for(unsigned j=0;j<4;++j)b[4*i+j]=std::byte((w[i]>>(24-j*8))&255);return b;}
int main(){// The oracle is the explicit epoch tables, not the production validator.
 for(unsigned mask:{0u,1u,2u,4u,6u,8u,10u,12u,14u})for(unsigned code=0;code<4;++code)for(std::uint32_t epoch:{0u,1u,315964800u,315964811u,UINT32_MAX}){
 const bool utc=mask&2,gps=mask&4;bool expected=true;if(utc&&gps)expected=code==0;else if(utc)expected=code==0||((code==1||code==3)&&epoch==0);else if(gps)expected=code==0||(code==1&&epoch==315964811)||(code==2&&epoch==0)||(code==3&&epoch==315964800);
 TimestampDetailsValue value{code<<16,epoch};auto result=validate_timestamp_details(value,TimestampDetailsScope{static_cast<std::uint8_t>(mask),true,0,false});assert(bool(result)==expected);if(result)assert(*result==((mask&14)?TimestampScopeStatus::complete:TimestampScopeStatus::not_applicable));auto incomplete=validate_timestamp_details(value,TimestampDetailsScope{static_cast<std::uint8_t>(mask),false,0,false});assert(bool(incomplete)==expected);if(incomplete)assert(*incomplete==TimestampScopeStatus::incomplete);
 }
 for(unsigned lsh=0;lsh<4;++lsh)for(unsigned lsp=0;lsp<4;++lsp){TimestampDetailsValue v{(lsh<<14)|(lsp<<12),0};assert(bool(validate_timestamp_details_intrinsic(v))==(lsh!=0||lsp==0||lsp==2));}
 for(unsigned source=0;source<8;++source){TimestampDetailsValue v{(source<<9)|0x80,0xffffffff};auto w=packet(v.flags,v.epoch);auto parsed=decode_packet(w);assert(parsed&&*parsed->fields[0].value()==SemanticValue{v});ContextPacket b;assert(b.set<TimestampDetails>(v));Envelope e;e.type=PacketType::context;e.stream_id=1;std::array<std::byte,24>out{};assert(encode_packet(e,b.freeze(),out)&&out==w);auto s=validate_timestamp_details(v,{8,true,0,false});assert(s&&*s==(source>=6?TimestampScopeStatus::incomplete:TimestampScopeStatus::complete));assert(*validate_timestamp_details(v,{8,true,0,true})==TimestampScopeStatus::complete);}
 for(std::uint8_t tsfmask:{std::uint8_t{2},std::uint8_t{4},std::uint8_t{8}}){auto fractional=validate_timestamp_details(TimestampDetailsValue{0,UINT32_MAX},TimestampDetailsScope{1,true,0,false,tsfmask});assert(fractional&&*fractional==TimestampScopeStatus::complete);}
 TimestampDetailsValue user{0xa5000000,0};assert(*validate_timestamp_details(user,{8,true,0,false})==TimestampScopeStatus::incomplete);assert(*validate_timestamp_details(user,{8,true,0xa5,false})==TimestampScopeStatus::complete);
 for(unsigned r=19;r<24;++r){auto w=packet(1u<<r,0);unsigned callbacks=0;auto parsed=decode_and_visit(w,{},[&](FieldView)noexcept->Result<void>{++callbacks;return {};});assert(!parsed&&callbacks==0);assert(!make_timestamp_details(1u<<r,0));}
 // Intrinsic semantic error must remain inspectable after structural decode.
 auto raw=packet(0x1000,0);auto p=decode_packet(raw);assert(p&&std::get<TimestampDetailsValue>(*p->fields[0].value()).flags==0x1000);ContextPacket b;assert(!p->fields[0].materialize_into(b));
}
