#pragma once
#include <vita/core/error.hpp>
#include <vita/memory/envelope.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace vita::runtime {
enum class CompletionStatus : std::uint8_t { succeeded, failed, cancelled, abandoned };
struct CompletionResult {
    CompletionStatus status{CompletionStatus::succeeded};
    Error error{ErrorCode::invalid_state};
    std::uint64_t value{}, effective_monotonic_ns{};
};
struct CompletionRecord { std::uint64_t operation{}; CompletionResult result{}; };
enum class TicketState : std::uint8_t { free, reserved, writing, ready, reading, retired };
inline constexpr std::uint64_t max_ticket_generation=(std::uint64_t{1}<<56)-1;
namespace detail {
inline constexpr std::uint64_t tag(std::uint64_t g,TicketState s) noexcept { return (g<<8)|static_cast<std::uint8_t>(s); }
struct CompletionSlot { std::atomic<std::uint64_t> tagged{}; std::uint64_t operation{}; CompletionResult payload{}; };
struct CompletionState {
    std::unique_ptr<CompletionSlot[]> slots;
    std::size_t count{};
    std::mutex allocation_mutex;
    std::atomic<std::uint64_t> rejected_publications{0};
    CompletionState(std::size_t n,std::uint64_t initial) : slots(std::make_unique<CompletionSlot[]>(n)),count(n) {
        for(std::size_t i=0;i<n;++i) slots[i].tagged.store(tag(initial,TicketState::free),std::memory_order_relaxed);
    }
};
}
class CompletionPublisher;
class CompletionWriter {
    std::shared_ptr<detail::CompletionState> state_;
    std::size_t slot_{};
    std::uint64_t generation_{};
    CompletionWriter(std::shared_ptr<detail::CompletionState> state,std::size_t slot,std::uint64_t generation) noexcept : state_(std::move(state)),slot_(slot),generation_(generation) {}
    friend class CompletionPublisher;
public:
    CompletionWriter(CompletionWriter const&)=delete;
    CompletionWriter& operator=(CompletionWriter const&)=delete;
    CompletionWriter(CompletionWriter&&)=default;
    CompletionWriter& operator=(CompletionWriter&&)=delete;
    // Abandoning a claimed writer leaves writing, never steals or republishes partial data.
    bool finish(CompletionResult result) noexcept {
        if(!state_) return false;
        auto& slot=state_->slots[slot_]; slot.payload=result;
        slot.tagged.store(detail::tag(generation_,TicketState::ready),std::memory_order_release);
        state_.reset(); return true;
    }
};
class CompletionPublisher {
    std::shared_ptr<detail::CompletionState> state_;
    std::size_t slot_{};
    std::uint64_t generation_{};
    template<std::size_t> friend class CompletionArena;
    friend class CompletionToken;
    CompletionPublisher(std::shared_ptr<detail::CompletionState> state,std::size_t slot,std::uint64_t generation) noexcept : state_(std::move(state)),slot_(slot),generation_(generation) {}
public:
    CompletionPublisher() noexcept=default;
    bool is_reserved() const noexcept {
        return state_ && state_->slots[slot_].tagged.load(std::memory_order_acquire)==detail::tag(generation_,TicketState::reserved);
    }
    std::size_t slot() const noexcept { return slot_; }
    std::uint64_t generation() const noexcept { return generation_; }
    std::optional<CompletionWriter> try_claim() const noexcept {
        if(!state_) return {};
        auto expected=detail::tag(generation_,TicketState::reserved);
        if(!state_->slots[slot_].tagged.compare_exchange_strong(expected,detail::tag(generation_,TicketState::writing),std::memory_order_acq_rel,std::memory_order_relaxed)) {
            state_->rejected_publications.fetch_add(1,std::memory_order_relaxed); return {};
        }
        return CompletionWriter(state_,slot_,generation_);
    }
    bool publish(CompletionResult result) const noexcept { auto writer=try_claim(); return writer && writer->finish(result); }
};
class CompletionToken {
    CompletionPublisher publisher_;
    bool active_{};
    template<std::size_t> friend class CompletionArena;
    explicit CompletionToken(CompletionPublisher p) noexcept : publisher_(std::move(p)),active_(true) {}
    void abandon() noexcept {
        if(active_) { CompletionResult r; r.status=CompletionStatus::abandoned; r.error=Error{ErrorCode::callback_failure}; publisher_.publish(r); active_=false; }
    }
public:
    CompletionToken() noexcept=default;
    CompletionToken(CompletionToken const&)=delete;
    CompletionToken& operator=(CompletionToken const&)=delete;
    CompletionToken(CompletionToken&& other) noexcept : publisher_(std::move(other.publisher_)),active_(std::exchange(other.active_,false)) {}
    CompletionToken& operator=(CompletionToken&& other) noexcept { if(this!=&other) { abandon(); publisher_=std::move(other.publisher_); active_=std::exchange(other.active_,false); } return *this; }
    ~CompletionToken() { abandon(); }
    // Ownership/preflight observations; not an atomic transport handoff barrier.
    bool active() const noexcept { return active_; }
    bool is_reserved() const noexcept { return active_ && publisher_.is_reserved(); }
    CompletionPublisher publisher() const noexcept { return publisher_; }
    bool publish(CompletionResult result) noexcept { if(!active_) return false; active_=false; return publisher_.publish(result); }
};
template<std::size_t N> class CompletionArena {
    static_assert(N>0);
    std::shared_ptr<detail::CompletionState> state_;
public:
    // Initial generation is a configuration input, including max for retirement verification.
    explicit CompletionArena(std::uint64_t initial_generation=1) : state_(std::make_shared<detail::CompletionState>(N,
      initial_generation<=max_ticket_generation ? initial_generation : max_ticket_generation)) {}
    Result<CompletionToken> reserve(std::uint64_t operation) noexcept {
        std::lock_guard lock(state_->allocation_mutex);
        for(std::size_t i=0;i<N;++i) {
            auto tagged=state_->slots[i].tagged.load(std::memory_order_acquire);
            if(static_cast<TicketState>(tagged&255)!=TicketState::free) continue;
            auto generation=tagged>>8; state_->slots[i].operation=operation;
            state_->slots[i].tagged.store(detail::tag(generation,TicketState::reserved),std::memory_order_release);
            return CompletionToken(CompletionPublisher(state_,i,generation));
        }
        return std::unexpected(Error{ErrorCode::capacity_exhausted});
    }
    std::optional<CompletionRecord> consume(std::size_t index) noexcept {
        if(index>=N) return {};
        auto& slot=state_->slots[index]; auto expected=slot.tagged.load(std::memory_order_acquire);
        if(static_cast<TicketState>(expected&255)!=TicketState::ready) return {};
        auto generation=expected>>8;
        if(!slot.tagged.compare_exchange_strong(expected,detail::tag(generation,TicketState::reading),std::memory_order_acquire,std::memory_order_relaxed)) return {};
        CompletionRecord result{slot.operation,slot.payload}; slot.payload={};
        slot.tagged.store(generation==max_ticket_generation ? detail::tag(generation,TicketState::retired) : detail::tag(generation+1,TicketState::free),std::memory_order_release);
        return result;
    }
    template<class F> std::size_t scan(F&& consume_record) noexcept {
        static_assert(noexcept(consume_record(std::declval<CompletionRecord>())));
        std::size_t count=0; for(std::size_t i=0;i<N;++i) if(auto record=consume(i)) { consume_record(*record); ++count; } return count;
    }
    TicketState state(std::size_t index) const noexcept { return index<N ? static_cast<TicketState>(state_->slots[index].tagged.load(std::memory_order_acquire)&255) : TicketState::retired; }
    std::uint64_t rejected_publications() const noexcept { return state_->rejected_publications.load(std::memory_order_relaxed); }
    static constexpr std::size_t capacity() noexcept { return N; }
    static constexpr std::size_t slot_bytes() noexcept { return sizeof(detail::CompletionSlot); }
    static constexpr std::size_t metadata_bytes() noexcept { return sizeof(detail::CompletionState)+N*sizeof(detail::CompletionSlot); }
};
// Quiescence has a distinct lifetime from transaction outcome. Setup one guard per admitted operation.
class QuiescenceGuard {
    struct State {
        std::mutex mutex;
        std::optional<memory::TxStorage> storage;
        std::shared_ptr<State> quarantine;
        std::uint64_t generation{};
    };
    std::shared_ptr<State> state_;
    std::uint64_t generation_{};
public:
    QuiescenceGuard() : state_(std::make_shared<State>()) {}
    Result<void> arm(memory::TxStorage&& storage) noexcept {
        std::lock_guard lock(state_->mutex);
        if(state_->storage) return std::unexpected(Error{ErrorCode::invalid_state});
        if(state_->generation==std::numeric_limits<std::uint64_t>::max()) return std::unexpected(Error{ErrorCode::overflow});
        generation_=++state_->generation;
        state_->storage.emplace(std::move(storage)); state_->quarantine=state_; return {};
    }
    bool retained() const noexcept { std::lock_guard lock(state_->mutex); return state_->storage.has_value(); }
    bool prove_quiescent() noexcept {
        std::optional<memory::TxStorage> released;
        { std::lock_guard lock(state_->mutex);
          if(generation_!=state_->generation || !state_->storage) return false;
          released=std::move(state_->storage); state_->storage.reset(); state_->quarantine.reset(); }
        // Provider callbacks run after dropping the guard lock. Copy the armed guard
        // into each callback: older-generation evidence cannot release a later operation.
        return true;
    }
    static constexpr std::size_t metadata_bytes() noexcept { return sizeof(State); }
};
static_assert(sizeof(detail::CompletionSlot)<=512);
} // namespace vita::runtime
