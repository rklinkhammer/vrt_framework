#include <vita/runtime/transport/framing.hpp>
#include "../P10/runtime_fixture.hpp"
#include "peer.hpp"
using namespace vita;using namespace vita::runtime::transport;using namespace verify_p10;
static memory::TxStorage storage(memory::ExternalPool& pool,Bytes bytes,std::size_t a,std::size_t b){memory::TxStorage tx;std::array<std::size_t,4> ends{0,a,b,bytes.size()};for(unsigned i=0;i<3;++i){const auto length=ends[i+1]-ends[i];if(!length)continue;auto lease=pool.acquire({length});if(!lease||!lease->set_size(length))std::abort();auto out=lease->writable_bytes();if(!out)std::abort();std::memcpy(out->data(),bytes.data()+ends[i],length);if(!tx.append(std::move(*lease),0,length))std::abort();}return tx;}
int main(){auto pool=external_pool(128,3);
 for(std::size_t a=0;a<=16;++a)for(std::size_t b=a;b<=16;++b){auto tx=storage(pool,verify_p12::signal,a,b);auto decoded=inspect(tx);if(!decoded||decoded->envelope.stream_id!=0x01020304||decoded->payload_offset!=8||decoded->payload_bytes!=8||decoded->packet_bytes!=16)return 1;}
 std::array<std::byte,72> command{};std::array<std::uint32_t,18> words{0x68a00012,0x01020304,0x0000002a,0,1000,0,0,0xf9000000,42,1,2,3,4,5,6,7,8,0};for(unsigned n=0;n<words.size();++n)for(unsigned k=0;k<4;++k)command[n*4+k]=std::byte(words[n]>>(24-8*k));
 for(std::size_t a=1;a<72;++a){auto tx=storage(pool,command,a,a);auto decoded=inspect(tx);if(!decoded||decoded->payload_offset!=68||decoded->payload_bytes!=4||!decoded->envelope.command||decoded->envelope.command->controllee.words[3]!=4||decoded->envelope.command->controller.words[3]!=8)return 2;}
 for(unsigned malformed=0;malformed<4;++malformed){auto bytes=verify_p12::signal;if(malformed==0)bytes[0]=std::byte{0xf0};if(malformed==1)bytes[3]=std::byte{3};if(malformed==2)bytes[3]=std::byte{5};if(malformed==3)bytes[0]=std::byte{0x1c};auto tx=storage(pool,bytes,3,9);if(inspect(tx))return 3;}
 return 0;}
