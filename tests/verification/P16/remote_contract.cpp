#include <vita/adapters/posix_udp/factory.hpp>
#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
#include <cassert>
using namespace vita;using namespace vita::adapters::posix_udp;
using Runtime=VitaRuntime<1,4,32,65536>;
static void trial(Family family,bool matching){
 FactoryConfig<32,8> left,right;for(auto* x:{&left,&right}){x->config.capabilities.reserved_control_slots=8;x->config.capabilities.reserved_cancellation_slots=2;for(auto& lane:x->config.sockets)lane.bind=Address::loopback(family);}
 auto lc=profiles::iq::lab::config(0xabcdef),rc=lc;auto lp=profiles::iq::lab::pools(),rp=profiles::iq::lab::pools();assert(lc&&rc&&lp&&rp);lc->transport=factory(left);rc->transport=factory(right);auto client=Runtime::create(*lc,std::move(*lp)),server=Runtime::create(*rc,std::move(*rp));assert(client&&server);
 PeerBinding a,b;a.local_source={1,1};a.remote_source={2,1};b.local_source={2,1};b.remote_source={1,1};for(unsigned i=0;i<3;++i){a.remote[i]=right.instance->local_address(static_cast<Lane>(i));b.remote[i]=left.instance->local_address(static_cast<Lane>(i));}assert(left.instance->add_peer(a)&&right.instance->add_peer(b));
 auto backend=std::make_shared<runtime::transaction::VirtualBackend<4>>();StreamConfig source;source.sid=1;source.controller_id=2;source.controllee_id=3;source.sample_rate=100000;source.ipv6=family==Family::ipv6;source.profile=profiles::iq::Profile::frequency_tunable;source.role=EndpointRole::controllee_only;source.device={backend->binding(),backend,sizeof(*backend),[](void* p,const runtime::transaction::OperationContext&)noexcept {auto& device=*static_cast<runtime::transaction::VirtualBackend<4>*>(p);if(device.pending())device.complete_next();}};auto device=(*server)->add_controllee(source);assert(device&&!(*server)->add_controller(*device));assert((*server)->observe_pps({0},{1000,0})&&device->start());
 RemoteTargetConfig target;target.sid=1;target.controller_id=2;target.controllee_id=3;target.profile=matching?profiles::iq::Profile::frequency_tunable:profiles::iq::Profile::generator_v1;auto controller=(*client)->add_remote_controller(target);assert(controller);
 CommandOptions options;options.timeout_ns=30'000'000;auto request=matching?controller->set_center_frequency(*Hertz::from_integer(100025000),options):controller->set_sample_rate(*Hertz::from_integer(200000),options);assert(request);bool execution=false,readback=false;
 for(std::uint64_t now=0;now<40'000'000;now+=100'000){auto p=(*client)->progress({now});assert(p||p.error().retryable);auto q=(*server)->progress({now});assert(q||q.error().retryable);auto observation=controller->observation(*request);assert(observation);auto retained=controller->wait(*request,0);if(retained)execution|=retained->observation.confirms_execution;auto state=controller->state(*request);assert(state);if(*state){auto rf=(**state).value<RFReferenceFrequency>();if(rf){assert(rf->q20==100025000ll*(1ll<<20));readback=true;}}}
 assert(backend->writes()==unsigned(matching));assert(execution==matching&&readback==matching);assert(device->confirmed_state().fields[1].value==SemanticValue{*Hertz::from_integer(100000)});
 // Controller-only progress worked without PPS or a local started source. Its
 // shutdown cannot stop or establish physical quiescence of the remote device.
 assert((*client)->shutdown());assert((*client)->progress({40'000'000}));assert(device->confirmed_state().fields[4].validity==runtime::Validity::known);
}
int main(){for(auto family:{Family::ipv4,Family::ipv6})for(bool matching:{false,true})trial(family,matching);}
