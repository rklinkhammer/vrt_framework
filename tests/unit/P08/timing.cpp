#include <vita/runtime/timing/sample_timeline.hpp>
#include <vita/runtime/timing/scheduling.hpp>
#include <array>
#include <cassert>
#include <iostream>
using namespace vita;
using namespace vita::runtime::timing;
int main() {
    auto carry=add({1700000000,999999999999},{0,1}); assert(carry && carry->seconds==1700000001 && carry->picoseconds==0);
    assert(!add({UINT64_MAX,999999999999},{0,1})); assert(!subtract({0,0},{0,1}));
    auto timeline=SampleTimeline::create({1700000000,0},3); assert(timeline && timeline->advance(1));
    assert(timeline->time().picoseconds==333333333333 && timeline->exact_time().numerator==1 && timeline->exact_time().denominator==3);
    assert(timeline->change_rate(7) && timeline->advance(1)); assert(timeline->time().picoseconds==476190476190 && timeline->exact_time().numerator==10 && timeline->exact_time().denominator==21);
    assert(timeline->advance(6)); assert(timeline->time().seconds==1700000001 && timeline->time().picoseconds==333333333333);
    auto fast=SampleTimeline::create({0,0},100000000); assert(fast && fast->advance(100000000)); assert(fast->time()==(ProtocolTime{1,0}));
    auto slow=SampleTimeline::create({0,0},1); assert(slow && slow->advance(1)); assert(slow->time()==(ProtocolTime{1,0}));
    auto bounded=SampleTimeline::create({0,0},99999989); assert(bounded && bounded->advance(1)); assert(bounded->change_rate(99999971)); assert(bounded->advance(1));
    auto before=bounded->exact_time(); auto rate=bounded->rate(); assert(!bounded->change_rate(99999959)); assert(bounded->rate()==rate && bounded->exact_time().denominator==before.denominator);
    ProtocolClock clock; ClockBinding binding; binding.epoch_configured=true; binding.injected=true; binding.holdover_drift_ppb=100;
    assert(clock.bind(binding)); assert(!clock.data_start_allowed()); assert(!clock.observe_pps({0},{}));
    assert(clock.observe_pps({100},ProtocolTime{1700000000,0})); auto generation=clock.generation(); assert(clock.data_start_allowed());
    auto snap=clock.snapshot({1000000100}); assert(snap && snap->time.seconds==1700000001);
    assert(clock.pps_lost({1000000100})); snap=clock.snapshot({1000000100}); assert(snap && !snap->calibrated && snap->uncertainty_ps==100000);
    assert(!clock.data_start_allowed() && clock.data_continue_allowed()); auto timeout=deadline({100},5000); assert(timeout && timeout->ns==5100);
    snap=clock.snapshot({2000000100}); assert(snap && snap->state==ClockState::faulted && !clock.data_continue_allowed());
    assert(clock.observe_pps({3000000100},ProtocolTime{1800000000,0})); assert(!clock.mapping_current(generation)); assert(timeout->ns==5100);
    auto capabilities=TimingCapabilities::deterministic(); ProtocolTime requested{100,10000000000ULL};
    auto allowed=effect_interval_allowed(1,requested,{100,9999500000},{100,10000500000},capabilities); assert(allowed && *allowed);
    allowed=effect_interval_allowed(1,requested,{100,10000000000},{100,10002000000},capabilities); assert(allowed && !*allowed);
    allowed=effect_interval_allowed(2,requested,{100,10500000000},{100,10501000000},capabilities); assert(allowed && *allowed);
    allowed=effect_interval_allowed(3,requested,{100,10500000000},{100,10501000000},capabilities); assert(allowed && !*allowed);
    allowed=effect_interval_allowed(3,requested,{100,9500000000},{100,9501000000},capabilities); assert(allowed && *allowed);
    allowed=effect_interval_allowed(4,requested,{100,9000000000},{100,11000000000},capabilities); assert(allowed && *allowed);
    ClockSnapshot scheduled{ClockState::locked,{100,0},0,9,true,true,Epoch::other};
    std::array<Boundary,2> boundaries{{{{100,9999500000},10,9,false},{{100,10000500000},11,9,false}}};
    auto chosen=choose_boundary(1,requested,scheduled,boundaries,capabilities); assert(chosen && chosen->sample_ordinal==10);
    scheduled.uncertainty_ps=1000000; assert(!choose_boundary(1,requested,scheduled,boundaries,capabilities));
    scheduled.state=ClockState::faulted; assert(!choose_boundary(1,requested,scheduled,boundaries,capabilities)); assert(choose_boundary(0,{UINT64_MAX,UINT64_MAX},scheduled,boundaries,capabilities));
    std::array<Boundary,1> immediate{{{{100,256000000},1,9,false}}};
    assert(choose_boundary(0,{},scheduled,immediate,capabilities)); // next 256 us, no timed 1 ms lead
    auto fractional=SampleTimeline::create({100,0},3); assert(fractional && fractional->advance(1));
    auto fractional_boundary=make_boundary(*fractional,9); assert(fractional_boundary.quantization_uncertainty_ps==1);
    TimingCapabilities exact_caps{0,0,0,0,0,10000000000ULL,false,true}; scheduled.state=ClockState::locked; scheduled.uncertainty_ps=0;
    assert(!choose_boundary(1,fractional->time(),scheduled,std::span(&fractional_boundary,1),exact_caps));
    std::cout<<"P08 checks passed; clock="<<sizeof(ProtocolClock)<<" timeline="<<sizeof(SampleTimeline)<<" boundary="<<sizeof(Boundary)<<" snapshot="<<sizeof(ClockSnapshot)<<"\n";
}
