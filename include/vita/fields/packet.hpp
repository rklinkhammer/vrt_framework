#pragma once
#include <vita/fields/types.hpp>
#include <array>
#include <optional>
#include <span>
#include <limits>
namespace vita {
enum class PacketFamily : std::uint8_t { signal_data=1, context=4, command=6, extension_data=3, extension_context=5, extension_command=7 };
enum class BodyKind { values, selectors, diagnostics };
enum class PacketSubtype { signal, context, control, query, cancel, diagnostic_ack, state_ack };
struct LayoutContext {
    PacketFamily family = PacketFamily::context;
    PacketSubtype subtype = PacketSubtype::context;
    std::uint8_t action = 0;
    std::uint16_t packet_class = 0;
    std::array<std::uint32_t,8> cif{};
    std::uint32_t attributes = 0; // zero means implicit Current, no CIF7 word
};
struct FieldEntry {
    FieldId id{};
    std::array<std::optional<SemanticValue>,13> values{};
    std::uint32_t diagnostic = 0;
};
struct AttributeValue { FieldId field; Attribute attribute; SemanticValue value; };
template<BodyKind Kind, std::size_t N> class PacketBuilder;
template<BodyKind Kind, std::size_t N = 16> class PacketSnapshot {
    LayoutContext layout_{};
    std::array<FieldEntry,N> entries_{};
    std::size_t size_ = 0;
    std::uint64_t generation_ = 0;
    friend class PacketBuilder<Kind,N>;
public:
    static constexpr BodyKind body_kind = Kind;
    constexpr const LayoutContext& layout() const noexcept { return layout_; }
    constexpr std::span<const FieldEntry> fields() const noexcept { return {entries_.data(),size_}; }
    constexpr std::uint64_t generation() const noexcept { return generation_; }
};
template<BodyKind Kind, std::size_t N = 16> class PacketBuilder {
    PacketSnapshot<Kind,N> state_{};
    constexpr Result<void> bump() noexcept {
        if (state_.generation_ == std::numeric_limits<std::uint64_t>::max()) return std::unexpected(Error{ErrorCode::overflow});
        ++state_.generation_; return {};
    }
    constexpr void indicators() noexcept {
        state_.layout_.cif.fill(0);
        for (auto entry : state_.fields()) {
            state_.layout_.cif[entry.id.cif] |= std::uint32_t{1} << entry.id.bit;
            if (entry.id.cif != 0) state_.layout_.cif[0] |= std::uint32_t{1} << entry.id.cif;
        }
        if (state_.layout_.attributes) { state_.layout_.cif[0] |= 1u << 7; state_.layout_.cif[7] = state_.layout_.attributes; }
    }
    constexpr Result<void> insert(FieldEntry entry, bool replace) noexcept {
        if (!valid_field_selector(entry.id)) return std::unexpected(Error{ErrorCode::invalid_argument});
        auto candidate = *this;
        std::size_t index = 0;
        while (index < candidate.state_.size_ && field_before(candidate.state_.entries_[index].id,entry.id)) ++index;
        const bool found = index < candidate.state_.size_ && candidate.state_.entries_[index].id == entry.id;
        if (replace && !found) return std::unexpected(Error{ErrorCode::invalid_argument});
        if (!found) {
            if (candidate.state_.size_ == N) return std::unexpected(Error{ErrorCode::capacity_exhausted});
            for (auto j=candidate.state_.size_; j>index; --j) candidate.state_.entries_[j]=candidate.state_.entries_[j-1];
            ++candidate.state_.size_;
        }
        candidate.state_.entries_[index]=entry;
        auto result=candidate.bump(); if (!result) return result;
        candidate.indicators(); *this=candidate; return {};
    }
protected:
    explicit constexpr PacketBuilder(PacketSubtype subtype) noexcept {
        state_.layout_.subtype=subtype;
        state_.layout_.action=(subtype==PacketSubtype::control || subtype==PacketSubtype::cancel) ? 2 : 0;
        state_.layout_.family=(subtype==PacketSubtype::context ? PacketFamily::context : subtype==PacketSubtype::signal ? PacketFamily::signal_data : PacketFamily::command);
    }
public:
    constexpr PacketBuilder() noexcept : PacketBuilder(Kind==BodyKind::values ? PacketSubtype::context : Kind==BodyKind::selectors ? PacketSubtype::query : PacketSubtype::diagnostic_ack) {}
    constexpr Result<void> configure(std::uint16_t packet_class,std::uint8_t action) noexcept {
        const auto subtype=state_.layout_.subtype;
        if(action>2 || (subtype==PacketSubtype::query && action!=0) ||
            (subtype==PacketSubtype::cancel && action!=2) ||
            (subtype==PacketSubtype::control && action==0) ||
            ((subtype==PacketSubtype::context || subtype==PacketSubtype::signal) && action!=0))
            return std::unexpected(Error{ErrorCode::invalid_argument});
        auto candidate=*this;auto result=candidate.bump();if(!result)return result;
        candidate.state_.layout_.packet_class=packet_class;candidate.state_.layout_.action=action;
        *this=candidate;return {};
    }
    constexpr PacketSnapshot<Kind,N> freeze() const noexcept { return state_; }
    constexpr std::uint64_t generation() const noexcept { return state_.generation_; }
    template<class Field> constexpr Result<void> set(typename Field::value_type value) noexcept requires (Kind==BodyKind::values) {
        return set_value(Field::id, SemanticValue{value});
    }
    constexpr Result<void> set_value(FieldId id, SemanticValue value, bool replace=false) noexcept requires (Kind==BodyKind::values) {
        auto valid=validate_value(id,value); if (!valid) return valid;
        // Attribute-bearing edits require the complete transactional API below.
        if (state_.layout_.attributes && state_.layout_.attributes != attribute_bit(Attribute::current)) return std::unexpected(Error{ErrorCode::invalid_argument});
        FieldEntry entry{};entry.id=id;entry.values[0]=value;return insert(entry,replace);
    }
    template<class Field> constexpr Result<void> replace(typename Field::value_type value) noexcept requires (Kind==BodyKind::values) { return set_value(Field::id,SemanticValue{value},true); }
    template<class Field> constexpr Result<void> select() noexcept requires (Kind==BodyKind::selectors) { return select(Field::id); }
    constexpr Result<void> select(FieldId id) noexcept requires (Kind==BodyKind::selectors) { FieldEntry entry{};entry.id=id;return insert(entry,false); }
    constexpr Result<void> diagnostic(FieldId id,std::uint32_t bits) noexcept requires (Kind==BodyKind::diagnostics) { FieldEntry entry{};entry.id=id;entry.diagnostic=bits;return insert(entry,false); }
    template<class Field> constexpr Result<void> remove() noexcept { return remove(Field::id); }
    constexpr Result<void> remove(FieldId id) noexcept {
        auto candidate=*this;
        std::size_t i=0;while(i<candidate.state_.size_ && candidate.state_.entries_[i].id!=id) ++i;
        if(i==candidate.state_.size_) return std::unexpected(Error{ErrorCode::invalid_argument});
        for(auto j=i+1;j<candidate.state_.size_;++j) candidate.state_.entries_[j-1]=candidate.state_.entries_[j];
        candidate.state_.entries_[--candidate.state_.size_]={};
        auto result=candidate.bump();if(!result)return result;candidate.indicators();*this=candidate;return {};
    }
    constexpr Result<void> with_attributes(std::uint32_t mask,std::span<const AttributeValue> supplied={}) noexcept {
        if(mask & ~std::uint32_t{0xfff80000})return std::unexpected(Error{ErrorCode::invalid_argument});
        auto candidate=*this;
        const auto selected=mask ? mask : attribute_bit(Attribute::current);
        std::array<std::array<bool,13>,N> seen{};
        if constexpr (Kind!=BodyKind::values) { if(!supplied.empty())return std::unexpected(Error{ErrorCode::invalid_argument}); }
        for(const auto& value:supplied) {
            const auto a=static_cast<std::size_t>(value.attribute);
            if(a>=13 || !(selected&attribute_bit(value.attribute)))return std::unexpected(Error{ErrorCode::invalid_argument});
            std::size_t i=0;while(i<candidate.state_.size_ && candidate.state_.entries_[i].id!=value.field)++i;
            if(i==candidate.state_.size_ || seen[i][a])return std::unexpected(Error{ErrorCode::invalid_argument});
            auto valid=validate_value(value.field,value.value);if(!valid)return valid;
            seen[i][a]=true;candidate.state_.entries_[i].values[a]=value.value;
        }
        for(auto& entry:candidate.state_.entries_) {
            if(&entry>=candidate.state_.entries_.data()+candidate.state_.size_) break;
            const auto* d=descriptor(entry.id);
            if(!d || (selected&~d->attributes))return std::unexpected(Error{ErrorCode::unsupported_layout});
            for(std::size_t a=0;a<13;++a) {
                if(!(selected&attribute_bit(static_cast<Attribute>(a))))entry.values[a].reset();
                else if constexpr(Kind==BodyKind::values) { if(!entry.values[a])return std::unexpected(Error{ErrorCode::invalid_argument}); }
            }
        }
        candidate.state_.layout_.attributes=mask;
        auto result=candidate.bump();if(!result)return result;candidate.indicators();*this=candidate;return {};
    }
};
constexpr bool compatible_body(BodyKind kind,PacketSubtype subtype) noexcept {
    switch(subtype) {
    case PacketSubtype::query: case PacketSubtype::cancel: return kind==BodyKind::selectors;
    case PacketSubtype::diagnostic_ack: return kind==BodyKind::diagnostics;
    default: return kind==BodyKind::values;
    }
}
template<BodyKind Kind, PacketSubtype Subtype, std::size_t N=16>
    requires (compatible_body(Kind,Subtype))
class TypedPacket : public PacketBuilder<Kind,N> {
public:
    constexpr TypedPacket() noexcept : PacketBuilder<Kind,N>(Subtype) {}
};
using ContextPacket = TypedPacket<BodyKind::values,PacketSubtype::context>;
using ControlPacket = TypedPacket<BodyKind::values,PacketSubtype::control>;
using QueryPacket = TypedPacket<BodyKind::selectors,PacketSubtype::query>;
using CancelPacket = TypedPacket<BodyKind::selectors,PacketSubtype::cancel>;
using DiagnosticAck = TypedPacket<BodyKind::diagnostics,PacketSubtype::diagnostic_ack>;
using StateAck = TypedPacket<BodyKind::values,PacketSubtype::state_ack>;
using SignalMetadata = TypedPacket<BodyKind::values,PacketSubtype::signal>;
} // namespace vita
