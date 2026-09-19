#include <vita/codec/packet.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <vector>
using namespace vita;using namespace vita::codec;
static void word(std::vector<std::byte>&b,std::uint32_t v){for(int i=3;i>=0;--i)b.push_back(std::byte(v>>(8*i)));}
static std::vector<std::byte> literal(unsigned tsi,unsigned tsf){
 unsigned t=(tsi?1:0)+(tsf?2:0);std::vector<std::byte>b;
 for(auto v:{0x40000000u|(tsi<<22)|(tsf<<20)|(16+2*t),1u})word(b,v);
 if(tsi)word(b,99);if(tsf){word(b,0);word(b,17);}
 for(auto v:{2u,1u<<9,12+t,0x00000001u|((9+t)<<12),0xc0f00000u,7u,0u,0x00100000u,0u,1000u})word(b,v);
 if(tsi)word(b,123);if(tsf){word(b,0x01234567);word(b,0x89abcdef);}
 // Dwell, Time3 and Time4 each occupy two words, independent of prologue timestamp.
 word(b,0xffffffff);word(b,0xfffffff9);word(b,0);word(b,13);return b;
}
int main(){for(unsigned tsi=0;tsi<4;++tsi)for(unsigned tsf=0;tsf<4;++tsf){
 auto raw=literal(tsi,tsf);assert(raw.size()==4*(16+2*((tsi?1:0)+(tsf?2:0))));auto parsed=decode_packet(raw);
 if(!tsi&&!tsf){assert(!parsed);continue;}assert(parsed&&parsed->fields.size()==1);
 NativeContextPacket<1024> builder;assert(builder.bind_timestamp_format(tsi,tsf));assert(parsed->fields[0].materialize_into(builder));
 auto frozen=builder.freeze();auto s=frozen.get<SectorStepScan>();assert(s&&s->records.size()==1);auto record=s->records.at(0);assert(record&&record->sector==7&&record->dwell_fs==1000&&record->time3_fs==-7&&record->time4_fs==13);assert(record->start==(SectorStartTime{tsf?0x0123456789abcdefull:0,tsi?123u:0,static_cast<std::uint8_t>(tsi),static_cast<std::uint8_t>(tsf)}));
 Envelope envelope;envelope.type=PacketType::context;envelope.stream_id=1;envelope.timestamp={static_cast<Tsi>(tsi),static_cast<Tsf>(tsf),99,17};std::array<std::byte,256> encoded{};auto n=encode_packet(envelope,frozen,encoded);assert(n&&*n==raw.size()&&std::equal(raw.begin(),raw.end(),encoded.begin()));
 auto generation=frozen.generation();assert(!builder.bind_timestamp_format(tsi,(tsf+1)%4));assert(builder.freeze().generation()==generation);
 for(unsigned dimension=0;dimension<2;++dimension){auto altered=envelope;if(dimension)altered.timestamp.tsi=static_cast<Tsi>((tsi+1)%4);else altered.timestamp.tsf=static_cast<Tsf>((tsf+1)%4);encoded.fill(std::byte{0x5a});assert(!encode_packet(altered,frozen,encoded));assert(std::all_of(encoded.begin(),encoded.end(),[](auto b){return b==std::byte{0x5a};}));}
 for(std::size_t size=0;size<raw.size();++size){unsigned callbacks=0;assert(!decode_and_visit(Bytes{raw}.first(size),{},[&](FieldView)noexcept->Result<void>{++callbacks;return {};}));assert(callbacks==0);}
 }
 // Equal-size timestamp-code changes cannot validate an old measured layout.
 NativeContextPacket<1024> a,b;assert(a.bind_timestamp_format(1,1)&&b.bind_timestamp_format(1,2));SectorRecord ra;ra.sector=1;ra.start=SectorStartTime{3,2,1,1};auto rb=ra;rb.start->tsf=2;
 assert(a.set<SectorStepScan>(SectorStepScanInput{0xc0400000,{1,1,true},std::span{&ra,1}}));assert(b.set<SectorStepScan>(SectorStepScanInput{0xc0400000,{1,2,true},std::span{&rb,1}}));auto sa=a.freeze(),sb=b.freeze();auto ma=measure(sa),mb=measure(sb);assert(ma&&mb&&ma->bytes==mb->bytes&&!validate_measure(sb,*ma));
}
