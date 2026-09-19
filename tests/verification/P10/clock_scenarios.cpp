#include "runtime_fixture.hpp"
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::transaction;using namespace verify_p10;
struct Events{bool validation=false,execution=false,confirmed=false;static void record(void*p,const Observation&o)noexcept{auto&e=*static_cast<Events*>(p);e.validation|=o.response_kind==ObservationKind::validation;e.execution|=o.response_kind==ObservationKind::execution;e.confirmed|=o.confirms_execution;}};
int main(){using Runtime=VitaRuntime<1,4,32,65536>;
 {
  auto instance=Runtime::create(runtime_config(),external_pools());if(!instance)return 1;auto&r=**instance;auto device=r.add_controllee(stream_config());if(!device)return 2;auto controller=r.add_controller(*device);if(!controller||!r.observe_pps({0},{1000,0}))return 3;
  // One genuinely unanswered transaction tests the deadline; independent timed
  // work on the same clock proves mapping revalidation by failed execution evidence.
  adapters::loopback::Fault lost;lost.lose=true;r.inject_next_transport_fault(lost);CommandOptions deadline;deadline.timeout_ns=50000000;auto missing=controller->query(2,deadline);if(!missing)return 4;
  CommandOptions scheduled;scheduled.timing_mode=1;scheduled.execute_at=timing::ProtocolTime{1001,192000000};auto timed=controller->set_sample_rate(*Hertz::from_integer(2000000),scheduled);Events events;if(!timed||!controller->observe(*timed,&events,Events::record)||!r.progress({0})||!events.validation||events.execution)return 5;
  if(!r.observe_pps({10000000},{2000,0})||!r.progress({10000000})||!events.execution||events.confirmed)return 6;
  if(!controller->deadline(*missing)||controller->deadline(*missing)->ns!=50000000||!r.progress({49999999})||controller->observation(*missing)->kind==ObservationKind::timeout)return 7;
  if(!r.progress({50000000})||controller->observation(*missing)->kind!=ObservationKind::timeout||r.monotonic_now().ns!=50000000)return 8;
  auto query=controller->query();if(!query||!r.progress({50000001})||!r.progress({50000002}))return 9;auto state=controller->state(*query);if(!state||!*state||(**state).state.fields[1].validity!=Validity::known||std::get<Hertz>((**state).state.fields[1].value)!=*Hertz::from_integer(1000000))return 10;
 }
 {
  auto instance=Runtime::create(runtime_config(),external_pools());if(!instance)return 11;auto&r=**instance;auto device=r.add_controllee(stream_config());if(!device)return 12;auto controller=r.add_controller(*device);if(!controller||!r.observe_pps({0},{1000,0})||!r.progress({2000000000})||!r.clock_snapshot()||r.clock_snapshot()->state!=timing::ClockState::faulted||device->start())return 13;
  auto query=controller->query();if(!query||!r.progress({2000000001})||!r.progress({2000000002}))return 14;auto state=controller->state(*query);if(!state||!*state||(**state).state.fields[1].validity!=Validity::known)return 15;
  adapters::loopback::Fault lost;lost.lose=true;r.inject_next_transport_fault(lost);CommandOptions options;options.timeout_ns=50000000;auto missing=controller->query(2,options);if(!missing||!r.progress({2050000001})||controller->observation(*missing)->kind==ObservationKind::timeout)return 16;
  if(!r.progress({2050000002})||controller->observation(*missing)->kind!=ObservationKind::timeout||device->metrics().packets)return 17;
 }
 return 0;}
