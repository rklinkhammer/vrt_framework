#include <vita/profiles/iq/lab.hpp>
#include <vita/runtime/public/runtime.hpp>
#include <cassert>
#include <cstdio>
using namespace vita;
struct Events{unsigned count=0;std::uint64_t ordinal=0;};
Result<void> observed(void*p,const runtime::EffectiveEvent&e)noexcept{auto& events=*static_cast<Events*>(p);assert(e.ordinal_known);assert(e.sample_ordinal>=events.ordinal);events.ordinal=e.sample_ordinal;++events.count;return {};}
int main(){
 auto config=profiles::iq::lab::config(0xabcdef);auto pools=profiles::iq::lab::pools();assert(config&&pools);
 auto runtime=VitaRuntime<1,4,32,65536>::create(*config,std::move(*pools));assert(runtime);
 StreamConfig stream;stream.sid=1;stream.controller_id=2;stream.controllee_id=3;stream.profile=profiles::iq::Profile::frequency_tunable;stream.sample_rate=100000;
 Events events;auto source=profiles::iq::default_source();stream.source={&events,source.callback,observed};
 auto device=(*runtime)->add_controllee(stream);assert(device);auto controller=(*runtime)->add_controller(*device);assert(controller);
 assert((*runtime)->observe_pps({0},{1000,0}));assert(device->start());assert(events.count==1);
 auto tune=controller->set_center_frequency(*Hertz::from_integer(100025000));assert(tune);
 assert((*runtime)->run_for(10'000'000));auto evidence=controller->observation(*tune);assert(evidence);auto executed=controller->wait(*tune,0,WaitEvidence::execution);assert(executed&&executed->status==WaitStatus::evidence_received&&executed->observation.confirms_execution);
 auto state=controller->state(*tune);assert(state&&*state);auto frequency=(**state).value<RFReferenceFrequency>();assert(frequency&&frequency->q20==100025000ll*(1ll<<20));assert(events.count==2);
 auto query=controller->query(QuerySelection{QueryField::sample_rate,QueryField::center_frequency});assert(query);assert((*runtime)->run_for(1000000));auto readback=controller->state(*query);assert(readback&&*readback);assert((**readback).value<SampleRate>()->q20==100000ll*(1ll<<20));assert((**readback).value<RFReferenceFrequency>()->q20==frequency->q20);
 auto rate=controller->set_sample_rate(*Hertz::from_integer(200000));assert(rate);assert((*runtime)->run_for(1000000));auto denied=controller->observation(*rate);assert(denied&&!denied->confirms_execution&&events.count==2);
 CommandOptions scheduled;scheduled.timing_mode=1;scheduled.execute_at=runtime::timing::ProtocolTime{1000,102'400'000'000};
 auto pending=controller->set_center_frequency(*Hertz::from_integer(100050000),scheduled);assert(pending);
 auto foreign=*pending;++foreign.runtime_id;auto invalid=controller->cancel(foreign,QuerySelection{QueryField::center_frequency});assert(!invalid&&invalid.error().code==ErrorCode::identity_conflict);
 assert(controller->cancel(*pending,QuerySelection{QueryField::center_frequency}));assert((*runtime)->run_for(2'000'000));assert(events.count==2);
 std::printf("state=%zu event=%zu revision=%zu ack=%zu\n",sizeof(runtime::StateSnapshot),sizeof(runtime::EffectiveEvent),sizeof(runtime::context::Revision),sizeof(runtime::transaction::AckRecord));
}
