#pragma once
#include <vita/runtime/public/config.hpp>
#include <array>
#include <new>

namespace vita::profiles::iq::lab {
// Explicit isolated-test setup. The caller chooses its fixture OUI; this helper
// does not provision production identities or claim GPS/device qualification.
inline Result<RuntimeConfig> config(std::uint32_t fixture_oui) noexcept {
    if (fixture_oui > 0xffffff)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    RuntimeConfig result;
    result.oui = fixture_oui;
    result.isolated_lab = true;
    result.clock.epoch = runtime::timing::Epoch::gps;
    result.clock.epoch_configured = true;
    result.clock.injected = true;
    result.clock.qualified = false;
    result.clock.calibration_uncertainty_ps = 0;
    result.clock.holdover_drift_ppb = 0;
    result.clock.holdover_limit_ns = 2'000'000'000;
    result.timing = runtime::timing::TimingCapabilities::deterministic();
    return result;
}

struct PoolCounts {
    std::size_t header = 32, payload = 32, trailer = 32;
    std::size_t control = 32, cancellation = 32, emergency = 32;
    std::size_t rx_data = 32, rx_control = 32, rx_cancellation = 32;
    std::size_t payload_bytes = 2048, rx_data_bytes = 2048;
    std::size_t large = 0, large_bytes = 8192;
    std::size_t max_setup_bytes = runtime::framework_budget;
};
inline PoolCounts reference_counts() noexcept {
    PoolCounts counts;
    counts.header=4096; counts.trailer=1024; counts.payload=8192;
    counts.control=1728; counts.cancellation=64; counts.emergency=256;
    counts.rx_data=3584; counts.rx_control=448; counts.rx_cancellation=64;
    counts.large=128;
    return counts;
}
struct PoolMemory {
    std::size_t raw_bytes = 0;
    std::size_t provider_metadata_bytes = 0;
    std::size_t pool_frontend_bytes = sizeof(ExternalPools);
};
namespace detail {
struct Role { std::size_t block_size, count; };
inline std::array<Role, 10> roles(const PoolCounts& counts) noexcept {
    return {{{128, counts.header}, {counts.payload_bytes, counts.payload},
             {64, counts.trailer}, {2048, counts.control},
             {2048, counts.cancellation}, {counts.rx_data_bytes, counts.rx_data},
             {2048, counts.rx_control}, {2048, counts.rx_cancellation},
             {2048, counts.emergency}, {counts.large_bytes, counts.large}}};
}
inline Result<std::size_t> add(std::size_t a, std::size_t b) noexcept {
    if (b > SIZE_MAX - a) return std::unexpected(Error{ErrorCode::overflow});
    return a + b;
}
inline Result<std::size_t> multiply(std::size_t a, std::size_t b) noexcept {
    if (b && a > SIZE_MAX / b) return std::unexpected(Error{ErrorCode::overflow});
    return a * b;
}
inline Result<memory::ExternalPool> allocate(Role role) {
    const auto bytes = role.block_size * role.count; // checked by measure first
    auto* raw = static_cast<std::byte*>(::operator new(bytes, std::align_val_t{64}, std::nothrow));
    if (!raw) return std::unexpected(Error{ErrorCode::capacity_exhausted});
    std::shared_ptr<void> backing(raw, [](void* pointer) noexcept {
        ::operator delete(pointer, std::align_val_t{64});
    });
    memory::BufferSpec specification{backing, raw, role.block_size, role.count, 64};
    return memory::ExternalPool::create(std::span(&specification, 1));
}
} // namespace detail

// Counts and total extents are checked before the factory allocates any role.
// Shared_ptr/allocator infrastructure is excluded under the architecture's
// process-infrastructure rule; raw bytes and in-arena objects are exact.
inline Result<PoolMemory> measure(const PoolCounts& counts = {}) noexcept {
    PoolMemory result;
    const auto roles = detail::roles(counts);
    for (std::size_t index=0; index<roles.size(); ++index) {
        const auto role=roles[index];
        if(index==roles.size()-1 && !role.count) continue;
        if (!role.count || !role.block_size || role.block_size % 64)
            return std::unexpected(Error{ErrorCode::invalid_argument});
        auto raw = detail::multiply(role.block_size, role.count);
        auto blocks = detail::multiply(sizeof(memory::detail::Block), role.count);
        if (!raw) return std::unexpected(raw.error());
        if (!blocks) return std::unexpected(blocks.error());
        auto metadata = detail::add(sizeof(memory::detail::PoolState), *blocks);
        if (!metadata) return std::unexpected(metadata.error());
        auto total_raw = detail::add(result.raw_bytes, *raw);
        auto total_metadata = detail::add(result.provider_metadata_bytes, *metadata);
        if (!total_raw) return std::unexpected(total_raw.error());
        if (!total_metadata) return std::unexpected(total_metadata.error());
        result.raw_bytes = *total_raw;
        result.provider_metadata_bytes = *total_metadata;
    }
    auto total = detail::add(result.raw_bytes, result.provider_metadata_bytes);
    if (!total) return std::unexpected(total.error());
    total = detail::add(*total, result.pool_frontend_bytes);
    if (!total) return std::unexpected(total.error());
    if (*total > counts.max_setup_bytes)
        return std::unexpected(Error{ErrorCode::capacity_exhausted});
    return result;
}

inline Result<ExternalPools> pools(const PoolCounts& counts = {}) {
    auto checked = measure(counts);
    if (!checked) return std::unexpected(checked.error());
    ExternalPools result;
    const std::array destinations{
        &result.header, &result.payload, &result.trailer, &result.control,
        &result.cancellation, &result.rx_data, &result.rx_control, &result.rx_cancellation,
        &result.emergency, &result.large
    };
    const auto specifications = detail::roles(counts);
    for (std::size_t i = 0; i < destinations.size(); ++i) {
        if(!specifications[i].count) continue;
        auto pool = detail::allocate(specifications[i]);
        if (!pool) return std::unexpected(pool.error());
        *destinations[i] = std::move(*pool);
    }
    return result;
}
} // namespace vita::profiles::iq::lab
