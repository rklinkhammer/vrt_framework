#include <vita/runtime/stream/routing.hpp>
#include <vita/runtime/stream/counters.hpp>
using namespace vita;using namespace vita::runtime;
static void ignore(void*,const codec::PacketView&,const memory::RxEnvelope&)noexcept{}
int main(){
    PeerSession peer{1,1};RouteRegistry<8> routes;
    RouteKey data{peer,42,codec::PacketType::signal},context{peer,42,codec::PacketType::context},command{peer,42,codec::PacketType::command};
    command.controllee=codec::Identifier::short_id(5);command.controller=codec::Identifier::short_id(6);
    if(!routes.add({data,nullptr,ignore}) || !routes.add({context,nullptr,ignore}) || !routes.add({command,nullptr,ignore}))return 1;
    RouteKey sidless{peer,std::nullopt,codec::PacketType::signal_without_sid};if(!routes.add({sidless,nullptr,ignore}) || routes.add({sidless,nullptr,ignore}))return 2;
    routes.freeze();codec::Envelope e;e.type=codec::PacketType::signal;e.stream_id=42;
    auto a=routes.lookup(peer,e);e.type=codec::PacketType::context;auto b=routes.lookup(peer,e);if(!a||!b||*a==*b)return 3;
    e.type=codec::PacketType::command;e.command=codec::Command{0xa0000000,0,codec::Identifier::short_id(5),codec::Identifier::short_id(6)};
    if(!routes.lookup(peer,e) || routes.lookup(PeerSession{1,2},e))return 4;
    e.command->controllee=codec::Identifier::uuid({5,0,0,0});if(routes.lookup(peer,e))return 5;
    if(routes.add({RouteKey{peer,99,codec::PacketType::signal},nullptr,ignore}))return 6;
    CounterRegistry<4> counters;CounterKey signal{7,42,codec::PacketType::signal},metadata{7,42,codec::PacketType::context},other{8,42,codec::PacketType::signal};
    if(!counters.add(signal)||!counters.add(metadata)||!counters.add(other))return 7;counters.freeze();
    if(counters.accept(signal,1)||*counters.next(signal)!=0)return 8;
    for(unsigned i=0;i<17;++i)if(!counters.accept(signal,i&15))return 9;
    if(*counters.next(signal)!=1||*counters.next(metadata)!=0||*counters.next(other)!=0)return 10;
    return 0;
}
