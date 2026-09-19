#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
#include <cassert>
using namespace vita;
namespace tx=vita::runtime::transaction;
using Runtime=VitaRuntime<2,4,32,65536>;
using Device=tx::VirtualBackend<4>;
static StreamConfig configuration(){StreamConfig s;s.sid=1;s.controller_id=2;s.controllee_id=3;s.sample_rate=100000;s.profile=profiles::iq::Profile::frequency_tunable;return s;}
static std::unique_ptr<Runtime> make(){auto c=profiles::iq::lab::config(0xabcdef);auto p=profiles::iq::lab::pools();assert(c&&p);auto r=Runtime::create(*c,std::move(*p));assert(r);return std::move(*r);}
int main(){
 // The borrowed caller owner can disappear while an actual completion capability
 // keeps backend storage alive, including beyond Runtime destruction.
 auto r=make();auto backend=std::make_shared<Device>();std::weak_ptr<Device> weak=backend;auto s=configuration();s.device={backend->binding(),backend,sizeof(Device),nullptr};auto device=r->add_controllee(s);assert(device);auto controller=r->add_controller(*device);assert(controller);s.device={};backend.reset();assert(!weak.expired());assert(r->observe_pps({0},{1000,0})&&device->start());
 auto tune=controller->set_center_frequency(*Hertz::from_integer(100025000));assert(tune);
 for(unsigned i=0;i<20&&!weak.lock()->pending();++i)assert(r->progress({i*100000u}));
 auto owner=weak.lock();assert(owner&&owner->pending()==1);auto capability=owner->pending_capability();assert(capability);auto complete=owner->complete_next();assert(complete&&*complete);owner.reset();r.reset();assert(!weak.expired());
 runtime::FieldOutcome late;late.id=RFReferenceFrequency::id;late.status=runtime::FieldStatus::executed;late.value=*Hertz::from_integer(100025000);late.validity=runtime::Validity::known;assert(!capability->complete(late));capability.reset();assert(weak.expired());
 // Omitted simulation cannot become a fabricated successful dry-run.
 auto second=make();auto b=std::make_shared<Device>();auto spec=configuration();auto binding=b->binding();binding.simulate=nullptr;binding.disarm=nullptr;spec.device={binding,b,sizeof(Device),nullptr};auto d=second->add_controllee(spec);assert(d);auto c=second->add_controller(*d);assert(c);assert(second->observe_pps({0},{1000,0})&&d->start());CommandOptions dry;dry.dry_run=true;auto simulated=c->set_center_frequency(*Hertz::from_integer(100025000),dry);assert(simulated);assert(second->run_for(5'000'000));assert(b->begins()==0&&b->writes()==0);auto observed=c->observation(*simulated);assert(observed&&!observed->confirms_execution);assert(d->confirmed_state().fields[4].value==SemanticValue{*Hertz::from_integer(100000000)});
 // Required physical quiescence is not supplied by a missing disarm callback.
 b->set_quiescence_available(false);auto pending=c->set_center_frequency(*Hertz::from_integer(100025000));assert(pending);assert(second->run_for(5'000'000));assert(b->pending()==1);assert(d->shutdown(StopMode::immediate));auto elapsed=second->progress({2'110'000'000});assert(elapsed||elapsed.error().retryable);auto status=d->lifecycle();assert(!status.backend_quiescent&&status.phase!=LifecyclePhase::stopped);runtime::FieldOutcome aborted;aborted.id=RFReferenceFrequency::id;aborted.status=runtime::FieldStatus::cancelled;(void)b->complete_next(aborted);
 // A controller-only local handle never claims remote physical quiescence.
 auto remote_runtime=make();auto remote_spec=configuration();remote_spec.role=EndpointRole::controller_only;auto remote=remote_runtime->add_controllee(remote_spec);assert(remote);assert(remote->shutdown());assert(remote_runtime->progress({0}));assert(!remote->lifecycle().backend_quiescent);
}
