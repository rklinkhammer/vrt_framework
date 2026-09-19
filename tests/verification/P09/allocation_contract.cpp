#include <vita/runtime/context/receiver.hpp>
#include <cstdlib>
#include <new>
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::context;
static std::size_t allocations=0;
void* operator new(std::size_t n){++allocations;if(void*p=std::malloc(n?n:1))return p;std::abort();}
void* operator new[](std::size_t n){return ::operator new(n);}
void* operator new(std::size_t n,std::align_val_t a){++allocations;void*p=nullptr;if(!posix_memalign(&p,std::size_t(a),n?n:1))return p;std::abort();}
void* operator new[](std::size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}
struct Consumer{ReceiverHistory<> history;timing::MonoTime now{};unsigned frames=0;
 static Result<void> send(void*p,const ContextFrame&f)noexcept{auto&c=*static_cast<Consumer*>(p);codec::Envelope envelope;envelope.type=codec::PacketType::context;envelope.stream_id=1;std::array<std::byte,256> bytes;auto n=encode_context(f,envelope,bytes);if(!n)return std::unexpected(n.error());auto decoded=codec::decode_packet(Bytes{bytes}.first(*n));if(!decoded)return std::unexpected(decoded.error());++c.frames;return c.history.receive(*decoded,1,c.now);}
 static Result<void> data(void*,memory::TxStorage&tx,const RevisionHandle&)noexcept{tx={};return {};}};
int main(){AdmissionPool pool(AdmissionPool::reference_capacities());RevisionStore<4> store;Consumer consumer;ContextPublisher<4> publisher(store,{&consumer,Consumer::send,Consumer::data});auto sink=store.binding();EffectiveEvent e;e.association_generation=1;e.time_known=e.ordinal_known=true;e.actual_time={100,0};for(auto id:baseline_fields)e.state.fields[field_index(id)].validity=Validity::known;e.state.fields[1].value=*Hertz::from_integer(1);e.state.fields[2].value=std::uint32_t{valid_data_enable|valid_data_indicator};e.state.fields[3].value=PayloadFormat{0x200003cf00000000ULL};
 auto initial_credit=pool.acquire(AdmissionRequest{}.need(Resource::revision).need(Resource::context_publication));auto first=store.initial(e,std::move(*initial_credit));if(!first||!publisher.start({0})||!publisher.progress({0},e.actual_time,codec::Tsi::gps,true))return 1;first->operator=(RevisionHandle{});
 const auto before=allocations;
 for(unsigned n=1;n<=1000;++n){auto reserved=store.reserve(1);auto credit=pool.acquire(AdmissionRequest{}.need(Resource::revision).need(Resource::context_publication));if(!reserved||!credit)return 2;e.actual_time.picoseconds=n*1000000ULL;e.sample_ordinal=n;e.state.fields[1].value=*Hertz::from_integer(n+1);sink.record(sink.context,e,*reserved,std::move(*credit));reserved->reset();consumer.now={n*1000ULL};if(!publisher.progress(consumer.now,e.actual_time,codec::Tsi::gps,true))return 3;auto metadata=consumer.history.resolve(e.actual_time,consumer.now);if(metadata.confidence!=Confidence::known||std::get<Hertz>(metadata.state.fields[1].value)!=*Hertz::from_integer(n+1))return 4;}
 if(consumer.frames!=1001||pool.used(Resource::revision)!=1||store.occupied()!=1)return 5;
 return allocations==before?0:6;}
