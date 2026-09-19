#include "runtime_fixture.hpp"
using namespace vita;using namespace verify_p10;
static void observed(void*,const runtime::transaction::Observation&)noexcept{}
int main(){using Runtime=VitaRuntime<2,4,32,65536>;
 {auto pools=external_pools();pools.payload=pools.control;if(Runtime::create(runtime_config(),pools))return 1;}
 auto a=Runtime::create(runtime_config(),external_pools()),b=Runtime::create(runtime_config(),external_pools());if(!a||!b)return 2;auto&ra=**a;auto&rb=**b;
 auto invalid=stream_config();invalid.controller_peer=invalid.controllee_peer;const auto before=ra.budget().charged_bytes();if(ra.add_controllee(invalid)||ra.budget().charged_bytes()!=before)return 3;
 auto da=ra.add_controllee(stream_config()),db=rb.add_controllee(stream_config());auto second=stream_config();second.sid=2;auto da2=ra.add_controllee(second);if(!da||!db||!da2||ra.add_controller(*db))return 4;
 auto ca=ra.add_controller(*da),cb=rb.add_controller(*db),ca2=ra.add_controller(*da2);if(!ca||!cb||!ca2)return 5;
 auto ha=ca->set_sample_rate(*Hertz::from_integer(2000000)),hb=cb->set_sample_rate(*Hertz::from_integer(2000000));if(!ha||!hb)return 6;
 if(ca2->observation(*ha)||ca2->deadline(*ha)||ca2->state(*ha)||ca2->observe(*ha,nullptr,observed)||ca2->wait(*ha,0)||ca2->cancel(*ha)||ca2->release(*ha))return 7;
 if(ca->observation(*hb)||ca->deadline(*hb)||ca->state(*hb)||ca->observe(*hb,nullptr,observed)||ca->wait(*hb,0)||ca->cancel(*hb)||ca->release(*hb))return 8;
 if(!ca->observation(*ha)||!cb->observation(*hb))return 9;
 return 0;}
