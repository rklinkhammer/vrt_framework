#include "../../../bench/allocation.hpp"
#include <cstdlib>
#include <new>
using namespace vita::bench;
int main(){
 auto c_probe=[](auto call){critical_c_allocations=0;critical_thread=true;void* p=call();critical_thread=false;if(!p||critical_c_allocations.load()==0)return false;std::free(p);return true;};
 if(!c_probe([]{auto* volatile f=&std::malloc;return f(17);}))return 1;
 if(!c_probe([]{auto* volatile f=&std::calloc;return f(3,17);}))return 2;
 void* previous=std::malloc(17);if(!c_probe([&]{auto* volatile f=&std::realloc;return f(previous,91);}))return 3;
 if(!c_probe([]{void* p=nullptr;auto* volatile f=&posix_memalign;return f(&p,64,128)==0?p:nullptr;}))return 4;
 if(!c_probe([]{auto* volatile f=&std::aligned_alloc;return f(64,128);}))return 5;
 for(unsigned kind=0;kind<4;++kind){critical_allocations=0;critical_thread=true;void* p=kind==0?::operator new(17):kind==1?::operator new[](17):kind==2?::operator new(128,std::align_val_t{64}) : ::operator new[](128,std::align_val_t{64});critical_thread=false;if(!p||critical_allocations.load()==0)return 6;if(kind==0)::operator delete(p);else if(kind==1)::operator delete[](p);else if(kind==2)::operator delete(p,std::align_val_t{64});else ::operator delete[](p,std::align_val_t{64});}
 critical_c_allocations=0;critical_allocations=0;auto* volatile malloc_call=&std::malloc;void* outside=malloc_call(23);std::free(outside);if(critical_c_allocations||critical_allocations)return 7;return 0;}
