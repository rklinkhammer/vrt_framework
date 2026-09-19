#pragma once
#include <vita/core/error.hpp>
#include <vita/core/bytes.hpp>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <utility>

namespace vita::memory {
enum class MemoryDomain : std::uint8_t { cpu, device };
struct BufferSpec {
    std::shared_ptr<void> lifetime;
    std::byte* cpu_address{};
    std::size_t block_size{}, block_count{}, alignment{1};
    MemoryDomain domain{MemoryDomain::cpu};
    std::uintptr_t device_address{};
    void (*on_return)(void*, std::size_t) noexcept{};
    void* callback_context{};
};
struct BufferRequest {
    std::size_t capacity{}, alignment{1};
    MemoryDomain domain{MemoryDomain::cpu};
    bool cpu_access{true};
};
namespace detail {
struct Block {
    std::shared_ptr<void> lifetime;
    std::byte* address{};
    std::size_t capacity{}, alignment{}, references{};
    MemoryDomain domain{};
    std::uintptr_t device_address{};
    void (*on_return)(void*, std::size_t) noexcept{};
    void* context{};
};
struct PoolState {
    std::mutex mutex;
    std::unique_ptr<Block[]> blocks;
    std::size_t count{}, returns{};
};
inline bool power_of_two(std::size_t v) noexcept { return v && !(v & (v-1)); }
}
class ExternalPool;
class RxEnvelope;
class RetainedRx;
class BufferLease {
    std::shared_ptr<detail::PoolState> state_;
    std::size_t index_{}, used_{};
    BufferLease(std::shared_ptr<detail::PoolState> s, std::size_t i) noexcept : state_(std::move(s)), index_(i) {}
    friend class ExternalPool;
    friend class RxEnvelope;
    friend class RetainedRx;
    BufferLease share() const noexcept {
        std::lock_guard lock(state_->mutex);
        ++state_->blocks[index_].references;
        BufferLease out(state_,index_); out.used_=used_; return out;
    }
public:
    BufferLease() noexcept = default;
    BufferLease(BufferLease const&)=delete;
    BufferLease& operator=(BufferLease const&)=delete;
    BufferLease(BufferLease&& other) noexcept : state_(std::move(other.state_)), index_(other.index_), used_(other.used_) {}
    BufferLease& operator=(BufferLease&& other) noexcept {
        if(this!=&other) { reset(); state_=std::move(other.state_); index_=other.index_; used_=other.used_; } return *this;
    }
    ~BufferLease() { reset(); }
    void reset() noexcept {
        if(!state_) return;
        auto state=std::move(state_);
        auto& b=state->blocks[index_];
        bool last=false;
        {
            std::lock_guard lock(state->mutex);
            if(--b.references==0) { b.references=std::numeric_limits<std::size_t>::max(); last=true; }
        }
        // Reclamation hook runs unlocked. The block stays unavailable until it finishes.
        if(last) {
            if(b.on_return) b.on_return(b.context,index_);
            std::lock_guard lock(state->mutex); b.references=0; ++state->returns;
        }
        used_=0;
    }
    explicit operator bool() const noexcept { return bool(state_); }
    std::size_t capacity() const noexcept { return state_ ? state_->blocks[index_].capacity : 0; }
    std::size_t size() const noexcept { return used_; }
    std::size_t alignment() const noexcept { return state_ ? state_->blocks[index_].alignment : 0; }
    MemoryDomain domain() const noexcept { return state_ ? state_->blocks[index_].domain : MemoryDomain::cpu; }
    std::uintptr_t device_address() const noexcept { return state_ ? state_->blocks[index_].device_address : 0; }
    Result<void> set_size(std::size_t n) noexcept {
        if(!state_ || n>capacity()) return std::unexpected(Error{ErrorCode::invalid_argument});
        used_=n; return {};
    }
    Result<MutableBytes> writable_bytes() noexcept {
        if(!state_) return std::unexpected(Error{ErrorCode::invalid_state});
        auto& b=state_->blocks[index_];
        if(!b.address) return std::unexpected(Error{ErrorCode::no_cpu_access});
        return MutableBytes{b.address,b.capacity};
    }
    Result<Bytes> bytes() const noexcept {
        if(!state_) return std::unexpected(Error{ErrorCode::invalid_state});
        auto const& b=state_->blocks[index_];
        if(!b.address) return std::unexpected(Error{ErrorCode::no_cpu_access});
        return Bytes{b.address,used_};
    }
};
class ExternalPool {
    std::shared_ptr<detail::PoolState> state_;
    explicit ExternalPool(std::shared_ptr<detail::PoolState> s) noexcept : state_(std::move(s)) {}
public:
    ExternalPool() noexcept=default;
    // Configuration-only allocation. The supplied lifetime owns every block and callback context.
    static Result<ExternalPool> create(std::span<BufferSpec const> specs) {
        std::size_t count=0;
        for(auto const& s:specs) {
            if(!s.lifetime || !s.block_size || !s.block_count || !detail::power_of_two(s.alignment) || s.block_size%s.alignment ||
               (s.domain==MemoryDomain::cpu && !s.cpu_address) ||
               (s.cpu_address && reinterpret_cast<std::uintptr_t>(s.cpu_address)%s.alignment))
                return std::unexpected(Error{ErrorCode::invalid_argument});
            if(s.block_count>std::numeric_limits<std::size_t>::max()/s.block_size ||
               s.block_count>std::numeric_limits<std::size_t>::max()-count)
                return std::unexpected(Error{ErrorCode::overflow});
            auto extent=s.block_count*s.block_size;
            if(s.device_address && extent>std::numeric_limits<std::uintptr_t>::max()-s.device_address)
                return std::unexpected(Error{ErrorCode::overflow});
            count+=s.block_count;
        }
        if(!count || count>std::numeric_limits<std::size_t>::max()/sizeof(detail::Block))
            return std::unexpected(Error{ErrorCode::invalid_argument});
        auto state=std::make_shared<detail::PoolState>();
        state->blocks=std::make_unique<detail::Block[]>(count); state->count=count;
        std::size_t k=0;
        for(auto const& s:specs) for(std::size_t i=0;i<s.block_count;++i) {
            auto& b=state->blocks[k++]; b.lifetime=s.lifetime; b.address=s.cpu_address ? s.cpu_address+i*s.block_size : nullptr;
            b.capacity=s.block_size; b.alignment=s.alignment; b.domain=s.domain;
            b.device_address=s.device_address ? s.device_address+i*s.block_size : 0;
            b.on_return=s.on_return; b.context=s.callback_context;
        }
        return ExternalPool(std::move(state));
    }
    Result<BufferLease> acquire(BufferRequest request) noexcept {
        if(!state_ || !request.capacity || !detail::power_of_two(request.alignment))
            return std::unexpected(Error{ErrorCode::invalid_argument});
        std::lock_guard lock(state_->mutex);
        std::size_t selected=state_->count;
        for(std::size_t i=0;i<state_->count;++i) {
            auto const& b=state_->blocks[i];
            if(!b.references && b.capacity>=request.capacity && b.alignment>=request.alignment && b.domain==request.domain && (!request.cpu_access || b.address) &&
               (selected==state_->count || b.capacity<state_->blocks[selected].capacity)) selected=i;
        }
        if(selected==state_->count) return std::unexpected(Error{ErrorCode::capacity_exhausted});
        state_->blocks[selected].references=1; return BufferLease(state_,selected);
    }
    bool supports(BufferRequest request) const noexcept {
        if(!state_||!request.capacity||!detail::power_of_two(request.alignment))return false;
        for(std::size_t i=0;i<state_->count;++i){const auto& b=state_->blocks[i];if(b.capacity>=request.capacity&&b.alignment>=request.alignment&&b.domain==request.domain&&(!request.cpu_access||b.address))return true;}return false;
    }
    std::size_t return_count() const noexcept { if(!state_) return 0; std::lock_guard lock(state_->mutex); return state_->returns; }
    bool shares_provider_with(ExternalPool const& other) const noexcept { return state_ && state_==other.state_; }
    std::size_t block_count() const noexcept { return state_ ? state_->count : 0; }
    Result<std::size_t> raw_bytes() const noexcept {
        std::size_t total=0;if(state_)for(std::size_t i=0;i<state_->count;++i){if(state_->blocks[i].capacity>SIZE_MAX-total)return std::unexpected(Error{ErrorCode::overflow});total+=state_->blocks[i].capacity;}return total;
    }
    std::size_t metadata_bytes() const noexcept { return state_ ? sizeof(detail::PoolState)+state_->count*sizeof(detail::Block) : 0; }
};
} // namespace vita::memory
