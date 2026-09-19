#include <vita/runtime/context/publisher.hpp>
#include <cstdlib>
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::context;using namespace vita::memory;
static EffectiveEvent event(unsigned rate=1){EffectiveEvent e;e.association_generation=1;e.actual_time={100,0};e.time_known=e.ordinal_known=true;for(auto& f:e.state.fields)f.validity=Validity::known;e.state.fields[1].value=*Hertz::from_integer(rate);e.state.fields[3].value=PayloadFormat{0x200003cf00000000ULL};return e;}
struct Transport{bool reject=false;unsigned contexts=0,data=0;std::array<ContextFrame,32> sent;std::optional<TxStorage> packet;RevisionHandle revision;
 static Result<void> context(void* p,const ContextFrame& f)noexcept{auto&t=*static_cast<Transport*>(p);if(t.reject)return std::unexpected(Error{ErrorCode::capacity_exhausted});if(t.contexts==32)return std::unexpected(Error{ErrorCode::capacity_exhausted});t.sent[t.contexts++]=f;return {};}
 static Result<void> send(void* p,TxStorage& storage,const RevisionHandle& r)noexcept{auto&t=*static_cast<Transport*>(p);if(!t.contexts)return std::unexpected(Error{ErrorCode::invalid_state});++t.data;t.packet.emplace(std::move(storage));t.revision=r;return {};}
 PublisherBinding binding(){return{this,context,send};}};
struct Backing{alignas(64)std::array<std::byte,8192> data{};};
static TxStorage packet(ExternalPool& p){TxStorage tx;auto l=p.acquire({64,64});if(!l)std::abort();auto output=l->writable_bytes();codec::Envelope envelope;envelope.type=codec::PacketType::signal;envelope.stream_id=1;envelope.timestamp={codec::Tsi::gps,codec::Tsf::picoseconds,100,0};const std::array payload{std::byte{0x12},std::byte{0x34},std::byte{0xab},std::byte{0xcd}};auto size=codec::encode_envelope(envelope,payload,std::nullopt,*output);if(!size||!l->set_size(*size)||!tx.append(std::move(*l),0,*size))std::abort();return tx;}
int main(){auto backing=std::make_shared<Backing>();BufferSpec spec{backing,backing->data.data(),64,128,64,MemoryDomain::cpu};auto pool=ExternalPool::create(std::span{&spec,1});if(!pool)return 1;
 {
  RevisionStore<4> store;auto first=store.initial(event(),{});Transport t;ContextPublisher<4> p(store,t.binding());if(!first||!p.start({0})||!p.submit(packet(*pool),*first,{0})||t.data)return 2;
  if(!p.progress({0},{100,0},codec::Tsi::gps,true)||t.contexts!=1||t.data!=1||!t.sent[0].valid)return 3;
  p.pause();if(!p.start({100})||!p.submit(packet(*pool),*first,{100})||t.data!=1)return 4;
  if(!p.progress({100},{100,100000},codec::Tsi::gps,true)||t.contexts!=2||t.data!=2||!t.sent[1].observation)return 5;
  t.reject=true;if(p.progress({1000000100},{101,0},codec::Tsi::gps,true))return 6;
  if(p.progress({1010000100},{101,10000000000ULL},codec::Tsi::gps,true)||p.status()!=StreamStatus::context_unavailable)return 7;
 }
 {
  RevisionStore<4> store;auto first=store.initial(event(),{});Transport t;t.reject=true;ContextPublisher<4> p(store,t.binding());if(!p.start({0})||!p.submit(packet(*pool),*first,{0}))return 8;
  if(p.progress({9999999},{100,0},codec::Tsi::gps,true)||p.status()!=StreamStatus::active||p.held()!=1)return 9;
  if(p.progress({10000000},{100,0},codec::Tsi::gps,true)||p.status()!=StreamStatus::context_unavailable||p.held()||t.data)return 10;
 }
 {
  RevisionStore<4> store;auto first=store.initial(event(),{});Transport t;ContextPublisher<4> p(store,t.binding());if(!p.start({0}))return 11;for(unsigned i=0;i<64;++i)if(!p.submit(packet(*pool),*first,{0}))return 11;if(p.held()!=64)return 23;
  if(p.submit(packet(*pool),*first,{0})||p.status()!=StreamStatus::context_unavailable||p.held()||t.data)return 12;
 }
 {
  RevisionStore<4> store;auto first=store.initial(event(),{});Transport t;ContextPublisher<4> p(store,t.binding());if(!p.start({0})||!p.progress({0},{100,0},codec::Tsi::gps,true))return 13;
  if(p.progress({1000000000},{99,0},codec::Tsi::gps,true)||p.status()!=StreamStatus::temporal_association||t.contexts!=1)return 14;
 }
 {
  RevisionStore<4> store;auto first=store.initial(event(),{});Transport t;ContextPublisher<4> p(store,t.binding());if(!p.start({0})||!p.submit(packet(*pool),*first,{0})||!p.progress({0},{200,0},codec::Tsi::gps,true))return 15;
  bool fresh=false;for(unsigned i=0;i<t.contexts;++i)fresh|=t.sent[i].time==timing::ProtocolTime{200,0}&&t.sent[i].observation;
  if(!fresh||t.data!=1)return 16;
 }
 {
  RevisionStore<4> store;auto first=store.initial(event(),{});Transport t;ContextPublisher<4> p(store,t.binding());if(!p.start({0})||!p.progress({0},{100,0},codec::Tsi::gps,true)||!p.submit(packet(*pool),*first,{0}))return 17;
  std::array<std::byte,24> immutable;auto original=t.packet->segment(0);if(!original||original->size()!=24)return 21;std::copy(original->begin(),original->end(),immutable.begin());
  auto reservation=store.reserve(1);if(!reservation)return 18;auto next=event(2);next.actual_time={100,1000};next.sample_ordinal=1;auto sink=store.binding();sink.record(sink.context,next,*reservation,{});
  if(!p.progress({1},{100,1000},codec::Tsi::gps,true)||!t.packet||t.packet->byte_size()!=24||t.revision.id()!=first->id()||std::get<Hertz>(t.revision.event().state.fields[1].value)!=*Hertz::from_integer(1))return 19;
  if(!std::equal(immutable.begin(),immutable.end(),t.packet->segment(0)->begin()))return 22;
  t.packet.reset();t.revision={};if(std::get<Hertz>(store.current()->event().state.fields[1].value)!=*Hertz::from_integer(2))return 20;
 }
 return 0;}
