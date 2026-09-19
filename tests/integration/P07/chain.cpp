#include "../../unit/P05/support.hpp"
#include "../../unit/P07/support.hpp"
using namespace vita::adapters::loopback;
struct Endpoint {
    TransactionManager<2,8,16384>* manager=nullptr;OperationContext now;
    std::array<RetentionAdmission,4> admissions{};std::size_t received=0,replies=0;
    ControllerObserver observer{7};
    static void command(void* p,const codec::PacketView& packet,const memory::RxEnvelope&) noexcept {
        auto& e=*static_cast<Endpoint*>(p);auto admitted=e.manager->accept(packet,e.now,{11,1});assert(admitted);e.admissions[e.received++]=*admitted;
    }
    static void response(void* p,const codec::PacketView& packet,const memory::RxEnvelope&) noexcept {
        auto& e=*static_cast<Endpoint*>(p);assert(e.observer.receive(packet));++e.replies;
    }
    static std::optional<codec::RequestContext> context(void*,const codec::Envelope& e) noexcept {return codec::RequestContext{e.cancel?0x090d0000u:0x091f0000u};}
};
int main(){
    AdmissionPool admission(AdmissionPool::reference_capacities());VirtualBackend<> backend;EngineOptions options;options.external_retention=true;
    Engine<2> engine(admission,backend.binding(),initial(),options);RetentionStore<8,16384> store(admission);TransactionManager<2,8,16384> manager(engine,store,admission);Endpoint endpoint;endpoint.manager=&manager;
    auto tx=p05_test::pool(),data=p05_test::pool(),control=p05_test::pool(),cancel_pool=p05_test::pool();
    RouteRegistry<8> routes;CounterRegistry<8> counters;CounterKey outgoing{11,1,codec::PacketType::command},incoming{22,1,codec::PacketType::command};
    assert(routes.add({{{11,1},1,codec::PacketType::command},&endpoint,Endpoint::command}));assert(routes.add({{{22,1},1,codec::PacketType::command},&endpoint,Endpoint::response,Endpoint::context}));routes.freeze();assert(counters.add(outgoing));assert(counters.add(incoming));counters.freeze();
    Loopback<8,8,8> transport(data,control,cancel_pool,admission,routes,counters);CompletionArena<16> tickets;std::size_t submitted=0;
    auto send=[&](Bytes wire,CounterKey key,PeerSession peer){auto lease=tx.acquire({wire.size()});assert(lease);auto bytes=lease->writable_bytes();assert(bytes);std::copy(wire.begin(),wire.end(),bytes->begin());assert(lease->set_size(wire.size()));memory::TxStorage storage;assert(storage.append(std::move(*lease),0,wire.size()));auto completion=tickets.reserve(++submitted);assert(completion);auto token=transport.try_send({std::move(storage),std::move(*completion),peer,key,{}});assert(token);assert(transport.progress(*token));assert(tickets.scan([](CompletionRecord result) noexcept {assert(result.result.status==CompletionStatus::succeeded);})==1);};
    auto ordinary=make();send(Bytes{ordinary.bytes}.first(ordinary.size),outgoing,{11,1});assert(manager.progress(endpoint.now));assert(backend.pending()==1);
    auto cancellation=make(true);cancellation.bytes[1]|=std::byte{1};send(Bytes{cancellation.bytes}.first(cancellation.size),outgoing,{11,1});assert(manager.progress(endpoint.now));assert(backend.pending()==0&&backend.writes()==0);
    auto transmit_responses=[&](RetentionToken token){for(std::size_t i=0;;++i){auto ack=manager.response(token,i);assert(ack);if(!*ack)break;std::array<std::byte,256> wire;auto count=counters.next(incoming);assert(count);auto bytes=encode_response(**ack,wire,*count);assert(bytes);send(Bytes{wire}.first(*bytes),incoming,{22,1});}};
    transmit_responses(endpoint.admissions[0].token);transmit_responses(endpoint.admissions[1].token);assert(endpoint.replies==5);
    cancellation.bytes[1]=std::byte{2};send(Bytes{cancellation.bytes}.first(cancellation.size),outgoing,{11,1});assert(endpoint.admissions[2].kind==DuplicateKind::replay);transmit_responses(endpoint.admissions[2].token);assert(endpoint.replies==7&&backend.writes()==0);
    assert(endpoint.observer.observation().cancellation&&!endpoint.observer.observation().confirms_execution);
    for(std::size_t i=0;i<endpoint.received;++i)assert(manager.release(endpoint.admissions[i].token));assert(tx.return_count()==submitted);
}
