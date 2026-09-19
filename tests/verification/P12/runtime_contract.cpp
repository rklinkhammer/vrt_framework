#include "fixture.hpp"
#include <vita/adapters/posix_udp/factory.hpp>
#include <chrono>
#include <thread>
using namespace verify_p12;
static std::uint32_t word(Bytes bytes,std::size_t offset){return(std::uint32_t(bytes[offset])<<24)|(std::uint32_t(bytes[offset+1])<<16)|(std::uint32_t(bytes[offset+2])<<8)|std::uint32_t(bytes[offset+3]);}
int main(){for(bool v6:{false,true}){Peer external_controller(v6),external_device(v6);FactoryConfig<32,16> setup;setup.config=config(v6);PeerBinding c;c.local_source={1,1};c.remote_source={2,1};c.remote.fill(Address::loopback(v6?Family::ipv6:Family::ipv4,external_device.port()));PeerBinding d;d.local_source={2,1};d.remote_source={1,1};d.remote.fill(Address::loopback(v6?Family::ipv6:Family::ipv4,external_controller.port()));if(!setup.peers.push_back(c)||!setup.peers.push_back(d))return 1;
 auto configured=verify_p10::runtime_config();configured.transport=factory(setup);using Runtime=VitaRuntime<1,4,32,65536>;auto made=Runtime::create(configured,verify_p10::external_pools());if(!made||!setup.instance)return 2;auto& runtime=**made;auto stream=verify_p10::stream_config();stream.ipv6=v6;auto device=runtime.add_controllee(stream);if(!device||!runtime.observe_pps({0},{1000,0})||!device->start()||!runtime.progress({0})||!device->pause())return 3;
 // Independent native Controller constructs a literal query. It does not use
 // the production encoder, UDP adapter or command wrappers.
 std::array<std::uint32_t,9> words{0x68000009,1,0x0000a1b2,0x00010020,0xa0040000,42,3,2,0x00200000};std::array<std::byte,36> query{};for(unsigned n=0;n<9;++n)for(unsigned k=0;k<4;++k)query[4*n+k]=std::byte(words[n]>>(24-8*k));
 if(!external_controller.send_to(query,v6,setup.instance->local_address(Lane::control).port))return 4;
 bool context=false,data=false,state=false;std::array<std::byte,2048> received{};std::uint64_t now=1;auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(200);
 while((!state||!context||!data)&&std::chrono::steady_clock::now()<deadline){if(!runtime.progress({now++}))return 5;auto count=external_controller.receive(received,0);if(count<0){std::this_thread::yield();continue;}Bytes bytes{received.data(),static_cast<std::size_t>(count)};auto h=word(bytes,0);if((h>>28)==4)context=true;if((h>>28)==1){data=true;if(count!=1052||word(bytes,28)!=0x40000000)return 6;}if((h>>28)==6&&(h&0x04000000)){if(count!=56||h!=0x6ca0000e||word(bytes,32)!=42||word(bytes,36)!=3||word(bytes,40)!=2||word(bytes,44)!=0x00200000||word(bytes,48)!=0x000000f4||word(bytes,52)!=0x24000000)return 7;state=true;}}
 if(!context||!data||!state)return 8;
 made->reset();if(setup.instance->open()||setup.instance->outstanding())return 9;auto progress=setup.instance->progress_next();if(!progress||*progress)return 10;setup.instance.reset();
 }return 0;}
