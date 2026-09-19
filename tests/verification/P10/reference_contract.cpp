#include <vita/runtime/public/runtime.hpp>
#include <vita/profiles/iq/lab.hpp>
#include <cstdio>
using namespace vita;
int main(){auto config=profiles::iq::lab::config(0x00a1b2);auto measured=profiles::iq::lab::measure(profiles::iq::lab::reference_counts());if(!config||!measured||measured->raw_bytes!=30998528)return 1;auto pools=profiles::iq::lab::pools(profiles::iq::lab::reference_counts());if(!pools)return 2;auto instance=VitaRuntime<>::create(*config,std::move(*pools));if(!instance)return 3;auto&r=**instance;std::array<std::optional<VitaRuntime<>::Controllee>,16> streams;
 for(unsigned i=0;i<16;++i){StreamConfig c;c.sid=i+1;c.controller_id=2;c.controllee_id=3;auto stream=r.add_controllee(c);if(!stream)return 4;streams[i]=*stream;}
 const auto charged=r.budget().charged_bytes();if(charged>67108864||r.budget().row(runtime::BudgetCategory::raw_blocks).charged!=30998528)return 5;
 StreamConfig extra;extra.sid=17;extra.controller_id=2;extra.controllee_id=3;if(r.add_controllee(extra)||r.budget().charged_bytes()!=charged)return 6;
 if(!r.observe_pps({0},{1000,0}))return 7;for(auto&s:streams)if(!s->start())return 8;if(!r.progress({0}))return 9;for(auto&s:streams)if(s->metrics().packets!=1)return 10;
 std::printf("reference raw=%zu charged=%zu streams=16\n",measured->raw_bytes,charged);return 0;}
