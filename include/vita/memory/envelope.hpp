#pragma once
#include <vita/memory/pool.hpp>
#include <array>
#include <functional>

namespace vita::memory {
struct SegmentRef { std::size_t lease{}, offset{}, length{}; };
namespace detail {
struct QuotaState { std::atomic<std::size_t> active{0}; std::size_t limit{}; std::atomic<bool> enabled{true}; };
inline bool take_quota(std::shared_ptr<QuotaState> const& s) noexcept {
    if(!s || !s->enabled.load(std::memory_order_acquire)) return false;
    auto n=s->active.load(std::memory_order_relaxed);
    while(n<s->limit) if(s->active.compare_exchange_weak(n,n+1,std::memory_order_acq_rel,std::memory_order_relaxed)) return true;
    return false;
}
}
class RetentionQuota {
    std::shared_ptr<detail::QuotaState> state_;
    friend class RxEnvelope;
    friend class RetainedRx;
public:
    explicit RetentionQuota(std::size_t limit) : state_(std::make_shared<detail::QuotaState>()) { state_->limit=limit; }
    std::size_t active() const noexcept { return state_->active.load(std::memory_order_relaxed); }
    void enable(bool value) noexcept { state_->enabled.store(value,std::memory_order_release); }
};
class RetainedRx {
    std::array<BufferLease,16> leases_{};
    std::array<SegmentRef,16> fragments_{};
    std::size_t lease_count_{}, fragment_count_{};
    std::shared_ptr<detail::QuotaState> consumer_, global_;
    friend class RxEnvelope;
    void release_quotas() noexcept {
        if(consumer_) consumer_->active.fetch_sub(1,std::memory_order_acq_rel);
        if(global_ && global_!=consumer_) global_->active.fetch_sub(1,std::memory_order_acq_rel);
        consumer_.reset(); global_.reset();
    }
public:
    RetainedRx() noexcept=default;
    RetainedRx(RetainedRx const&)=delete;
    RetainedRx& operator=(RetainedRx const&)=delete;
    RetainedRx(RetainedRx&& other) noexcept : leases_(std::move(other.leases_)), fragments_(other.fragments_),
        lease_count_(std::exchange(other.lease_count_,0)), fragment_count_(std::exchange(other.fragment_count_,0)),
        consumer_(std::move(other.consumer_)), global_(std::move(other.global_)) {}
    RetainedRx& operator=(RetainedRx&& other) noexcept {
        if(this!=&other) { reset(); leases_=std::move(other.leases_); fragments_=other.fragments_;
            lease_count_=std::exchange(other.lease_count_,0); fragment_count_=std::exchange(other.fragment_count_,0);
            consumer_=std::move(other.consumer_); global_=std::move(other.global_); } return *this;
    }
    ~RetainedRx() { reset(); }
    void reset() noexcept { for(auto& lease:leases_) lease.reset(); lease_count_=fragment_count_=0; release_quotas(); }
    Result<RetainedRx> retain(RetentionQuota& consumer, RetentionQuota& global) const noexcept {
        if(!fragment_count_) return std::unexpected(Error{ErrorCode::invalid_state});
        if(!detail::take_quota(consumer.state_)) return std::unexpected(Error{ErrorCode::capacity_exhausted});
        if(consumer.state_!=global.state_ && !detail::take_quota(global.state_)) {
            consumer.state_->active.fetch_sub(1,std::memory_order_acq_rel); return std::unexpected(Error{ErrorCode::capacity_exhausted});
        }
        RetainedRx out;out.consumer_=consumer.state_;out.global_=global.state_;
        out.lease_count_=lease_count_;out.fragment_count_=fragment_count_;out.fragments_=fragments_;
        for(std::size_t i=0;i<lease_count_;++i)out.leases_[i]=leases_[i].share();
        return out;
    }
    std::size_t fragment_count() const noexcept { return fragment_count_; }
    Result<Bytes> fragment(std::size_t i) const noexcept {
        if(i>=fragment_count_) return std::unexpected(Error{ErrorCode::invalid_argument});
        auto const& f=fragments_[i]; auto b=leases_[f.lease].bytes(); if(!b) return std::unexpected(b.error());
        return b->subspan(f.offset,f.length);
    }
};
class BorrowedBytes;
class RxEnvelope {
    std::array<BufferLease,18> leases_{};
    std::array<SegmentRef,16> payload_{};
    std::size_t lease_count_{}, fragment_count_{};
    SegmentRef prologue_{}, trailer_{};
    bool has_prologue_{}, has_trailer_{};
    friend class BorrowedBytes;
    bool valid(SegmentRef s) const noexcept {
        return s.lease<lease_count_ && s.offset<=leases_[s.lease].size() && s.length<=leases_[s.lease].size()-s.offset;
    }
public:
    RxEnvelope() noexcept=default;
    RxEnvelope(RxEnvelope const&)=delete;
    RxEnvelope& operator=(RxEnvelope const&)=delete;
    RxEnvelope(RxEnvelope&& other) noexcept : leases_(std::move(other.leases_)),payload_(other.payload_),
      lease_count_(std::exchange(other.lease_count_,0)),fragment_count_(std::exchange(other.fragment_count_,0)),
      prologue_(other.prologue_),trailer_(other.trailer_),has_prologue_(std::exchange(other.has_prologue_,false)),has_trailer_(std::exchange(other.has_trailer_,false)) {}
    RxEnvelope& operator=(RxEnvelope&& other) noexcept {
        if(this!=&other) { leases_=std::move(other.leases_); payload_=other.payload_;
          lease_count_=std::exchange(other.lease_count_,0); fragment_count_=std::exchange(other.fragment_count_,0);
          prologue_=other.prologue_; trailer_=other.trailer_; has_prologue_=std::exchange(other.has_prologue_,false); has_trailer_=std::exchange(other.has_trailer_,false); } return *this;
    }
    Result<std::size_t> add_buffer(BufferLease&& lease) noexcept {
        if(!lease) return std::unexpected(Error{ErrorCode::invalid_argument});
        if(lease_count_==leases_.size()) return std::unexpected(Error{ErrorCode::capacity_exhausted});
        auto index=lease_count_++; leases_[index]=std::move(lease); return index;
    }
    Result<void> append_payload(SegmentRef s) noexcept {
        if(!valid(s) || !s.length) return std::unexpected(Error{ErrorCode::invalid_argument});
        if(fragment_count_==payload_.size()) return std::unexpected(Error{ErrorCode::capacity_exhausted});
        payload_[fragment_count_++]=s; return {};
    }
    Result<void> set_prologue(SegmentRef s) noexcept {
        if(!valid(s)) return std::unexpected(Error{ErrorCode::invalid_argument});
        auto b=leases_[s.lease].bytes(); if(!b) return std::unexpected(b.error());
        prologue_=s; has_prologue_=true; return {};
    }
    Result<void> set_trailer(SegmentRef s) noexcept {
        if(!valid(s)) return std::unexpected(Error{ErrorCode::invalid_argument});
        trailer_=s; has_trailer_=true; return {};
    }
    Result<Bytes> prologue() const noexcept {
        if(!has_prologue_) return std::unexpected(Error{ErrorCode::invalid_state});
        auto b=leases_[prologue_.lease].bytes(); if(!b) return std::unexpected(b.error());
        return b->subspan(prologue_.offset,prologue_.length);
    }
    Result<Bytes> trailer() const noexcept {
        if(!has_trailer_) return std::unexpected(Error{ErrorCode::invalid_state});
        auto b=leases_[trailer_.lease].bytes(); if(!b) return std::unexpected(b.error());
        return b->subspan(trailer_.offset,trailer_.length);
    }
    std::size_t fragment_count() const noexcept { return fragment_count_; }
    Result<Bytes> fragment(std::size_t i) const noexcept {
        if(i>=fragment_count_) return std::unexpected(Error{ErrorCode::invalid_argument});
        auto const& s=payload_[i]; auto b=leases_[s.lease].bytes(); if(!b) return std::unexpected(b.error());
        return b->subspan(s.offset,s.length);
    }
    Result<RetainedRx> retain(RetentionQuota& consumer, RetentionQuota& global) const noexcept {
        if(!fragment_count_) return std::unexpected(Error{ErrorCode::invalid_state});
        if(!detail::take_quota(consumer.state_)) return std::unexpected(Error{ErrorCode::capacity_exhausted});
        if(consumer.state_!=global.state_ && !detail::take_quota(global.state_)) {
            consumer.state_->active.fetch_sub(1,std::memory_order_acq_rel); return std::unexpected(Error{ErrorCode::capacity_exhausted});
        }
        RetainedRx out; out.consumer_=consumer.state_; out.global_=global.state_;
        std::array<std::size_t,18> remap{}; remap.fill(18);
        for(std::size_t i=0;i<fragment_count_;++i) {
            auto s=payload_[i];
            if(remap[s.lease]==18) { remap[s.lease]=out.lease_count_; out.leases_[out.lease_count_++]=leases_[s.lease].share(); }
            s.lease=remap[s.lease]; out.fragments_[out.fragment_count_++]=s;
        }
        return out;
    }
    template<class F> decltype(auto) with_payload(F&& fn) const;
};
class BorrowedBytes {
    RxEnvelope const& envelope_;
    explicit BorrowedBytes(RxEnvelope const& e) noexcept : envelope_(e) {}
    friend class RxEnvelope;
public:
    BorrowedBytes(BorrowedBytes const&)=delete;
    BorrowedBytes& operator=(BorrowedBytes const&)=delete;
    std::size_t fragment_count() const noexcept { return envelope_.fragment_count(); }
    Result<Bytes> fragment(std::size_t i) const noexcept { return envelope_.fragment(i); }
    Result<RetainedRx> retain(RetentionQuota& consumer, RetentionQuota& global) const noexcept { return envelope_.retain(consumer,global); }
};
template<class F> decltype(auto) RxEnvelope::with_payload(F&& fn) const {
    BorrowedBytes borrowed(*this); return std::invoke(std::forward<F>(fn),borrowed);
}
class TxStorage {
    std::array<BufferLease,3> leases_{};
    std::array<SegmentRef,3> segments_{};
    std::size_t count_{};
public:
    TxStorage() noexcept=default;
    TxStorage(TxStorage const&)=delete;
    TxStorage& operator=(TxStorage const&)=delete;
    TxStorage(TxStorage&& other) noexcept : leases_(std::move(other.leases_)),segments_(other.segments_),count_(std::exchange(other.count_,0)) {}
    TxStorage& operator=(TxStorage&& other) noexcept { if(this!=&other) { leases_=std::move(other.leases_); segments_=other.segments_; count_=std::exchange(other.count_,0); } return *this; }
    Result<void> append(BufferLease&& lease,std::size_t offset,std::size_t length) noexcept {
        if(!lease || offset>lease.size() || length>lease.size()-offset || !length) return std::unexpected(Error{ErrorCode::invalid_argument});
        if(count_==3) return std::unexpected(Error{ErrorCode::capacity_exhausted});
        if(length>std::numeric_limits<std::size_t>::max()-byte_size()) return std::unexpected(Error{ErrorCode::overflow});
        segments_[count_]={count_,offset,length}; leases_[count_]=std::move(lease); ++count_; return {};
    }
    std::size_t segment_count() const noexcept { return count_; }
    std::size_t byte_size() const noexcept { std::size_t n=0; for(std::size_t i=0;i<count_;++i) n+=segments_[i].length; return n; }
    Result<Bytes> segment(std::size_t i) const noexcept {
        if(i>=count_) return std::unexpected(Error{ErrorCode::invalid_argument});
        auto const& s=segments_[i]; auto b=leases_[i].bytes(); if(!b) return std::unexpected(b.error()); return b->subspan(s.offset,s.length);
    }
    static Result<TxStorage> acquire(ExternalPool& pool,std::span<BufferRequest const> requests) noexcept {
        if(requests.size()>3) return std::unexpected(Error{ErrorCode::capacity_exhausted});
        TxStorage out;
        for(auto const& request:requests) {
            auto lease=pool.acquire(request); if(!lease) return std::unexpected(lease.error());
            auto sized=lease->set_size(request.capacity); if(!sized) return std::unexpected(sized.error());
            auto appended=out.append(std::move(*lease),0,request.capacity); if(!appended) return std::unexpected(appended.error());
        }
        return out;
    }
};
} // namespace vita::memory
