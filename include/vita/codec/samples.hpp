#pragma once
#include <vita/codec/wire.hpp>
#include <concepts>
#include <limits>
namespace vita::codec {
template<class T> concept SampleScalar = std::same_as<T,std::int16_t> || std::same_as<T,std::int32_t> || std::same_as<T,float>;
template<SampleScalar T> struct Iq { T i{},q{}; friend bool operator==(Iq,Iq)=default; };
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
template<SampleScalar T> inline Result<std::size_t> pack_iq(std::span<const Iq<T>> samples,MutableBytes output) noexcept {
    auto size=checked_multiply(samples.size(),sizeof(T)*2);if(!size)return std::unexpected(size.error());
    if(output.size()<*size)return std::unexpected(Error{ErrorCode::short_output,0,*size});
    if(*size) {
        const auto source=reinterpret_cast<std::uintptr_t>(samples.data()),destination=reinterpret_cast<std::uintptr_t>(output.data());
        if(source<=destination ? destination-source<*size : source-destination<*size)
            return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    std::size_t offset=0;
    for(auto pair:samples) {
        if constexpr(sizeof(T)==2){auto i=std::bit_cast<std::uint16_t>(pair.i),q=std::bit_cast<std::uint16_t>(pair.q);detail::store32(output,offset,(std::uint32_t{i}<<16)|q);offset+=4;}
        else{detail::store32(output,offset,std::bit_cast<std::uint32_t>(pair.i));detail::store32(output,offset+4,std::bit_cast<std::uint32_t>(pair.q));offset+=8;}
    }return *size;
}
inline Result<std::size_t> pack_iq16(std::span<const Iq<std::int16_t>> s,MutableBytes o) noexcept{return pack_iq(s,o);}
inline Result<std::size_t> pack_iq32(std::span<const Iq<std::int32_t>> s,MutableBytes o) noexcept{return pack_iq(s,o);}
inline Result<std::size_t> pack_iq_float(std::span<const Iq<float>> s,MutableBytes o) noexcept{return pack_iq(s,o);}
template<SampleScalar T> class SampleView {
    Bytes data_{};
    explicit SampleView(Bytes data) noexcept:data_(data){}
public:
    static Result<SampleView> create(Bytes data) noexcept {
        if(data.size()%(sizeof(T)*2))return std::unexpected(Error{ErrorCode::invalid_argument});return SampleView{data};
    }
    std::size_t size() const noexcept{return data_.size()/(sizeof(T)*2);}
    Result<Iq<T>> at(std::size_t index) const noexcept {
        if(index>=size())return std::unexpected(Error{ErrorCode::invalid_argument,index});
        const auto offset=index*sizeof(T)*2;
        if constexpr(sizeof(T)==2){auto bits=detail::load32(data_,offset);return Iq<T>{std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits>>16)),std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits))};}
        else return Iq<T>{std::bit_cast<T>(detail::load32(data_,offset)),std::bit_cast<T>(detail::load32(data_,offset+4))};
    }
};
} // namespace vita::codec
