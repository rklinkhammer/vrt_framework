#pragma once
#include <vita/core/error.hpp>
#include <array>
#include <cstddef>
#include <mutex>
namespace vita::runtime {
struct Task { void (*invoke)(void*) noexcept{}; void* context{}; };
namespace detail { struct DomainFrame { void const* domain; DomainFrame const* parent; }; inline thread_local DomainFrame const* current_frame=nullptr; }
inline Result<void> check_blocking_wait(void const* target_domain) noexcept {
    for(auto frame=detail::current_frame; target_domain && frame; frame=frame->parent)
        if(frame->domain==target_domain) return std::unexpected(Error{ErrorCode::would_deadlock});
    return {};
}
template<std::size_t N> class BoundedExecutor {
    static_assert(N>0);
    std::array<Task,N> queue_{};
    mutable std::mutex mutex_;
    std::size_t head_{},size_{};
    bool running_{},open_{true};
public:
    BoundedExecutor() noexcept=default;
    BoundedExecutor(BoundedExecutor const&)=delete;
    BoundedExecutor& operator=(BoundedExecutor const&)=delete;
    void const* domain() const noexcept { return this; }
    Result<void> post(Task task) noexcept {
        if(!task.invoke) return std::unexpected(Error{ErrorCode::invalid_argument});
        std::lock_guard lock(mutex_);
        if(!open_) return std::unexpected(Error{ErrorCode::invalid_state});
        if(size_==N) return std::unexpected(Error{ErrorCode::capacity_exhausted});
        queue_[(head_+size_)%N]=task; ++size_; return {};
    }
    bool run_one() noexcept {
        Task task;
        { std::lock_guard lock(mutex_); if(running_ || !size_) return false; running_=true; task=queue_[head_]; head_=(head_+1)%N; --size_; }
        detail::DomainFrame frame{this,detail::current_frame}; detail::current_frame=&frame;
        task.invoke(task.context); detail::current_frame=frame.parent;
        { std::lock_guard lock(mutex_); running_=false; } return true;
    }
    std::size_t run(std::size_t budget=N) noexcept { std::size_t count=0; while(count<budget && run_one()) ++count; return count; }
    void close() noexcept { std::lock_guard lock(mutex_); open_=false; }
    std::size_t pending() const noexcept { std::lock_guard lock(mutex_); return size_; }
    static constexpr std::size_t capacity() noexcept { return N; }
};
// A caller-driven strand is an independent serialized executor domain. Multiple callers
// may attempt progress, but only one callback runs; callbacks can enqueue without reentry.
template<std::size_t N> using ControlStrand=BoundedExecutor<N>;
} // namespace vita::runtime
