#include <vita/adapters/loopback/loopback.hpp>
#include <cstdlib>
using namespace vita;using namespace vita::runtime;using namespace vita::memory;using namespace vita::adapters::loopback;
struct Backing{alignas(64)std::array<std::byte,256> bytes{};};
static ExternalPool pool(){auto b=std::make_shared<Backing>();BufferSpec s{b,b->bytes.data(),64,4,64,MemoryDomain::cpu};auto p=ExternalPool::create(std::span{&s,1});if(!p)std::abort();return std::move(*p);}
static TxSubmission submission(ExternalPool&p,CompletionArena<4>&tickets,unsigned count,unsigned id,AdmissionBundle credit){auto l=p.acquire({64,64});if(!l)std::abort();codec::Envelope e;e.type=codec::PacketType::signal;e.stream_id=1;e.packet_count=count;std::array<std::byte,4> payload{};auto n=codec::encode_envelope(e,payload,std::nullopt,*l->writable_bytes());if(!n||!l->set_size(*n))std::abort();TxStorage tx;if(!tx.append(std::move(*l),0,*n))std::abort();auto token=tickets.reserve(id);if(!token)std::abort();return{std::move(tx),std::move(*token),{5,1},{9,1,codec::PacketType::signal},{},std::move(credit)};}
static void receive(void*p,const codec::PacketView&,const RxEnvelope&)noexcept{++*static_cast<unsigned*>(p);}
int main(){auto tx=pool(),data=pool(),control=pool(),cancel=pool();AdmissionRequest cap;cap.need(Resource::completion,1).need(Resource::data_queue,4);AdmissionPool own(cap),foreign(cap);CompletionArena<4> tickets;RouteRegistry<4> routes;CounterRegistry<4> counters;unsigned calls=0;if(!routes.add({RouteKey{{5,1},1,codec::PacketType::signal},&calls,receive})||!counters.add({9,1,codec::PacketType::signal}))return 1;routes.freeze();counters.freeze();Loopback<4,4,4> transport(data,control,cancel,own,routes,counters);
 {
  auto credit=foreign.acquire(AdmissionRequest{}.need(Resource::completion));if(!credit)return 2;auto rejected=transport.try_send(submission(tx,tickets,0,1,std::move(*credit)));if(rejected||rejected.error().error.code!=ErrorCode::invalid_argument||foreign.used(Resource::completion)!=1||own.used(Resource::completion)||rejected.error().submission.storage.segment_count()!=1||!rejected.error().submission.completion.is_reserved()||*counters.next({9,1,codec::PacketType::signal})!=0)return 3;
 }
 tickets.scan([](CompletionRecord)noexcept{});if(foreign.used(Resource::completion))return 4;
 {
  auto credit=own.acquire(AdmissionRequest{}.need(Resource::completion));if(!credit||!own.owns(*credit)||foreign.owns(*credit))return 5;auto request=submission(tx,tickets,0,2,std::move(*credit));request.fault.synchronous_reject=true;auto rejected=transport.try_send(std::move(request));if(rejected||own.used(Resource::completion)!=1||!own.owns(rejected.error().submission.completion_credit))return 6;
  rejected.error().submission.fault={};auto accepted=transport.try_send(std::move(rejected.error().submission));if(!accepted||own.used(Resource::completion)!=1||own.used(Resource::data_queue)!=1||!transport.progress(*accepted)||calls!=1)return 7;
  if(own.used(Resource::completion)||own.used(Resource::data_queue)||!tickets.consume(0))return 8;
 }
 {
  auto credit=own.acquire(AdmissionRequest{}.need(Resource::completion).need(Resource::data_queue));if(!credit)return 9;auto rejected=transport.try_send(submission(tx,tickets,1,3,std::move(*credit)));if(rejected||rejected.error().error.code!=ErrorCode::invalid_argument||own.used(Resource::data_queue)!=1)return 10;
 }
 tickets.scan([](CompletionRecord)noexcept{});if(own.used(Resource::completion)||own.used(Resource::data_queue))return 11;
 return 0;}
