#include "fixture.hpp"
#include <vita/profiles/iq/lab.hpp>
#include <new>
using namespace verify_p12;
static std::size_t allocations=0;
void* operator new(std::size_t n){++allocations;if(auto*p=std::malloc(n?n:1))return p;std::abort();}
void* operator new[](std::size_t n){return ::operator new(n);}
void* operator new(std::size_t n,std::align_val_t a){++allocations;void*p=nullptr;if(!posix_memalign(&p,std::size_t(a),n?n:1))return p;std::abort();}
void* operator new[](std::size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}
int main(){Peer peer;AdmissionPool admission(AdmissionPool::reference_capacities());RouteRegistry<8> routes;CounterRegistry<8> counters;if(!counters.add({7,0x01020304,codec::PacketType::signal}))return 1;routes.freeze();counters.freeze();auto made=Udp<8,8,8>::create(config(),pools(),admission,routes,counters);if(!made||!(*made)->add_peer(binding(peer)))return 2;auto txpool=verify_p10::external_pool(64,4);CompletionArena<1> tickets;auto before=allocations;
 for(unsigned n=0;n<100;++n){auto bytes=verify_p12::signal;bytes[1]=std::byte(n%16);auto ticket=tickets.reserve(n+1);if(!ticket)return 3;auto token=(*made)->try_send({storage(txpool,bytes),std::move(*ticket),{7,1},{7,0x01020304,codec::PacketType::signal}});if(!token)return 4;(*made)->begin_cycle();for(unsigned j=0;j<6&&(*made)->outstanding(*token);++j)if(!(*made)->progress_next())return 5;auto completed=tickets.consume(0);if(!completed||completed->result.status!=CompletionStatus::succeeded)return 6;std::array<std::byte,64> received{};if(peer.receive(received)!=16)return 7;}
 if(allocations!=before)return 8;return 0;}
