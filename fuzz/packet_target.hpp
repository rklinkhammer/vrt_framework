#pragma once
#include <vita/codec/packet.hpp>
#include <cstdint>
#include <cstdlib>

namespace vita::fuzz {
inline constexpr std::size_t max_input_bytes = 65536;
inline void require(bool condition) noexcept { if (!condition) std::abort(); }

template<std::size_t Fields,std::size_t Views>
inline void exercise_capacity(Bytes input,codec::DecodeOptions options) noexcept {
  const auto decoded=codec::decode_packet_bounded<Fields,Views>(input,options);
  std::size_t callbacks=0;
  auto visited=codec::decode_and_visit_bounded<Fields,Views>(input,options,
    [&](const codec::FieldView& field) noexcept -> Result<void> {
      require(decoded.has_value());
      require(callbacks<decoded->fields.size());
      const auto& expected=decoded->fields[callbacks++];
      require(field.id==expected.id && field.attribute==expected.attribute && field.kind==expected.kind
              && field.group==expected.group && field.bytes.data()==expected.bytes.data()
              && field.bytes.size()==expected.bytes.size());
      // Selectors deliberately have empty/null byte views.
      if(!field.bytes.empty()) {
        const auto begin=reinterpret_cast<std::uintptr_t>(input.data());
        const auto view=reinterpret_cast<std::uintptr_t>(field.bytes.data());
        require(view>=begin && view-begin<=input.size());
        require(field.bytes.size()<=input.size()-(view-begin));
      }
      if(field.kind==BodyKind::values)(void)field.value();
      if(field.kind==BodyKind::diagnostics)(void)field.diagnostic();
      return {};
    });
  require(bool(visited)==bool(decoded));
  if(!decoded) {
    require(callbacks==0);
    require(visited.error().code==decoded.error().code);
    return;
  }
  require(callbacks==decoded->fields.size());
  std::size_t stopping_callbacks=0;
  auto stopped=codec::decode_and_visit_bounded<Fields,Views>(input,options,
    [&](const codec::FieldView&) noexcept -> Result<void> {
      ++stopping_callbacks;
      return std::unexpected(Error{ErrorCode::callback_failure});
    });
  require(stopping_callbacks<=1);
  if(!decoded->fields.empty())require(!stopped && stopped.error().code==ErrorCode::callback_failure);
  else require(bool(stopped));
  // Callback failure must not poison later parsing or mutate the prior borrowed view.
  const auto after=codec::decode_packet_bounded<Fields,Views>(input,options);
  require(after.has_value() && after->fields.size()==decoded->fields.size());
  require(after->opaque==decoded->opaque && after->requires_request_context==decoded->requires_request_context
          && after->body_kind==decoded->body_kind && after->change==decoded->change);
  for(std::size_t i=0;i<after->fields.size();++i) {
    const auto& a=after->fields[i];const auto& b=decoded->fields[i];
    require(a.id==b.id && a.attribute==b.attribute && a.kind==b.kind && a.group==b.group
            && a.bytes.data()==b.bytes.data() && a.bytes.size()==b.bytes.size());
  }
}
inline void exercise(Bytes input) noexcept {
  if(input.size()>max_input_bytes)return;
  for(unsigned correlated=0;correlated<2;++correlated) {
    codec::DecodeOptions options;
    if(correlated)options.request=codec::RequestContext{0xa91f0000};
    // Explicit CPU/storage resource scope, independent of the enclosing packet's claimed length.
    options.limits.association_entries=1024;
    options.limits.index_entries=1024;
    options.limits.records=256;
    options.limits.work_units=4096;
    exercise_capacity<16,64>(input,options);
    exercise_capacity<128,1664>(input,options);
  }
}
} // namespace vita::fuzz
