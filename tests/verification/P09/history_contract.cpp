#include <vita/runtime/context/receiver.hpp>
using namespace vita;using namespace vita::runtime;using namespace vita::runtime::context;
static StateSnapshot state(unsigned rate){StateSnapshot s;for(auto id:baseline_fields)s.fields[field_index(id)].validity=Validity::known;s.fields[0].value=std::uint32_t{7};s.fields[1].value=*Hertz::from_integer(rate);s.fields[2].value=std::uint32_t{valid_data_enable|valid_data_indicator};s.fields[3].value=PayloadFormat{0x200003cf00000000ULL};return s;}
static bool rate(const MetadataSnapshot& s,unsigned expected){auto value=std::get_if<Hertz>(&s.state.fields[1].value);return value&&value->q20==std::int64_t(expected)*(1<<20);}
int main(){
    {
        ReceiverHistory<> h;auto a=state(1000000),b=state(2000000);
        if(!h.insert(b,{100,12000000000ULL},{2000000})||!h.insert(a,{100,10000000000ULL},{3000000}))return 1;
        auto old=h.resolve({100,11000000000ULL},{4000000});auto current=h.resolve({100,12000000000ULL},{4000000});
        if(old.confidence!=Confidence::known||!old.valid_data||!rate(old,1000000)||!rate(current,2000000))return 2;
        if(!h.insert(state(3000000),{100,12000000000ULL},{5000000})||h.resolve({100,12000000000ULL},{6000000}).confidence!=Confidence::ambiguous)return 3;
        if(!rate(old,1000000)||!h.insert(state(4000000),{100,14000000000ULL},{7000000}))return 4;
        if(h.resolve({100,13000000000ULL},{8000000}).confidence!=Confidence::ambiguous||h.resolve({100,14000000000ULL},{8000000}).confidence!=Confidence::known)return 5;
        if(h.resolve({100,9000000000ULL},{8000000}).confidence!=Confidence::missing)return 6;
    }
    {
        // A refresh observes only its own interval, and a duplicate cannot renew age.
        ReceiverHistory<> h;if(!h.insert(state(1),{20,0},{0}))return 7;
        if(h.resolve({21,999999999999ULL},{1999999999}).confidence!=Confidence::known)return 8;
        if(h.resolve({22,0},{1}).confidence!=Confidence::stale||h.resolve({20,0},{2000000000}).confidence!=Confidence::stale)return 9;
        if(!h.insert(state(1),{20,0},{1999999999})||h.resolve({20,0},{2000000000}).confidence!=Confidence::stale)return 10;
        h.expire({2000000000});if(h.size())return 11;
        if(!h.insert(state(2),{23,0},{2100000000})||h.resolve({22,0},{2200000000}).confidence!=Confidence::missing)return 12;
    }
    {
        ReceiverHistory<> h;for(unsigned i=0;i<129;++i)if(!h.insert(state(i+1),{10,i},{i}))return 13;
        if(h.size()!=128||h.resolve({10,0},{129}).confidence!=Confidence::missing||!rate(h.resolve({10,1},{129}),2))return 14;
        if(h.insert(state(9),{9,0},{130})||h.size()!=128)return 15;
    }
    {
        ReceiverHistory<> h;auto event=state(1);event.fields[2].value=std::uint32_t{valid_data_enable|valid_data_indicator|sample_loss_enable|sample_loss_indicator};
        if(!h.insert(event,{1,0},{0}))return 16;
        auto exact=h.resolve({1,0},{1}),later=h.resolve({1,1},{1});
        if(exact.events!=(sample_loss_enable|sample_loss_indicator)||later.events||!later.valid_data)return 17;
        if(std::get<std::uint32_t>(later.state.fields[2].value)&(sample_loss_enable|sample_loss_indicator))return 18;
        StateSnapshot delta;delta.fields[1].value=*Hertz::from_integer(2);delta.fields[1].validity=Validity::known;
        if(!h.insert(delta,{1,2},{2},false)||h.resolve({1,2},{3}).confidence!=Confidence::incomplete_delta)return 19;
        ReceiverHistory<> no_anchor;if(!no_anchor.insert(delta,{1,0},{0},false)||no_anchor.resolve({1,0},{1}).confidence==Confidence::known)return 20;
    }
    {
        ReceiverHistory<> h;auto malformed=state(1);malformed.fields[1].value=std::uint32_t{1};if(h.insert(malformed,{1,0},{0})||h.size())return 21;
        malformed=state(1);malformed.fields[1].value=Hertz{-1};if(h.insert(malformed,{1,0},{0})||h.size())return 22;
        malformed=state(1);malformed.fields[3].value=std::uint32_t{0};if(h.insert(malformed,{1,0},{0})||h.size())return 23;
    }
    return 0;
}
