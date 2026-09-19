#pragma once
#include <vita/fields/packet.hpp>
#include <vita/core/fixed_vector.hpp>
#include <bit>
#include <limits>
namespace vita {
constexpr Result<std::size_t> checked_add(std::size_t a,std::size_t b) noexcept {
    if(b>std::numeric_limits<std::size_t>::max()-a)return std::unexpected(Error{ErrorCode::overflow,a});
    return a+b;
}
constexpr Result<std::size_t> checked_multiply(std::size_t a,std::size_t b) noexcept {
    if(a && b>std::numeric_limits<std::size_t>::max()/a)return std::unexpected(Error{ErrorCode::overflow});
    return a*b;
}
struct LayoutElement { FieldId field; Attribute attribute; std::size_t offset, bytes; };
struct LayoutMeasure { std::size_t bytes; std::uint64_t generation, signature; };
template<BodyKind K,std::size_t N> constexpr std::uint64_t layout_signature(const PacketSnapshot<K,N>& packet) noexcept {
    std::uint64_t hash=14695981039346656037ull;
    auto mix=[&](std::uint64_t value){hash=(hash^value)*1099511628211ull;};
    mix(static_cast<std::uint64_t>(K));mix(packet.layout().attributes);
    mix(static_cast<std::uint64_t>(packet.layout().subtype));mix(packet.layout().packet_class);mix(packet.layout().action);
    for(const auto& e:packet.fields()){mix(e.id.cif);mix(e.id.bit);}
    return hash;
}
// Shared body traversal: offsets include CIF words, exclude packet-family prologue.
// The visitor receives checked extents only; encode/decode/index consume this contract.
template<BodyKind K,std::size_t N,class Visitor>
constexpr Result<LayoutMeasure> walk_layout(const PacketSnapshot<K,N>& packet,Visitor&& visitor,std::size_t start_offset=0) noexcept {
    std::size_t words=1;
    for(unsigned i=1;i<8;++i)if(packet.layout().cif[0]&(1u<<i))++words;
    auto head=checked_add(start_offset,words*4);if(!head)return std::unexpected(head.error());
    std::size_t offset=*head;
    for(const auto& field:packet.fields()) {
        const auto* d=descriptor(field.id);
        if(!d)return std::unexpected(Error{ErrorCode::unsupported_layout,offset});
        auto attributes=packet.layout().attributes ? packet.layout().attributes : attribute_bit(Attribute::current);
        if(attributes&~d->attributes)return std::unexpected(Error{ErrorCode::unsupported_layout,offset});
        if constexpr(K==BodyKind::selectors) continue;
        for(unsigned i=0;i<13;++i)if(attributes&attribute_bit(static_cast<Attribute>(i))) {
            const auto bytes=K==BodyKind::diagnostics ? 4 : d->words*4;
            auto next=checked_add(offset,bytes);if(!next)return std::unexpected(next.error());
            if constexpr(K==BodyKind::values) {
                if(!field.values[i])return std::unexpected(Error{ErrorCode::invalid_argument,offset});
                auto valid=validate_value(field.id,*field.values[i]);if(!valid)return std::unexpected(valid.error());
            }
            auto result=visitor(LayoutElement{field.id,static_cast<Attribute>(i),offset,bytes});
            if(!result)return std::unexpected(result.error());
            offset=*next;
        }
    }
    return LayoutMeasure{offset-start_offset,packet.generation(),layout_signature(packet)};
}
template<BodyKind K,std::size_t N> constexpr Result<LayoutMeasure> measure(const PacketSnapshot<K,N>& packet,std::size_t start_offset=0) noexcept {
    return walk_layout(packet,[](LayoutElement) noexcept -> Result<void>{return {};},start_offset);
}
template<BodyKind K,std::size_t N> constexpr Result<void> validate_measure(const PacketSnapshot<K,N>& packet,LayoutMeasure cached) noexcept {
    if(cached.generation!=packet.generation() || cached.signature!=layout_signature(packet))return std::unexpected(Error{ErrorCode::stale_generation});
    auto actual=measure(packet);if(!actual)return std::unexpected(actual.error());
    if(actual->bytes!=cached.bytes)return std::unexpected(Error{ErrorCode::invalid_argument});
    return {};
}
template<std::size_t Capacity,BodyKind K,std::size_t N>
constexpr Result<FixedVector<LayoutElement,Capacity>> index_layout(const PacketSnapshot<K,N>& packet) noexcept {
    FixedVector<LayoutElement,Capacity> index;
    auto result=walk_layout(packet,[&](LayoutElement entry) noexcept -> Result<void>{return index.push_back(entry);});
    if(!result)return std::unexpected(result.error());return index;
}
} // namespace vita
