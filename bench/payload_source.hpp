#pragma once
#include <vita/profiles/iq/source.hpp>
#include <vita/memory/pool.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>
#include <vector>

namespace vita::bench {
enum class PayloadMode { generated, precomputed_copy, prefilled_pool };
inline Result<PayloadMode> payload_mode(std::string_view name) noexcept {
  if(name=="generated")return PayloadMode::generated;
  if(name=="precomputed-copy")return PayloadMode::precomputed_copy;
  if(name=="prefilled-pool")return PayloadMode::prefilled_pool;
  return std::unexpected(Error{ErrorCode::invalid_argument});
}
class PayloadSource {
public:
  static constexpr std::size_t packet_pairs=256,packet_bytes=1024,max_blocks=8192;
private:
  PayloadMode mode_=PayloadMode::generated;
  std::array<std::byte,packet_bytes> canonical_{};
  std::array<std::uintptr_t,max_blocks> blocks_{};
  std::size_t block_count_=0;
  bool initialized_=false;
  // Only the Runtime producer thread changes these after setup; inspect after join.
  std::uint64_t calls_=0,payload_writes_=0,payload_copies_=0;
  Result<void> produce(profiles::iq::SampleWriteWindow& window) noexcept {
    if(!initialized_ || window.format()!=profiles::iq::SampleFormat::iq16 ||
       window.count()!=packet_pairs || window.wire().size()!=packet_bytes ||
       window.first_ordinal()%16)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    if(mode_==PayloadMode::prefilled_pool) {
      const auto address=reinterpret_cast<std::uintptr_t>(window.wire().data());
      if(!std::binary_search(blocks_.begin(),blocks_.begin()+block_count_,address))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    ++calls_;
    if(mode_==PayloadMode::generated) {
      auto produced=profiles::iq::default_source().produce(window);
      if(produced)payload_writes_+=packet_bytes;
      return produced;
    }
    if(mode_==PayloadMode::precomputed_copy) {
      std::memcpy(window.wire().data(),canonical_.data(),canonical_.size());
      payload_writes_+=packet_bytes;++payload_copies_;
    }
    return window.complete_from_wire();
  }
public:
  PayloadSource()=default;
  PayloadSource(const PayloadSource&)=delete;
  PayloadSource& operator=(const PayloadSource&)=delete;
  // Setup only. The caller owns this object and the pool for the whole producer
  // lifetime, and must not mutate prefilled blocks through another producer.
  Result<void> initialize(PayloadMode mode,memory::ExternalPool& pool) {
    if(initialized_)return std::unexpected(Error{ErrorCode::invalid_state});
    if(mode!=PayloadMode::generated&&mode!=PayloadMode::precomputed_copy&&mode!=PayloadMode::prefilled_pool)
      return std::unexpected(Error{ErrorCode::invalid_argument});
    runtime::StateSnapshot state;
    auto window=profiles::iq::SampleWriteWindow::create(canonical_,profiles::iq::SampleFormat::iq16,0,packet_pairs,state);
    if(!window)return std::unexpected(window.error());
    auto filled=profiles::iq::default_source().produce(*window);if(!filled)return filled;
    if(mode==PayloadMode::prefilled_pool) {
      if(!pool.block_count()||pool.block_count()>max_blocks)return std::unexpected(Error{ErrorCode::capacity_exhausted});
      // Holding every lease ensures acquisition visits distinct blocks. There is
      // no acquire/fill/return loop that could initialize the same block repeatedly.
      std::vector<memory::BufferLease> held;held.reserve(pool.block_count());
      for(std::size_t i=0;i<pool.block_count();++i) {
        auto lease=pool.acquire({packet_bytes});
        if(!lease)return std::unexpected(lease.error());
        held.push_back(std::move(*lease));
      }
      for(auto& lease:held) {
        auto wire=lease.writable_bytes();if(!wire)return std::unexpected(wire.error());
        blocks_[block_count_++]=reinterpret_cast<std::uintptr_t>(wire->data());
        std::memcpy(wire->data(),canonical_.data(),packet_bytes);
      }
      std::sort(blocks_.begin(),blocks_.begin()+block_count_);
    }
    mode_=mode;initialized_=true;return {};
  }
  profiles::iq::SourceProvider provider() noexcept {
    return {this,[](void* context,profiles::iq::SampleWriteWindow& window) noexcept {
      return static_cast<PayloadSource*>(context)->produce(window);
    }};
  }
  Bytes canonical() const noexcept{return canonical_;}
  std::size_t setup_blocks() const noexcept{return block_count_;}
  std::uint64_t calls() const noexcept{return calls_;}
  std::uint64_t payload_write_bytes() const noexcept{return payload_writes_;}
  std::uint64_t payload_copy_calls() const noexcept{return payload_copies_;}
};
} // namespace vita::bench
