#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
#include <cassert>
using namespace vita;
int main(){
 auto config=profiles::iq::lab::config(0xabcdef);auto pools=profiles::iq::lab::pools();assert(config&&pools);
 auto runtime=VitaRuntime<2,4,32,65536>::create(*config,std::move(*pools));assert(runtime);
 StreamConfig stream;stream.sid=1;stream.controller_id=2;stream.controllee_id=3;stream.profile=profiles::iq::Profile::frequency_tunable;stream.sample_rate=100000;
 auto malformed=stream;malformed.device.storage_bytes=5;assert(!(*runtime)->add_controllee(malformed));
 malformed=stream;malformed.device.progress=[](void*,const runtime::transaction::OperationContext&)noexcept{};assert(!(*runtime)->add_controllee(malformed));
 using Backend=runtime::transaction::VirtualBackend<4>;auto backend=std::make_shared<Backend>();std::weak_ptr<Backend> weak=backend;
 stream.device={backend->binding(),backend,sizeof(Backend),[](void*p,const runtime::transaction::OperationContext&)noexcept {auto& b=*static_cast<Backend*>(p);if(b.pending())b.complete_next();}};
 auto device=(*runtime)->add_controllee(stream);assert(device);auto controller=(*runtime)->add_controller(*device);assert(controller);
 assert(!(*runtime)->configure_virtual_backend(*device,RFReferenceFrequency::id,{}));
 assert((*runtime)->observe_pps({0},{1000,0}));assert(device->start());
 auto tune=controller->set_center_frequency(*Hertz::from_integer(100025000));assert(tune);assert((*runtime)->run_for(10'000'000));
 auto done=controller->wait(*tune,0);assert(done&&done->status==WaitStatus::evidence_received&&done->observation.confirms_execution);
 // External ownership is pinned, and omitted simulation is rejected explicitly.
 assert(backend->writes()==1);
 RecoveryConfig recovery;recovery.new_sid=4;recovery.peer_ready=true;recovery.confirmed_state=device->confirmed_state();recovery.confirmed_state.fields[0].value=std::uint32_t{4};
 recovery.reinitialize_context=backend.get();recovery.reinitialize=[](void*p,const runtime::StateSnapshot&)noexcept->Result<bool>{auto reset=static_cast<Backend*>(p)->reinitialize();if(!reset)return std::unexpected(reset.error());return true;};
 assert(device->recover(recovery));for(unsigned n=0;n<8;++n)assert((*runtime)->progress({20'000'000}));assert(device->sid()==4);assert(device->status()==SourceStatus::running);assert((*runtime)->run_for(3'000'000));
 assert(device->confirmed_state().fields[4].value==recovery.confirmed_state.fields[4].value);

}
