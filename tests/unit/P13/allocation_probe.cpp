#include "allocation.hpp"
#include <cstdlib>
#include <cstdio>
int main(){vita::bench::critical_thread=true;void*(*volatile m)(size_t)=malloc;void*(*volatile c)(size_t,size_t)=calloc;void*(*volatile r)(void*,size_t)=realloc;int(*volatile p)(void**,size_t,size_t)=posix_memalign;void*(*volatile a)(size_t,size_t)=aligned_alloc;auto* x=m(8);auto* y=c(2,8);x=r(x,64);void* z=nullptr;p(&z,64,64);auto* w=a(64,64);vita::bench::critical_thread=false;free(x);free(y);free(z);free(w);printf("%llu\n",vita::bench::critical_c_allocations.load());return vita::bench::critical_c_allocations<5;}
