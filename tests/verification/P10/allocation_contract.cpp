#include "runtime_fixture.hpp"
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
int main(){using Runtime=VitaRuntime<2,4,32,65536>;auto pools=external_pools();auto config=runtime_config();config.memory_limit=1;auto before=allocations;auto too_small=Runtime::create(config,pools);if(too_small||allocations!=before)return 1;
 profiles::iq::lab::PoolCounts invalid;invalid.max_setup_bytes=1;before=allocations;auto rejected=profiles::iq::lab::pools(invalid);if(rejected||allocations!=before)return 2;
 config=runtime_config();auto first=Runtime::create(config,pools);if(!first)return 3;const auto baseline=(*first)->budget().charged_bytes();if((*first)->budget().row(runtime::BudgetCategory::raw_blocks).charged!=991232)return 4;
 config.memory_limit=baseline;auto tight=Runtime::create(config,pools);if(!tight)return 5;before=allocations;auto no_stream=(*tight)->add_controllee(stream_config());if(no_stream||allocations!=before||(*tight)->budget().charged_bytes()!=baseline)return 6;
 auto shared=pools;shared.trailer=shared.header;auto alias=Runtime::create(runtime_config(),shared);if(!alias)return 7;if((*alias)->budget().row(runtime::BudgetCategory::raw_blocks).charged!=991232-4096)return 8;
 auto device=(*first)->add_controllee(stream_config());if(!device||!(*first)->observe_pps({0},{1000,0})||!device->start())return 9;
 before=allocations;for(unsigned i=0;i<100;++i)if(!(*first)->progress({i*256000ULL}))return 10;
 if(allocations!=before||device->metrics().packets!=100)return 11;
 return 0;}
