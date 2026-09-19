#include "allocation.hpp"
#include <cstdlib>
#include <new>
namespace {void observed() noexcept {if(vita::bench::critical_thread)vita::bench::critical_allocations.fetch_add(1,std::memory_order_relaxed);}void* allocate(std::size_t n){observed();if(auto* p=std::malloc(n?n:1))return p;std::abort();}void* aligned(std::size_t n,std::size_t a){observed();void* p=nullptr;if(posix_memalign(&p,a,n?n:a)==0)return p;std::abort();}}
void* operator new(std::size_t n){return allocate(n);}void* operator new[](std::size_t n){return allocate(n);}
void operator delete(void* p) noexcept{std::free(p);}void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
void* operator new(std::size_t n,std::align_val_t a){return aligned(n,std::size_t(a));}void* operator new[](std::size_t n,std::align_val_t a){return aligned(n,std::size_t(a));}
void operator delete(void* p,std::align_val_t) noexcept{std::free(p);}void operator delete[](void* p,std::align_val_t) noexcept{std::free(p);}
void operator delete(void* p,std::size_t,std::align_val_t) noexcept{std::free(p);}void operator delete[](void* p,std::size_t,std::align_val_t) noexcept{std::free(p);}

// Mach-O interposition observes C allocation calls made by linked code and system
// libraries. References from this image to the original symbols remain original.
#if defined(__APPLE__)
namespace {
void c_observed() noexcept {if(vita::bench::critical_thread)vita::bench::critical_c_allocations.fetch_add(1,std::memory_order_relaxed);}
void* tracked_malloc(std::size_t n){c_observed();return std::malloc(n);}
void* tracked_calloc(std::size_t n,std::size_t size){c_observed();return std::calloc(n,size);}
void* tracked_realloc(void* p,std::size_t n){c_observed();return std::realloc(p,n);}
int tracked_posix_memalign(void** p,std::size_t alignment,std::size_t n){c_observed();return posix_memalign(p,alignment,n);}
void* tracked_aligned_alloc(std::size_t alignment,std::size_t n){c_observed();return std::aligned_alloc(alignment,n);}
#define VITA_INTERPOSE(replacement,original) \
  __attribute__((used)) static struct {const void* replacement_address;const void* original_address;} \
  interpose_##original __attribute__((section("__DATA,__interpose"))) = { \
  reinterpret_cast<const void*>(&replacement),reinterpret_cast<const void*>(&original)};
VITA_INTERPOSE(tracked_malloc,malloc)
VITA_INTERPOSE(tracked_calloc,calloc)
VITA_INTERPOSE(tracked_realloc,realloc)
VITA_INTERPOSE(tracked_posix_memalign,posix_memalign)
VITA_INTERPOSE(tracked_aligned_alloc,aligned_alloc)
}
#endif
