#include <vita/profiles/iq/frequency_scan.hpp>
#include "../../../examples/frequency_scan/options.hpp"
#include <array>
#include <cassert>
using namespace vita;using namespace vita::profiles::iq;
namespace CLI=vita::examples::frequency_scan;
static auto parse(std::initializer_list<std::string_view> args){return CLI::parse_options(std::span{args.begin(),args.size()});}
int main(){
 auto defaults=parse({});assert(defaults&&defaults->sweep.start_hz==100'000'000&&defaults->sweep.stop_hz==100'200'000&&defaults->sweep.step_hz==25'000&&defaults->sweep.sweeps==1&&!defaults->sweep.continuous&&defaults->scene.sample_rate==100'000&&defaults->sweep.dwell_ns==100'000'000&&defaults->sweep.timeout_ns==1'000'000'000);
 for(auto args:{std::initializer_list<std::string_view>{"--step-hz","0"},{"--sweeps","0"},{"--dwell-ms","0"},{"--timeout-ms","0"},{"--start-hz","nan"},{"--start-hz","inf"},{"--start-hz","1e6"},{"--start-hz","1000000.0"},{"--step-hz","-1"},{"--step-hz","+1"},{"--step-hz","18446744073709551616"},{"--dwell-ms","18446744073710"},{"--start-hz","999999"},{"--stop-hz","6000000001"},{"--start-hz","100300000"},{"--sample-rate-hz","0"},{"--sample-rate-hz","100000001"},{"--sample-rate","100000"},{"--step-hz"},{"--unknown","1"},{"--sweeps","2","--continuous"},{"--continuous","--sweeps","2"}})assert(!CLI::parse_options(std::span{args.begin(),args.size()}));
 assert(parse({"--sample-rate-hz","1"}));assert(parse({"--continuous"}));assert(parse({"--start-hz","6000000000","--stop-hz","6000000000"}));
 SweepConfig config;config.start_hz=1'000'000;config.stop_hz=1'000'009;config.step_hz=4;config.sweeps=2;config.dwell_ns=10;config.timeout_ns=100;auto policy=SweepPolicy::create(config);assert(policy);constexpr std::array<std::uint64_t,6> expected{1'000'000,1'000'004,1'000'008,1'000'000,1'000'004,1'000'008};std::uint64_t now=0;
 for(unsigned i=0;i<expected.size();++i){auto point=policy->next({now});assert(point&&*point&&**point==expected[i]);assert(policy->phase()==SweepPhase::awaiting);auto not_yet=policy->next({now+1});assert(not_yet&&!*not_yet); // validation/local acceptance alone supplies no usable evidence.
  if(i%2){assert(policy->readback(expected[i],true,{now+5}));assert(policy->phase()==SweepPhase::awaiting);assert(policy->execution({true,false,false,false},{now+7}));}
  else{assert(policy->execution({true,false,false,false},{now+5}));assert(policy->phase()==SweepPhase::awaiting);assert(policy->readback(expected[i],true,{now+7}));}
  assert(policy->phase()==SweepPhase::dwelling&&policy->deadline_ns()==now+17);assert(policy->execution({true,false,false,false},{now+8}));assert(policy->readback(expected[i],true,{now+9}));assert(policy->deadline_ns()==now+17);assert(!*policy->next({now+16}));now+=17;
 }
 auto end=policy->next({now});assert(end&&!*end&&policy->phase()==SweepPhase::complete&&policy->completed_points()==6);
 auto timeout=SweepPolicy::create(config);assert(timeout&&timeout->next({0}));assert(!*timeout->next({99}));assert(!timeout->next({100})&&timeout->phase()==SweepPhase::failed);
 for(auto evidence:{TuneExecution{},TuneExecution{true,true,false,false},TuneExecution{true,false,true,false},TuneExecution{true,false,false,true}}){auto failed=SweepPolicy::create(config);assert(failed&&failed->next({0}));assert(!failed->execution(evidence,{1})&&failed->phase()==SweepPhase::failed);}
 auto mismatch=SweepPolicy::create(config);assert(mismatch&&mismatch->next({0}));assert(!mismatch->readback(1'000'001,true,{1}));
 auto overflow=SweepPolicy::create(config);assert(overflow&&!overflow->next({UINT64_MAX-99}));
 auto single=config;single.stop_hz=single.start_hz;single.step_hz=UINT64_MAX;single.sweeps=1;auto one=SweepPolicy::create(single);assert(one&&one->next({0}));assert(one->execution({true,false,false,false},{1})&&one->readback(single.start_hz,true,{1}));assert(!*one->next({11})&&one->phase()==SweepPhase::complete);
}
