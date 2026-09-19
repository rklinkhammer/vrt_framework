#pragma once
#include <vita/core/bytes.hpp>
#include <vita/core/error.hpp>
#include <cstdint>
#include <limits>
#include <span>

namespace vita::codec::general {
enum class PackingMode { link, processing };
enum class SampleKind { real, cartesian, polar };
enum class RepeatMode { none, component, channel };
struct PackingSpec {
    unsigned item_bits=16, packing_bits=16, channel_tag_bits=0, event_tag_bits=0;
    PackingMode packing=PackingMode::link;
    SampleKind kind=SampleKind::real;
    RepeatMode repeating=RepeatMode::none;
    std::uint32_t vector_size=1, repeat_count=1;
};
struct Limits { std::size_t max_items=4096, max_bytes=65535u*4; };
struct Shape {
    std::size_t structures=0, items_per_structure=0, items=0, bytes=0;
};
// Exact bits, including signed encodings, IEEE special values and VRT components.
// The caller supplies numeric interpretation separately; this API does not narrow.
struct Item { std::uint64_t data_bits=0, channel_tag=0, event_tag=0; friend bool operator==(Item,Item)=default; };
struct Coordinates {
    std::size_t structure=0;
    std::uint32_t time=0, channel=0, component=0;
    friend bool operator==(Coordinates,Coordinates)=default;
};
namespace detail {
constexpr unsigned components(SampleKind kind) noexcept { return kind==SampleKind::real?1:2; }
constexpr Result<std::size_t> multiply(std::size_t a,std::size_t b) noexcept {
    if(a && b>std::numeric_limits<std::size_t>::max()/a)return std::unexpected(Error{ErrorCode::overflow});
    return a*b;
}
constexpr bool fits(std::uint64_t v,unsigned bits) noexcept { return bits==64 || (v>>bits)==0; }
constexpr Result<void> validate(PackingSpec s) noexcept {
    if(s.packing!=PackingMode::link && s.packing!=PackingMode::processing)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(s.kind!=SampleKind::real && s.kind!=SampleKind::cartesian && s.kind!=SampleKind::polar)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(s.repeating!=RepeatMode::none && s.repeating!=RepeatMode::component && s.repeating!=RepeatMode::channel)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(!s.item_bits || s.item_bits>64 || !s.packing_bits || s.packing_bits>64 || s.channel_tag_bits>15 || s.event_tag_bits>7 ||
       s.item_bits+s.channel_tag_bits+s.event_tag_bits>s.packing_bits || !s.vector_size || s.vector_size>65535 || !s.repeat_count || s.repeat_count>65536)
        return std::unexpected(Error{ErrorCode::invalid_argument});
    if((s.repeating==RepeatMode::none && s.repeat_count!=1) || (s.repeating==RepeatMode::component && s.kind==SampleKind::real))
        return std::unexpected(Error{ErrorCode::invalid_argument});
    // Section6.1.1.2-3 defines whole fields in32-bit words, not a64-bit group.
    if(s.packing==PackingMode::processing && s.packing_bits>32)return std::unexpected(Error{ErrorCode::unsupported_layout});
    return {};
}
constexpr std::size_t bit_offset(PackingSpec s,std::size_t index) noexcept {
    if(s.packing==PackingMode::link)return index*s.packing_bits;
    const auto per=32/s.packing_bits;
    return (index/per)*32+(index%per)*s.packing_bits;
}
inline std::uint64_t read_bits(Bytes bytes,std::size_t offset,unsigned width) noexcept {
    std::uint64_t value=0;
    for(unsigned b=0;b<width;++b) {
        const auto bit=offset+b;
        value=(value<<1)|((std::to_integer<unsigned>(bytes[bit/8])>>(7-bit%8))&1u);
    }
    return value;
}
inline void write_bits(MutableBytes bytes,std::size_t offset,unsigned width,std::uint64_t value) noexcept {
    for(unsigned b=0;b<width;++b) {
        const auto bit=offset+b;
        if((value>>(width-1-b))&1u)bytes[bit/8]|=std::byte{static_cast<unsigned char>(1u<<(7-bit%8))};
    }
}
inline bool overlap(const void* a,std::size_t an,const void* b,std::size_t bn) noexcept {
    if(!an || !bn)return false;
    const auto x=reinterpret_cast<std::uintptr_t>(a),y=reinterpret_cast<std::uintptr_t>(b);
    return x<=y ? y-x<an : x-y<bn;
}
} // namespace detail

inline Result<Shape> measure(PackingSpec s,std::size_t structures,Limits limits={}) noexcept {
    auto valid=detail::validate(s);if(!valid)return std::unexpected(valid.error());
    auto samples=detail::multiply(s.vector_size,s.repeat_count);if(!samples)return std::unexpected(samples.error());
    auto group=detail::multiply(*samples,detail::components(s.kind));if(!group)return std::unexpected(group.error());
    auto items=detail::multiply(structures,*group);if(!items)return std::unexpected(items.error());
    if(*items>limits.max_items)return std::unexpected(Error{ErrorCode::resource_limit,0,*items});
    std::size_t words=0;
    if(s.packing==PackingMode::link) {
        auto bits=detail::multiply(*items,s.packing_bits);if(!bits)return std::unexpected(bits.error());
        words=*bits/32+(*bits%32!=0);
    } else {
        const auto per=32/s.packing_bits;
        words=*items/per+(*items%per!=0);
    }
    auto bytes=detail::multiply(words,4);if(!bytes)return std::unexpected(bytes.error());
    // Also prove all bit-offset arithmetic used by the cursor.
    auto bits=bytes?detail::multiply(*bytes,8):Result<std::size_t>{std::unexpected(bytes.error())};
    if(!bits)return std::unexpected(bits.error());
    if(*bytes>limits.max_bytes)return std::unexpected(Error{ErrorCode::resource_limit,0,*bytes});
    return Shape{structures,*group,*items,*bytes};
}

class PackedSamples {
    Bytes bytes_{};
    PackingSpec spec_{};
    Shape shape_{};
    PackedSamples(Bytes bytes,PackingSpec spec,Shape shape) noexcept:bytes_(bytes),spec_(spec),shape_(shape){}
public:
    // Exact extent and explicit count: no guessing sample count from trailing bits.
    static Result<PackedSamples> create(Bytes bytes,PackingSpec spec,std::size_t structures,Limits limits={}) noexcept {
        auto shape=measure(spec,structures,limits);if(!shape)return std::unexpected(shape.error());
        if(bytes.size()<shape->bytes)return std::unexpected(Error{ErrorCode::short_input,0,shape->bytes});
        if(bytes.size()!=shape->bytes)return std::unexpected(Error{ErrorCode::invalid_argument});
        return PackedSamples{bytes,spec,*shape};
    }
    const Shape& shape() const noexcept { return shape_; }
    PackingSpec spec() const noexcept { return spec_; }
    Result<Item> at(std::size_t index) const noexcept {
        if(index>=shape_.items)return std::unexpected(Error{ErrorCode::invalid_argument,index});
        const auto bit=detail::bit_offset(spec_,index);
        return Item{detail::read_bits(bytes_,bit,spec_.item_bits),
            detail::read_bits(bytes_,bit+spec_.packing_bits-spec_.channel_tag_bits,spec_.channel_tag_bits),
            detail::read_bits(bytes_,bit+spec_.packing_bits-spec_.channel_tag_bits-spec_.event_tag_bits,spec_.event_tag_bits)};
    }
    Result<Coordinates> coordinates(std::size_t index) const noexcept {
        if(index>=shape_.items)return std::unexpected(Error{ErrorCode::invalid_argument,index});
        const auto structure=index/shape_.items_per_structure;
        const auto local=index%shape_.items_per_structure;
        const auto c=detail::components(spec_.kind),r=spec_.repeat_count,v=spec_.vector_size;
        std::size_t time=0,channel=0,component=0;
        if(spec_.repeating==RepeatMode::component) {
            const auto linear=(local/(c*r))*r+local%r;
            component=(local/r)%c;time=linear/v;channel=linear%v;
        } else if(spec_.repeating==RepeatMode::channel) {
            component=local%c;time=(local/c)%r;channel=local/(c*r);
        } else { component=local%c;channel=local/c; }
        return Coordinates{structure,static_cast<std::uint32_t>(time),static_cast<std::uint32_t>(channel),static_cast<std::uint32_t>(component)};
    }
    Result<std::size_t> index(Coordinates x) const noexcept {
        if(x.structure>=shape_.structures || x.time>=spec_.repeat_count || x.channel>=spec_.vector_size || x.component>=detail::components(spec_.kind))
            return std::unexpected(Error{ErrorCode::invalid_argument});
        const auto c=detail::components(spec_.kind),r=spec_.repeat_count;
        std::size_t local=0;
        if(spec_.repeating==RepeatMode::component) {
            const auto linear=std::size_t{x.time}*spec_.vector_size+x.channel;
            local=((linear/r)*c+x.component)*r+linear%r;
        } else if(spec_.repeating==RepeatMode::channel) local=(std::size_t{x.channel}*r+x.time)*c+x.component;
        else local=std::size_t{x.channel}*c+x.component;
        return x.structure*shape_.items_per_structure+local;
    }
};

inline Result<std::size_t> pack(PackingSpec spec,std::size_t structures,std::span<const Item> items,MutableBytes output,Limits limits={}) noexcept {
    auto shape=measure(spec,structures,limits);if(!shape)return std::unexpected(shape.error());
    if(items.size()!=shape->items)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(output.size()<shape->bytes)return std::unexpected(Error{ErrorCode::short_output,0,shape->bytes});
    auto input_bytes=detail::multiply(items.size(),sizeof(Item));if(!input_bytes)return std::unexpected(input_bytes.error());
    if(detail::overlap(items.data(),*input_bytes,output.data(),shape->bytes))return std::unexpected(Error{ErrorCode::invalid_argument});
    // Validate all input before touching caller output; after this pass writes cannot fail.
    for(const auto item:items)
        if(!detail::fits(item.data_bits,spec.item_bits) || !detail::fits(item.channel_tag,spec.channel_tag_bits) || !detail::fits(item.event_tag,spec.event_tag_bits))
            return std::unexpected(Error{ErrorCode::invalid_argument});
    auto wire=output.first(shape->bytes);
    for(auto& byte:wire)byte=std::byte{0};
    for(std::size_t i=0;i<items.size();++i) {
        const auto bit=detail::bit_offset(spec,i);const auto item=items[i];
        detail::write_bits(wire,bit,spec.item_bits,item.data_bits);
        detail::write_bits(wire,bit+spec.packing_bits-spec.channel_tag_bits,spec.channel_tag_bits,item.channel_tag);
        detail::write_bits(wire,bit+spec.packing_bits-spec.channel_tag_bits-spec.event_tag_bits,spec.event_tag_bits,item.event_tag);
    }
    return shape->bytes;
}
} // namespace vita::codec::general
