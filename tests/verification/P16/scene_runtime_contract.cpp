#include <vita/profiles/iq/frequency_scan.hpp>
#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
#include <cassert>
#include <cstring>
#ifdef P16_ALLOCATION_GUARD
#include "../../../bench/allocation.hpp"
#include <cstdlib>
#include <new>
#endif
using namespace vita;namespace iq=vita::profiles::iq;
struct Scene {
 iq::VirtualRfScene source=*iq::VirtualRfScene::create();
 struct Event{std::uint64_t ordinal,center;};std::array<Event,16> events{};unsigned event_count=0,productions=0;
 static Result<void> effect(void* p,const runtime::EffectiveEvent& e)noexcept{auto& s=*static_cast<Scene*>(p);assert(!e.outcome.simulated&&e.ordinal_known&&s.event_count<s.events.size());auto center=std::get<Hertz>(e.state.fields[4].value).q20/(1ll<<20);s.events[s.event_count++]={e.sample_ordinal,static_cast<std::uint64_t>(center)};return s.source.effective(e);}
 std::uint64_t phase(std::uint64_t ordinal)const {std::int64_t total=0;for(unsigned i=0;i<event_count;++i){if(events[i].ordinal>ordinal)break;auto end=i+1<event_count?std::min(ordinal,events[i+1].ordinal):ordinal;auto delta=static_cast<std::int64_t>(100050000)-static_cast<std::int64_t>(events[i].center);total=(total+static_cast<std::int64_t>((end-events[i].ordinal)%100000)*delta)%100000;}return static_cast<std::uint64_t>((total+100000)%100000);}
 static Result<void> produce(void* p,iq::SampleWriteWindow& window)noexcept{auto& s=*static_cast<Scene*>(p);auto result=s.source.produce(window);assert(result);assert(s.source.phase_numerator()==s.phase(window.first_ordinal()+window.count()));++s.productions;return result;}
};
struct Retained {memory::RetentionQuota app{2},global{2};std::optional<memory::RetainedRx> payload;runtime::StateSnapshot metadata;std::array<std::byte,64> bytes{};std::size_t size=0;static void receive(void*p,const runtime::context::BorrowedSignalRx& rx)noexcept{auto& self=*static_cast<Retained*>(p);if(self.payload)return;assert(rx.metadata.confidence==runtime::context::Confidence::known);auto retained=rx.retain(self.app,self.global);assert(retained);self.payload.emplace(std::move(*retained));self.metadata=rx.metadata.state;auto wire=self.payload->fragment(0);assert(wire&&wire->size()<=self.bytes.size());self.size=wire->size();std::memcpy(self.bytes.data(),wire->data(),self.size);}void verify()const{assert(payload&&metadata.fields[4].value==SemanticValue{*Hertz::from_integer(100025000)});auto wire=payload->fragment(0);assert(wire&&wire->size()==size&&std::memcmp(wire->data(),bytes.data(),size)==0);}};
int main(){
#ifdef P16_ALLOCATION_GUARD
 using namespace vita::bench;critical_thread=true;auto* volatile malloc_call=&std::malloc;void* probe=malloc_call(23);critical_thread=false;assert(probe&&critical_c_allocations>0);std::free(probe);critical_allocations=0;critical_thread=true;probe=::operator new(64,std::align_val_t{64});critical_thread=false;assert(critical_allocations>0);::operator delete(probe,std::align_val_t{64});critical_c_allocations=0;critical_allocations=0;
#endif
 auto config=iq::lab::config(0xabcdef);auto pools=iq::lab::pools();assert(config&&pools);auto payload=pools->payload;auto made=VitaRuntime<1,4,32,65536>::create(*config,std::move(*pools));assert(made);Scene scene;Retained retained;using Backend=runtime::transaction::VirtualBackend<4>;auto backend=std::make_shared<Backend>();StreamConfig stream;stream.sid=1;stream.controller_id=2;stream.controllee_id=3;stream.profile=iq::Profile::frequency_tunable;stream.sample_rate=100000;stream.center_frequency=100025000;stream.ip_mtu=100;stream.source={&scene,Scene::produce,Scene::effect};stream.receiver={&retained,Retained::receive,nullptr};stream.device={backend->binding(),backend,sizeof(Backend),[](void* p,const runtime::transaction::OperationContext&)noexcept{auto& b=*static_cast<Backend*>(p);if(b.pending())b.complete_next();}};auto device=(*made)->add_controllee(stream);assert(device);auto controller=(*made)->add_controller(*device);assert(controller);
 std::array<memory::BufferLease,128> held{};unsigned held_count=0;
#ifdef P16_ALLOCATION_GUARD
 critical_thread=true;
#endif
 assert((*made)->observe_pps({0},{1000,0})&&device->start()&&(*made)->progress({0}));assert(scene.event_count==1&&scene.productions==1);
 std::array<TransactionHandle,3> tunes;constexpr std::array<std::uint64_t,3> frequencies{100040000,100065000,100050000};for(unsigned i=0;i<3;++i){CommandOptions options;options.timing_mode=1;options.execute_at=runtime::timing::ProtocolTime{1000,(i+1)*2'200'000'000ull};auto t=controller->set_center_frequency(*Hertz::from_integer(frequencies[i]),options);assert(t);tunes[i]=*t;}assert((*made)->progress({0}));
 while(held_count<held.size()){auto lease=payload.acquire({1,1,memory::MemoryDomain::cpu,true});if(!lease)break;held[held_count++]=std::move(*lease);}assert(held_count>0&&held_count<held.size());const auto prior=scene.productions;
 for(unsigned i=0;i<3;++i){auto advanced=(*made)->progress({(i+1)*2'200'000ull});assert(advanced||advanced.error().retryable);for(unsigned round=0;round<4&&scene.event_count<i+2;++round){auto retry=(*made)->progress({(i+1)*2'200'000ull});assert(retry||retry.error().retryable);}assert(scene.productions==prior&&scene.event_count==i+2);assert(scene.events[i+1].ordinal==(i+1)*220&&scene.events[i+1].center==frequencies[i]);bool confirmed=false;for(unsigned round=0;round<4;++round){auto evidence=controller->wait(tunes[i],0);if(evidence&&evidence->observation.confirms_execution){confirmed=true;break;}auto retry=(*made)->progress({(i+1)*2'200'000ull});assert(retry||retry.error().retryable);}assert(confirmed);}
 for(auto& lease:held)lease.reset();assert((*made)->progress({6'710'000}));assert(scene.productions>prior&&!scene.source.faulted());const auto real_events=scene.event_count;CommandOptions dry;dry.dry_run=true;auto simulation=controller->set_center_frequency(*Hertz::from_integer(100025000),dry);assert(simulation);assert((*made)->run_for(1'000'000));assert(scene.event_count==real_events);retained.verify();
#ifdef P16_ALLOCATION_GUARD
 critical_thread=false;assert(critical_allocations==0&&critical_c_allocations==0);
#endif
 made->reset();retained.verify();retained.payload.reset();assert(retained.app.active()==0&&retained.global.active()==0);
}
