#include <vita/adapters/posix_udp/factory.hpp>
#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
#include <cassert>
#include <cstdio>
using namespace vita;
using namespace vita::adapters::posix_udp;
int main(){
 FactoryConfig<32,8> client_setup,server_setup;
 for(auto* setup:{&client_setup,&server_setup}){setup->config.capabilities.reserved_control_slots=8;setup->config.capabilities.reserved_cancellation_slots=2;for(auto& socket:setup->config.sockets)socket.bind=Address::loopback(Family::ipv4);}
 auto client_config=profiles::iq::lab::config(0xabcdef),server_config=client_config;
 auto client_pools=profiles::iq::lab::pools(),server_pools=profiles::iq::lab::pools();assert(client_config&&server_config&&client_pools&&server_pools);
 client_config->transport=factory(client_setup);server_config->transport=factory(server_setup);
 using Runtime=VitaRuntime<1,4,32,65536>;
 auto client=Runtime::create(*client_config,std::move(*client_pools)),server=Runtime::create(*server_config,std::move(*server_pools));assert(client&&server);
 PeerBinding outbound;outbound.local_source={1,1};outbound.remote_source={2,1};PeerBinding inbound;inbound.local_source={2,1};inbound.remote_source={1,1};
 for(unsigned i=0;i<3;++i){outbound.remote[i]=server_setup.instance->local_address(static_cast<Lane>(i));inbound.remote[i]=client_setup.instance->local_address(static_cast<Lane>(i));}
 assert(client_setup.instance->add_peer(outbound));assert(server_setup.instance->add_peer(inbound));
 StreamConfig stream;stream.sid=1;stream.controller_id=2;stream.controllee_id=3;stream.sample_rate=100000;stream.profile=profiles::iq::Profile::frequency_tunable;stream.role=EndpointRole::controllee_only;
 auto device=(*server)->add_controllee(stream);assert(device);assert(!(*server)->add_controller(*device));
 RemoteTargetConfig target;target.sid=1;target.controller_id=2;target.controllee_id=3;target.profile=profiles::iq::Profile::frequency_tunable;
 auto controller=(*client)->add_remote_controller(target);assert(controller);
 assert((*server)->observe_pps({0},{1000,0}));assert(device->start());
 auto transaction=controller->set_center_frequency(*Hertz::from_integer(100025000));assert(transaction);
 bool done=false;for(std::uint64_t now=0;now<50'000'000;now+=100'000){auto a=(*server)->progress({now});if(!a){std::fprintf(stderr,"server error%u retry%d at%llu\n",unsigned(a.error().code),a.error().retryable,(unsigned long long)now);assert(a.error().retryable);}auto b=(*client)->progress({now});if(!b)assert(b.error().retryable);auto evidence=controller->wait(*transaction,0);if(evidence&&evidence->status==WaitStatus::evidence_received&&evidence->observation.confirms_execution){auto state=controller->state(*transaction);if(state&&*state){assert((**state).value<RFReferenceFrequency>()->q20==100025000ll*(1ll<<20));done=true;break;}}}assert(done);
 assert((*client)->shutdown());assert((*client)->progress({50'000'000}));
}
