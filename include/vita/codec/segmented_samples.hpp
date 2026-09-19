#pragma once
#include <vita/codec/sample_descriptors.hpp>
#include <array>
#include <limits>

namespace vita::codec::samples {
template<std::size_t MaxSegments=8> class SegmentedSamples {
    struct Segment { Bytes bytes{}; std::size_t end=0; };
    std::array<Segment,MaxSegments> segments_{};
    std::size_t used_=0;
    Descriptor descriptor_;
    general::Shape shape_;
    SegmentedSamples(const Descriptor& d,general::Shape shape) noexcept:descriptor_(d),shape_(shape){}
    std::byte byte(std::size_t offset) const noexcept {
        std::size_t begin=0;
        for(std::size_t i=0;i<used_;++i) {
            if(offset<segments_[i].end)return segments_[i].bytes[offset-begin];
            begin=segments_[i].end;
        }
        return std::byte{0}; // Construction and checked field offsets make this unreachable.
    }
    std::uint64_t read(std::size_t offset,unsigned width) const noexcept {
        std::uint64_t value=0;
        for(unsigned b=0;b<width;++b) {
            const auto bit=offset+b;
            value=(value<<1)|((std::to_integer<unsigned>(byte(bit/8))>>(7-bit%8))&1u);
        }
        return value;
    }
public:
    // Copies only descriptors; every nonempty byte span remains a caller-owned immutable borrow.
    static Result<SegmentedSamples> create(std::span<const Bytes> pieces,const Descriptor& d,
                                          PayloadBinding binding,general::Limits limits={}) noexcept {
        if(pieces.size()>MaxSegments)return std::unexpected(Error{ErrorCode::resource_limit,0,pieces.size()});
        auto shape=measure_payload(d,binding,limits);if(!shape)return std::unexpected(shape.error());
        SegmentedSamples result{d,*shape};
        std::size_t extent=0;
        for(auto bytes:pieces) {
            if(bytes.empty())continue;
            if(!bytes.data())return std::unexpected(Error{ErrorCode::invalid_argument});
            if(bytes.size()>std::numeric_limits<std::size_t>::max()-extent)return std::unexpected(Error{ErrorCode::overflow});
            extent+=bytes.size();result.segments_[result.used_++]={bytes,extent};
        }
        if(extent<shape->bytes)return std::unexpected(Error{ErrorCode::short_input,0,shape->bytes});
        if(extent!=shape->bytes)return std::unexpected(Error{ErrorCode::invalid_argument});
        return result;
    }
    const general::Shape& shape() const noexcept{return shape_;}
    const Descriptor& descriptor() const noexcept{return descriptor_;}
    Result<general::Item> at(std::size_t index) const noexcept {
        if(index>=shape_.items)return std::unexpected(Error{ErrorCode::invalid_argument,index});
        const auto p=descriptor_.packing();
        const auto offset=general::detail::bit_offset(p,index);
        return general::Item{read(offset,p.item_bits),read(offset+p.packing_bits-p.channel_tag_bits,p.channel_tag_bits),
            read(offset+p.packing_bits-p.channel_tag_bits-p.event_tag_bits,p.event_tag_bits)};
    }
};
} // namespace vita::codec::samples
