#include <vita/runtime/timing/scheduling.hpp>
#include <array>
using namespace vita;using namespace vita::runtime::timing;
static ProtocolTime ns(std::uint64_t n){return {n/1000000000,(n%1000000000)*1000};}
int main(){
    const auto cap=TimingCapabilities::deterministic();
    struct Case{unsigned mode;std::uint64_t first,last;bool allowed;};
    constexpr std::array cases{Case{1,9999500,10000500,true},Case{1,10000000,10002000,false},
      Case{2,10500000,10501000,true},Case{3,10500000,10501000,false},Case{3,9500000,9501000,true},Case{4,9000000,11000000,true}};
    for(const auto c:cases){auto r=effect_interval_allowed(c.mode,ns(10000000),ns(c.first),ns(c.last),cap);if(!r || *r!=c.allowed)return 1;}
    ClockSnapshot clock{ClockState::locked,ns(0),0,7,true,true,Epoch::other};
    std::array<Boundary,2> ties{{{ns(11000000),2,7},{ns(9000000),1,7}}};
    auto chosen=choose_boundary(4,ns(10000000),clock,ties,cap);if(!chosen || chosen->sample_ordinal!=1)return 2;
    auto wrong=ties;wrong[0].mapping_generation=6;wrong[1].committed=true;
    if(choose_boundary(4,ns(10000000),clock,wrong,cap))return 3;
    std::array<Boundary,1> late{{{ns(11000500),3,7}}};clock.uncertainty_ps=500000;
    if(!choose_boundary(2,ns(10000000),clock,late,cap))return 4;
    late[0].time=ns(11000501);if(choose_boundary(2,ns(10000000),clock,late,cap))return 5;
    clock.uncertainty_ps=2000000;std::array<Boundary,1> exact{{{ns(10000000),4,7}}};
    if(choose_boundary(1,ns(10000000),clock,exact,cap))return 6;
    clock.state=ClockState::holdover;if(!choose_boundary(4,ns(10000000),clock,exact,cap))return 7;
    clock.state=ClockState::faulted;if(choose_boundary(4,ns(10000000),clock,exact,cap))return 8;
    std::array<Boundary,1> immediate{{{ns(256000),5,7}}};
    if(!choose_boundary(0,{0,1000000000000},clock,immediate,cap))return 9;
    immediate[0].backend_ready=false;if(choose_boundary(0,ns(0),clock,immediate,cap))return 10;
    clock.state=ClockState::locked;clock.uncertainty_ps=0;
    std::array<Boundary,129> too_many{};if(choose_boundary(1,ns(10000000),clock,too_many,cap))return 11;
    auto unqualified=cap;unqualified.injected=false;if(choose_boundary(1,ns(10000000),clock,exact,unqualified))return 12;
    if(choose_boundary(1,ns(10000000001ULL),clock,exact,cap))return 13;
    auto timeline=SampleTimeline::create({0,0},3);if(!timeline || !timeline->advance(1))return 14;
    std::array<Boundary,1> fractional{{make_boundary(*timeline,7)}};
    auto precise=cap;precise.device_early_ps=precise.device_late_ps=0;
    if(fractional[0].quantization_uncertainty_ps!=1 || choose_boundary(1,timeline->time(),clock,fractional,precise))return 15;
    return 0;
}
