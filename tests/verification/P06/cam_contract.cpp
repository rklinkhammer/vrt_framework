#include <vita/runtime/transaction/cam.hpp>
using namespace vita;using namespace vita::codec;using namespace vita::runtime;using namespace vita::runtime::transaction;
static Envelope envelope(unsigned action,unsigned permissions,unsigned requests,unsigned details,bool nack){
    Envelope e;e.type=PacketType::command;e.stream_id=1;
    e.command=Command{0xa0000000u|(action<<23)|(permissions<<25)|(requests<<18)|(details<<16)|(nack?1u<<22:0),1,Identifier::short_id(2),Identifier::short_id(3)};
    return e;
}
int main(){
    unsigned cases=0;
    // Independent truth table: raw tuple P,W,Er and request tuple V,X,S; do not use production bit helpers.
    for(unsigned action=0;action<4;++action)for(unsigned permissions=0;permissions<8;++permissions)
    for(unsigned requests=0;requests<8;++requests)for(unsigned details=0;details<4;++details)for(unsigned nack=0;nack<2;++nack){
        auto e=envelope(action,permissions,requests,details,nack);auto generic=Cam::parse(e,Profile::generic_virtual_test);auto profile=Cam::parse(e,Profile::iq_generator_v1);
        if(bool(generic)!=(action!=3))return 1;
        bool profile_allowed=action!=3 && !(action==2 && (requests&1) && !(requests&2));
        if(bool(profile)!=profile_allowed)return 2;
        if(!generic)continue;const auto& c=*generic;
        if(c.action!=action || c.partial!=bool(permissions&4) || c.allow_warning!=bool(permissions&2) || c.allow_error!=bool(permissions&1) || c.nack!=bool(nack) || c.request_v!=bool(requests&4) || c.request_x!=bool(requests&2) || c.request_s!=bool(requests&1) || c.detail_warning!=bool(details&2) || c.detail_error!=bool(details&1))return 3;
        for(unsigned quality=0;quality<4;++quality)for(unsigned resolvable=0;resolvable<2;++resolvable){
            Diagnostics d{quality&1?0x10u:0u,quality&2?0x20u:0u};
            bool expected=resolvable && (!(quality&1)||(permissions&2)) && (!(quality&2)||(permissions&1));
            if(eligible(c,d,resolvable)!=expected)return 4;
            for(unsigned phase=0;phase<3;++phase){
                bool requested=bool(requests&(4u>>phase));bool emits=requested && (phase==2 || !nack || quality);
                if(should_emit(c,static_cast<AckKind>(phase),d)!=emits)return 5;
            }
            ++cases;
        }
    }
    if(cases!=12288)return 6;
    // Profile precision warnings: exact halves round to even, outside range remains unresolvable.
    constexpr std::int64_t u=1ll<<20;
    for(auto [raw,expected]:{std::pair{2*u+u/2,2*u},std::pair{3*u+u/2,4*u},std::pair{2*u+u/2+1,3*u},std::pair{2*u+u/2-1,2*u}}){
        auto v=iq_validate(SampleRate::id,Hertz{raw});if(!v.resolvable||!v.diagnostics.warnings||v.diagnostics.errors||std::get<Hertz>(v.adjusted).q20!=expected)return 7;
    }
    for(std::int64_t raw:{-u,std::int64_t{0},u-1,100000000*u+1}){auto v=iq_validate(SampleRate::id,Hertz{raw});if(v.resolvable||!v.diagnostics.errors)return 8;}
    for(std::int64_t raw:{u,100000000*u}){auto v=iq_validate(SampleRate::id,Hertz{raw});if(!v.resolvable||v.diagnostics.errors||v.diagnostics.warnings)return 9;}
    return 0;
}
