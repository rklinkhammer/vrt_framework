#include <vita/runtime/timing/clock.hpp>
#include <limits>
using namespace vita;using namespace vita::runtime::timing;
int main(){
    ProtocolClock clock;if(clock.data_start_allowed() || clock.snapshot({0}))return 1;
    ClockBinding binding{Epoch::other,true,false,true,100,10,2000000000};
    auto invalid=binding;invalid.injected=false;if(clock.bind(invalid))return 2;
    if(!clock.bind(binding) || clock.state()!=ClockState::acquiring || clock.observe_pps({0},std::nullopt))return 3;
    if(!clock.observe_pps({0},ProtocolTime{1000,0},50) || !clock.data_start_allowed())return 4;
    auto now=clock.snapshot({1000000});if(!now || now->time!=ProtocolTime{1000,1000000000} || now->uncertainty_ps!=160 || !now->calibrated || now->epoch!=Epoch::other)return 5;
    auto timeout=deadline({0},50000000);const auto old_generation=clock.generation();
    if(!clock.observe_pps({10000000},ProtocolTime{2000,0}) || clock.mapping_current(old_generation) || !timeout || timeout->ns!=50000000)return 6;
    auto step=clock.snapshot({50000000});if(!step || step->time!=ProtocolTime{2000,40000000000} || timeout->ns!=50000000)return 7;
    if(!clock.pps_lost({1010000000}) || clock.data_start_allowed() || !clock.data_continue_allowed())return 8;
    auto hold=clock.snapshot({1010000000});if(!hold || hold->calibrated || hold->state!=ClockState::holdover)return 9;
    auto fault=clock.snapshot({2010000000});if(!fault || fault->state!=ClockState::faulted || clock.data_continue_allowed() || clock.data_start_allowed())return 10;
    if(!clock.observe_pps({3010000000},ProtocolTime{2003,0}) || !clock.data_start_allowed())return 11;
    if(clock.snapshot({0}))return 12;
    ClockBinding overflow_binding{Epoch::gps,true,false,true,std::numeric_limits<std::uint64_t>::max(),0,2000000000};
    ProtocolClock bad;if(!bad.bind(overflow_binding) || bad.observe_pps({0},ProtocolTime{1,0},1) || bad.data_start_allowed())return 13;
    return 0;
}
