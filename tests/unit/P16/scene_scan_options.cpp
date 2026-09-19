#include "../../../examples/frequency_scan/options.hpp"
#include <array>
#include <cassert>
using namespace vita::examples::frequency_scan;
int main(){
    auto defaults=parse_options({});assert(defaults&&defaults->sweep.step_hz==25'000&&!defaults->sweep.continuous);
    constexpr std::array<std::string_view,7> good{"--start-hz","100025000","--step-hz","1000","--dwell-ms","20","--continuous"};
    auto parsed=parse_options(good);assert(parsed&&parsed->sweep.start_hz==100'025'000&&parsed->sweep.dwell_ns==20'000'000&&parsed->sweep.continuous);
    for(auto text:{"-1","0","1x","18446744073709551616"}){
        const std::array<std::string_view,2> args{"--step-hz",text};assert(!parse_options(args));
    }
    for(auto key:{"--dwell-ms","--timeout-ms","--sweeps"}){const std::array<std::string_view,2> args{key,"0"};assert(!parse_options(args));}
    const std::array<std::string_view,2> count{"--sweeps","3"};assert(parse_options(count)->sweep.sweeps==3);
    const std::array<std::string_view,3> conflict1{"--sweeps","2","--continuous"},conflict2{"--continuous","--sweeps","2"};
    assert(!parse_options(conflict1)&&!parse_options(conflict2));
    constexpr std::array<std::string_view,2> overflow{"--timeout-ms","18446744073709551615"};assert(!parse_options(overflow));
    constexpr std::array<std::string_view,1> missing{"--sample-rate-hz"};assert(!parse_options(missing));
    constexpr std::array<std::string_view,2> unknown{"--device","1"};assert(!parse_options(unknown));
}
