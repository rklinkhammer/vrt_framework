#include <vita/runtime/context/receiver.hpp>
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::context;
static std::uint32_t word(Bytes b,unsigned offset){return (std::uint32_t(b[offset])<<24)|(std::uint32_t(b[offset+1])<<16)|(std::uint32_t(b[offset+2])<<8)|std::uint32_t(b[offset+3]);}
int main(){
 ContextFrame f;f.time={100,123};f.time_known=true;f.epoch=codec::Tsi::gps;f.valid=true;f.change=true;for(auto id:baseline_fields)f.state.fields[field_index(id)].validity=Validity::known;f.state.fields[0].value=std::uint32_t{7};f.state.fields[1].value=*Hertz::from_integer(1000000);f.state.fields[2].value=std::uint32_t{valid_data_enable|valid_data_indicator|sample_loss_enable|sample_loss_indicator};f.state.fields[3].value=PayloadFormat{0x200003cf00000000ULL};
 codec::Envelope envelope;envelope.type=codec::PacketType::context;envelope.stream_id=1;std::array<std::byte,256> bytes;
 auto n=encode_context(f,envelope,bytes);if(!n||*n!=48)return 1;
 if(word(bytes,0)!=0x40a0000c||word(bytes,4)!=1||word(bytes,8)!=100||word(bytes,12)!=0||word(bytes,16)!=123)return 2;
 if(word(bytes,20)!=(0x80000000u|(1u<<30)|(1u<<21)|(1u<<16)|(1u<<15)))return 3;
 auto decoded=codec::decode_packet(Bytes{bytes}.first(*n));if(!decoded||decoded->fields.size()!=4)return 4;
 ReceiverHistory<> history(1,codec::Tsi::gps,7);if(!history.receive(*decoded,1,{0}))return 5;auto observed=history.resolve({100,123},{0});if(observed.confidence!=Confidence::known||!observed.valid_data||observed.events!=(sample_loss_enable|sample_loss_indicator))return 6;
 // Wire framing preserves a signed raw rate, but Context association must validate it.
 auto malformed=bytes;for(unsigned i=28;i<36;++i)malformed[i]=std::byte{0xff};auto negative=codec::decode_packet(Bytes{malformed}.first(*n));if(!negative)return 15;
 ReceiverHistory<> rejected;if(rejected.receive(*negative,1,{0})||rejected.size())return 16;
 f.refresh=true;f.change=false;auto refresh=encode_context(f,envelope,bytes);if(!refresh||word(bytes,20)&0x80000000)return 7;decoded=codec::decode_packet(Bytes{bytes}.first(*refresh));if(!decoded)return 8;
 for(unsigned i=0;i<decoded->fields.size();++i)if(decoded->fields[i].id==StateEvent::id&&std::get<std::uint32_t>(*decoded->fields[i].value())!=(valid_data_enable|valid_data_indicator))return 9;
 f.valid=false;f.refresh=false;f.state.fields[1].validity=Validity::unknown;f.time.picoseconds=124;auto invalid=encode_context(f,envelope,bytes);if(!invalid)return 10;decoded=codec::decode_packet(Bytes{bytes}.first(*invalid));if(!decoded||decoded->fields.size()!=3)return 11;
 for(unsigned i=0;i<decoded->fields.size();++i){const auto& field=decoded->fields[i];if(field.id==SampleRate::id)return 12;if(field.id==StateEvent::id&&(std::get<std::uint32_t>(*field.value())&valid_data_indicator))return 13;}
 if(!history.receive(*decoded,1,{1})||history.resolve({100,124},{1}).confidence==Confidence::known)return 14;
 return 0;
}
