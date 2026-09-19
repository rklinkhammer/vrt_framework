#include <vita/memory/pool.hpp>
#include <array>
#include <atomic>
#include <limits>
#include <memory>
#include <thread>
#include <type_traits>
using namespace vita;
using namespace vita::memory;
struct Backing {
    alignas(64) std::array<std::byte, 256> bytes{};
    std::atomic<unsigned> returns{0};
};
static void returned(void* p, std::size_t) noexcept {
    static_cast<Backing*>(p)->returns.fetch_add(1, std::memory_order_relaxed);
}
static_assert(!std::is_copy_constructible_v<BufferLease>);
static_assert(std::is_nothrow_move_constructible_v<BufferLease>);
int main() {
    auto backing = std::make_shared<Backing>();
    BufferSpec spec{backing, backing->bytes.data(), 64, 4, 64,
                    MemoryDomain::cpu, 0, returned, backing.get()};
    auto result = ExternalPool::create(std::span{&spec, 1});
    if (!result || result->block_count() != 4) return 1;
    auto& pool = *result;
    if(!pool.shares_provider_with(pool) || pool.shares_provider_with(ExternalPool{}))return 14;
    {
        auto lease = pool.acquire({63,64});
        if (!lease || lease->capacity() != 64 || lease->alignment() != 64) return 2;
        if (lease->set_size(65) || lease->size() != 0 || !lease->set_size(63)) return 3;
        if (lease->bytes()->size() != 63 || lease->writable_bytes()->size() != 64) return 4;
        auto moved = std::move(*lease);
        if (*lease || !moved || moved.size() != 63) return 5;
        moved.reset(); moved.reset();
        if (backing->returns != 1 || pool.return_count() != 1) return 6;
    }
    if (pool.acquire({65,64}) || pool.acquire({64,128}) || pool.acquire({1,3})) return 7;
    std::atomic<unsigned> acquired{0};
    auto run = [&] {
        for (unsigned n = 0; n < 1000; ++n) {
            auto lease = pool.acquire({64,64});
            if (lease) acquired.fetch_add(1, std::memory_order_relaxed);
        }
    };
    std::thread a(run), b(run); a.join(); b.join();
    if (acquired != 2000 || backing->returns != 2001 || pool.return_count() != 2001) return 8;
    std::weak_ptr<Backing> weak = backing;
    auto surviving = pool.acquire({64,64});
    spec.lifetime.reset(); backing.reset();
    result = ExternalPool{};
    if (weak.expired() || !surviving || !surviving->bytes()) return 9;
    surviving->reset();
    if (!weak.expired()) return 10;
    auto device_owner = std::make_shared<unsigned>(0);
    BufferSpec device{device_owner,nullptr,64,1,64,MemoryDomain::device,0x1000};
    auto device_pool = ExternalPool::create(std::span{&device,1});
    if (!device_pool || device_pool->acquire({64,64,MemoryDomain::device,true})) return 11;
    auto device_lease = device_pool->acquire({64,64,MemoryDomain::device,false});
    if (!device_lease || device_lease->device_address()!=0x1000 || device_lease->bytes() || device_lease->writable_bytes()) return 12;
    device.block_count=std::numeric_limits<std::size_t>::max();
    auto overflow = ExternalPool::create(std::span{&device,1});
    if (overflow || overflow.error().code != ErrorCode::overflow) return 13;
    return 0;
}
