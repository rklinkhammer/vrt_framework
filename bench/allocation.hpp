#pragma once
#include <atomic>
namespace vita::bench {
inline thread_local bool critical_thread=false;
inline std::atomic<unsigned long long> critical_allocations{0};
inline std::atomic<unsigned long long> critical_c_allocations{0};
}
