#include "../P10/runtime_fixture.hpp"
#include <vita/profiles/iq/lab.hpp>
#include <new>
using namespace vita;using namespace verify_p10;
static std::size_t allocations=0;
void* operator new(std::size_t n){++allocations;if(auto*p=std::malloc(n?n:1))return p;std::abort();}
void* operator new[](std::size_t n){return ::operator new(n);}
void* operator new(std::size_t n,std::align_val_t a){++allocations;void*p=nullptr;if(!posix_memalign(&p,std::size_t(a),n?n:1))return p;std::abort();}
void* operator new[](std::size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}
int main(){using Runtime=VitaRuntime<1,4,32,65536>;auto made=Runtime::create(runtime_config(),external_pools());if(!made)return 1;auto& runtime=**made;auto device=runtime.add_controllee(stream_config());if(!device||!runtime.observe_pps({0},{1000,0}))return 2;auto controller=runtime.add_controller(*device);if(!controller)return 3;auto held=controller->query();if(!held||!runtime.progress({0})||!runtime.progress({1}))return 4;
 RecoveryConfig recovery{4,device->confirmed_state(),true};recovery.confirmed_state.fields[0].value=std::uint32_t{4};const auto before=allocations;
 if(!device->recover(recovery))return 5;for(unsigned n=2;n<10;++n)if(!runtime.progress({n}))return 6;
 if(allocations!=before)return 7;recovery.new_sid=5;recovery.confirmed_state.fields[0].value=std::uint32_t{5};if(device->recover(recovery)||allocations!=before)return 8;
 if(!device->shutdown()||!runtime.progress({10})||allocations!=before)return 9;
 return 0;}
