#include "runtime_fixture.hpp"
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transaction;using namespace verify_p10;
int main(){using Runtime=VitaRuntime<1,4,32,65536>;auto instance=Runtime::create(runtime_config(),external_pools());if(!instance)return 1;auto&r=**instance;auto d=r.add_controllee(stream_config());if(!d)return 2;auto c=r.add_controller(*d);if(!c||!r.observe_pps({0},{1000,0}))return 3;
 adapters::loopback::Fault loss;loss.lose=true;r.inject_next_transport_fault(loss);CommandOptions options;options.timeout_ns=50000;auto lost=c->query(2,options);if(!lost)return 4;
 auto first=c->wait(*lost,10000,WaitEvidence::state);if(!first||first->status!=WaitStatus::wait_budget_expired||r.monotonic_now().ns!=10000||c->deadline(*lost)->ns!=50000||c->observation(*lost)->kind==ObservationKind::timeout)return 5;
 auto terminal=c->wait(*lost,100000,WaitEvidence::state);if(!terminal||terminal->status!=WaitStatus::transaction_timeout||r.monotonic_now().ns!=50000||terminal->observation.confirms_execution)return 6;
 auto query=c->query();if(!query)return 7;const auto start=r.monotonic_now().ns;auto state=c->wait(*query,100000,WaitEvidence::state);if(!state||state->status!=WaitStatus::evidence_received||state->observation.response_kind!=ObservationKind::state||state->observation.confirms_execution||r.monotonic_now().ns>=start+100000)return 8;
 auto polled=c->wait(*query,0,WaitEvidence::state);if(!polled||polled->status!=WaitStatus::evidence_received)return 9;
 for(bool nack:{false,true}){CommandOptions silent;silent.validation=false;silent.state=false;silent.execution=nack;silent.nack=nack;silent.timeout_ns=50000;auto handle=c->set_sample_rate(*Hertz::from_integer(1000000),silent);if(!handle)return 10;auto result=c->wait(*handle,10000);if(!result||result->status!=WaitStatus::wait_budget_expired||result->observation.confirms_execution||result->observation.success)return 11;}
 return 0;}
