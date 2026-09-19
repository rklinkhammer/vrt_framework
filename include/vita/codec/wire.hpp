#pragma once
#include <vita/core/bytes.hpp>
#include <vita/codec/layout.hpp>
#include <array>
#include <bit>
#include <optional>
#include <cstring>
namespace vita::codec {
namespace detail {
// Internal helpers: caller proves the four-byte extent before access.
inline std::uint32_t load32(Bytes data,std::size_t offset) noexcept {
    std::uint32_t v=0;std::memcpy(&v,data.data()+offset,4);
    if constexpr(std::endian::native==std::endian::little)v=std::byteswap(v);return v;
}
inline void store32(MutableBytes data,std::size_t offset,std::uint32_t value) noexcept {
    if constexpr(std::endian::native==std::endian::little)value=std::byteswap(value);
    std::memcpy(data.data()+offset,&value,4);
}
} // namespace detail
enum class PacketType : std::uint8_t { signal_without_sid=0,signal=1,extension_without_sid=2,extension_data=3,context=4,extension_context=5,command=6,extension_command=7 };
enum class Tsi : std::uint8_t { none,utc,gps,other };
enum class Tsf : std::uint8_t { none,sample_count,picoseconds,free_running };
struct Timestamp { Tsi tsi=Tsi::none;Tsf tsf=Tsf::none;std::uint32_t integer=0;std::uint64_t fractional=0; };
struct ClassId { std::uint32_t oui=0;std::uint16_t information_class=0,packet_class=0;std::uint8_t pad_bits=0; friend bool operator==(ClassId,ClassId)=default; };
enum class IdentifierKind { absent,short_id,uuid };
struct Identifier {
    IdentifierKind kind=IdentifierKind::absent;std::array<std::uint32_t,4> words{};
    static constexpr Identifier short_id(std::uint32_t value) noexcept{return {IdentifierKind::short_id,{value,0,0,0}};}
    static constexpr Identifier uuid(std::array<std::uint32_t,4> value) noexcept{return {IdentifierKind::uuid,value};}
    constexpr std::size_t size_words() const noexcept{return kind==IdentifierKind::absent?0:kind==IdentifierKind::short_id?1:4;}
};
struct Command { std::uint32_t cam=0,message_id=0;Identifier controllee{},controller{}; };
struct Envelope {
    PacketType type=PacketType::signal;
    std::uint8_t packet_count=0;
    std::optional<std::uint32_t> stream_id{};
    std::optional<ClassId> class_id{};
    Timestamp timestamp{};
    bool trailer=false,nd0=false,spectrum=false,tsm=false,ack=false,cancel=false;
    std::optional<Command> command{};
};
constexpr bool is_data(PacketType t) noexcept{return static_cast<unsigned>(t)<=3;}
constexpr bool is_command(PacketType t) noexcept{return static_cast<unsigned>(t)>=6;}
constexpr bool is_extension(PacketType t) noexcept{return t==PacketType::extension_without_sid||t==PacketType::extension_data||t==PacketType::extension_context||t==PacketType::extension_command;}
constexpr Result<void> validate_cam(const Envelope& e) noexcept {
    const auto cam=e.command->cam;
    const auto reserved=(1u<<21)|(1u<<15)|(e.ack?0x300u:0xf00u)|(e.type==PacketType::extension_command?0u:0xffu);
    if(cam&reserved)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(((cam>>23)&3)==3)return std::unexpected(Error{ErrorCode::invalid_argument});
    auto timing=(cam>>12)&7;
    if(timing>4 && !(e.ack&&timing==7))return std::unexpected(Error{ErrorCode::invalid_argument});
    // Ack timing refers to the original Control; an unknown/no-effect Ack timestamp may be omitted.
    if(timing && !e.ack && e.timestamp.tsi==Tsi::none && e.timestamp.tsf==Tsf::none)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(e.ack && std::popcount((cam>>18)&7)!=1)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(e.ack && (cam&(1u<<18)) && (cam&((1u<<17)|(1u<<16))))return std::unexpected(Error{ErrorCode::invalid_argument});
    if(e.cancel && !e.ack && (((cam>>23)&3)!=2 || !(cam&(1u<<27)) || (cam&(1u<<20)) || ((cam&(1u<<18))&&!(cam&(1u<<19)))))return std::unexpected(Error{ErrorCode::invalid_argument});
    return {};
}
inline Result<std::size_t> measure_envelope(const Envelope& e,std::size_t payload_bytes) noexcept {
    const auto type=static_cast<unsigned>(e.type);
    if(type>7 || e.packet_count>15 || static_cast<unsigned>(e.timestamp.tsi)>3 || static_cast<unsigned>(e.timestamp.tsf)>3)return std::unexpected(Error{ErrorCode::invalid_argument});
    const bool sid=type!=0&&type!=2;
    if(sid!=e.stream_id.has_value())return std::unexpected(Error{ErrorCode::invalid_argument});
    if(e.class_id && (e.class_id->oui>0xffffff || e.class_id->pad_bits>31))return std::unexpected(Error{ErrorCode::invalid_argument});
    if(is_data(e.type)) {if(e.ack||e.cancel||e.tsm||e.command)return std::unexpected(Error{ErrorCode::invalid_argument});}
    else if(is_command(e.type)){if(e.trailer||e.nd0||e.spectrum||e.tsm||!e.command)return std::unexpected(Error{ErrorCode::invalid_argument});}
    else if(e.trailer||e.ack||e.cancel||e.spectrum||e.command)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(payload_bytes%4)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(is_data(e.type)&&payload_bytes==0)return std::unexpected(Error{ErrorCode::invalid_argument});
    std::size_t words=1+(sid?1:0)+(e.class_id?2:0)+(e.timestamp.tsi!=Tsi::none?1:0)+(e.timestamp.tsf!=Tsf::none?2:0)+(e.trailer?1:0);
    if(e.command) {
        auto valid=validate_cam(e);if(!valid)return std::unexpected(valid.error());
        auto check_id=[&](const Identifier& id,unsigned enable,unsigned uuid)->bool {
            if(static_cast<unsigned>(id.kind)>2)return false;
            if(bool(e.command->cam&(1u<<enable))!=(id.kind!=IdentifierKind::absent))return false;
            // Type bits are ignored when the associated enable is absent.
            if(id.kind!=IdentifierKind::absent && bool(e.command->cam&(1u<<uuid))!=(id.kind==IdentifierKind::uuid))return false;
            return id.kind!=IdentifierKind::uuid || (id.words[0]|id.words[1]|id.words[2]|id.words[3]);
        };
        if(!check_id(e.command->controllee,31,30)||!check_id(e.command->controller,29,28))return std::unexpected(Error{ErrorCode::invalid_argument});
        words+=2+e.command->controllee.size_words()+e.command->controller.size_words();
    }
    auto total=checked_add(words,payload_bytes/4);if(!total)return std::unexpected(total.error());
    if(*total>65535)return std::unexpected(Error{ErrorCode::overflow});
    return *total*4;
}
inline Result<std::size_t> encode_envelope(const Envelope& e,Bytes payload,std::optional<std::uint32_t> trailer,MutableBytes output) noexcept {
    if(e.trailer!=trailer.has_value())return std::unexpected(Error{ErrorCode::invalid_argument});
    auto size=measure_envelope(e,payload.size());if(!size)return std::unexpected(size.error());
    if(output.size()<*size)return std::unexpected(Error{ErrorCode::short_output,0,*size});
    std::uint32_t header=(static_cast<std::uint32_t>(e.type)<<28)|(e.class_id?1u<<27:0)|(static_cast<std::uint32_t>(e.timestamp.tsi)<<22)|(static_cast<std::uint32_t>(e.timestamp.tsf)<<20)|(e.packet_count<<16)|static_cast<std::uint32_t>(*size/4);
    if(is_data(e.type))header|=(e.trailer?1u<<26:0)|(e.nd0?1u<<25:0)|(e.spectrum?1u<<24:0);
    else if(is_command(e.type))header|=(e.ack?1u<<26:0)|(e.cancel?1u<<24:0);
    else header|=(e.nd0?1u<<25:0)|(e.tsm?1u<<24:0);
    const auto payload_offset=*size-payload.size()-(e.trailer?4:0);
    // Relocate payload before writing any prologue bytes, even for overlapping views.
    if(!payload.empty())std::memmove(output.data()+payload_offset,payload.data(),payload.size());
    std::size_t offset=0;auto put=[&](std::uint32_t value){detail::store32(output,offset,value);offset+=4;};
    put(header);if(e.stream_id)put(*e.stream_id);
    if(e.class_id){put((std::uint32_t{e.class_id->pad_bits}<<27)|e.class_id->oui);put((std::uint32_t{e.class_id->information_class}<<16)|e.class_id->packet_class);}
    if(e.timestamp.tsi!=Tsi::none)put(e.timestamp.integer);
    if(e.timestamp.tsf!=Tsf::none){put(static_cast<std::uint32_t>(e.timestamp.fractional>>32));put(static_cast<std::uint32_t>(e.timestamp.fractional));}
    if(e.command){put(e.command->cam);put(e.command->message_id);for(const auto* id:{&e.command->controllee,&e.command->controller})for(std::size_t i=0;i<id->size_words();++i)put(id->words[i]);}
    offset+=payload.size();
    if(trailer)put(*trailer);return offset;
}
struct EnvelopeView { Envelope envelope;Bytes payload;std::optional<std::uint32_t> trailer;std::size_t payload_offset=0;Bytes wire; };
inline Result<EnvelopeView> decode_envelope(Bytes wire) noexcept {
    if(wire.size()<4)return std::unexpected(Error{ErrorCode::short_input,0,4});
    const auto h=detail::load32(wire,0);const auto type=h>>28;const std::size_t size=(h&0xffff)*4;
    if(type>7||size==0)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(size>wire.size())return std::unexpected(Error{ErrorCode::short_input,wire.size(),size});
    if(size!=wire.size())return std::unexpected(Error{ErrorCode::invalid_argument,size});
    Envelope e{};e.type=static_cast<PacketType>(type);e.packet_count=(h>>16)&15;e.timestamp.tsi=static_cast<Tsi>((h>>22)&3);e.timestamp.tsf=static_cast<Tsf>((h>>20)&3);
    if(is_data(e.type)){e.trailer=h&(1u<<26);e.nd0=h&(1u<<25);e.spectrum=h&(1u<<24);}
    else if(is_command(e.type)){if(h&(1u<<25))return std::unexpected(Error{ErrorCode::invalid_argument});e.ack=h&(1u<<26);e.cancel=h&(1u<<24);}
    else{if(h&(1u<<26))return std::unexpected(Error{ErrorCode::invalid_argument});e.nd0=h&(1u<<25);e.tsm=h&(1u<<24);}
    std::size_t offset=4;
    auto take=[&]() noexcept -> Result<std::uint32_t>{if(offset>size||size-offset<4)return std::unexpected(Error{ErrorCode::short_input,offset,offset+4});auto value=detail::load32(wire,offset);offset+=4;return value;};
    if(type!=0&&type!=2){auto v=take();if(!v)return std::unexpected(v.error());e.stream_id=*v;}
    if(h&(1u<<27)){auto a=take(),b=take();if(!a)return std::unexpected(a.error());if(!b)return std::unexpected(b.error());if(*a&0x07000000)return std::unexpected(Error{ErrorCode::invalid_argument,offset-8});e.class_id=ClassId{*a&0xffffff,static_cast<std::uint16_t>(*b>>16),static_cast<std::uint16_t>(*b),static_cast<std::uint8_t>(*a>>27)};}
    if(e.timestamp.tsi!=Tsi::none){auto v=take();if(!v)return std::unexpected(v.error());e.timestamp.integer=*v;}
    if(e.timestamp.tsf!=Tsf::none){auto a=take(),b=take();if(!a)return std::unexpected(a.error());if(!b)return std::unexpected(b.error());e.timestamp.fractional=(std::uint64_t{*a}<<32)|*b;}
    if(is_command(e.type)) {
        auto cam=take(),mid=take();if(!cam)return std::unexpected(cam.error());if(!mid)return std::unexpected(mid.error());e.command=Command{*cam,*mid};
        for(unsigned which=0;which<2;++which) {
            auto& id=which?e.command->controller:e.command->controllee;const unsigned enable=which?29:31,uuid=which?28:30;
            if(*cam&(1u<<enable)){id.kind=(*cam&(1u<<uuid))?IdentifierKind::uuid:IdentifierKind::short_id;for(std::size_t i=0;i<id.size_words();++i){auto v=take();if(!v)return std::unexpected(v.error());id.words[i]=*v;}}
        }
    }
    const std::size_t trailer_size=e.trailer?4:0;
    if(offset>size || trailer_size>size-offset)return std::unexpected(Error{ErrorCode::short_input,offset,offset+trailer_size});
    const auto payload=wire.subspan(offset,size-offset-trailer_size);
    auto measured=measure_envelope(e,payload.size());if(!measured)return std::unexpected(measured.error());
    return EnvelopeView{e,payload,e.trailer?std::optional<std::uint32_t>{detail::load32(wire,size-4)}:std::nullopt,offset,wire};
}
} // namespace vita::codec
