#include <vita/runtime/timing/sample_timeline.hpp>
#include <array>
#include <limits>
using namespace vita;using namespace vita::runtime::timing;
int main(){
    constexpr auto max=std::numeric_limits<std::uint64_t>::max();
    auto carried=add({1700000000,999999999999},{0,1});
    if(!carried || *carried!=ProtocolTime{1700000001,0} || add({max,999999999999},{0,1}) || add({1,1000000000000},{0,0}))return 1;
    auto borrowed=subtract({10,0},{0,1});if(!borrowed || *borrowed!=ProtocolTime{9,999999999999} || subtract({0,0},{0,1}))return 2;
    auto low=SampleTimeline::create({1700000000,0},1);if(!low || !low->advance(256) || low->time()!=ProtocolTime{1700000256,0})return 3;
    auto high=SampleTimeline::create({1700000000,0},100000000);if(!high || !high->advance(100000000) || high->time()!=ProtocolTime{1700000001,0})return 4;
    struct Expected{std::uint64_t rate,integer,numerator,denominator;};
    // Independently derived exact Fractions: sum(10^12 / r) for successive one-sample segments.
    constexpr std::array expected{
      Expected{3,333333333333,1,3},Expected{7,476190476190,10,21},Expected{11,567099567099,131,231},
      Expected{99999989,567099577099,13125408559,23099997459},
      Expected{99999931,567099587099,1328478948493519429ULL,2309998152000175329ULL}};
    auto timeline=SampleTimeline::create({0,0},3);if(!timeline)return 5;
    for(std::size_t i=0;i<expected.size();++i){
        const auto e=expected[i];if(i && !timeline->change_rate(e.rate))return 6;
        if(!timeline->advance(1))return 7;auto actual=timeline->exact_time();
        if(actual.time!=ProtocolTime{0,e.integer} || actual.numerator!=e.numerator || actual.denominator!=e.denominator || timeline->ordinal()!=i+1)return 8;
    }
    const auto before=timeline->exact_time();const auto revision=timeline->revision();
    auto overflow=timeline->change_rate(99999959);
    if(overflow || overflow.error().code!=ErrorCode::resource_limit || timeline->revision()!=revision || timeline->rate()!=99999931 ||
       timeline->exact_time().time!=before.time || timeline->exact_time().numerator!=before.numerator)return 9;
    auto endpoint=SampleTimeline::create({0,0},3);if(!endpoint || !endpoint->advance(1) || !endpoint->change_rate(7) || !endpoint->advance(7))return 10;
    if(endpoint->time()!=ProtocolTime{1,333333333333} || endpoint->exact_time().numerator!=1 || endpoint->exact_time().denominator!=3)return 11;
    auto final=SampleTimeline::create({max,0},1);if(!final || final->advance(1) || final->ordinal()!=0 || final->time()!=ProtocolTime{max,0})return 12;
    if(SampleTimeline::create({0,0},0) || SampleTimeline::create({0,0},100000001) || deadline({max},1))return 13;
    if(as_picoseconds({1700000000,0}))return 14;
    return 0;
}
