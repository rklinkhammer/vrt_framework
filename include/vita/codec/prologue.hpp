#pragma once
#include <vita/codec/wire.hpp>
namespace vita::codec {
struct EnvelopeLayout {
    std::size_t packet_bytes{},prologue_bytes{},payload_offset{},payload_bytes{};
    std::optional<std::size_t> trailer_offset{};
};
inline Result<EnvelopeLayout> measure_prologue(const Envelope& e,std::size_t payload_bytes) noexcept {
    auto total=measure_envelope(e,payload_bytes);if(!total)return std::unexpected(total.error());
    const auto prefix=*total-payload_bytes-(e.trailer?4:0);
    return EnvelopeLayout{*total,prefix,prefix,payload_bytes,e.trailer?std::optional<std::size_t>{*total-4}:std::nullopt};
}
namespace detail {
// Internal: envelope and whole packet length validated; output covers the prologue.
inline std::size_t write_prologue(const Envelope& e,std::size_t packet_bytes,MutableBytes output) noexcept {
    std::uint32_t header=(static_cast<std::uint32_t>(e.type)<<28)|(e.class_id?1u<<27:0)|(static_cast<std::uint32_t>(e.timestamp.tsi)<<22)|(static_cast<std::uint32_t>(e.timestamp.tsf)<<20)|(e.packet_count<<16)|static_cast<std::uint32_t>(packet_bytes/4);
    if(is_data(e.type))header|=(e.trailer?1u<<26:0)|(e.nd0?1u<<25:0)|(e.spectrum?1u<<24:0);
    else if(is_command(e.type))header|=(e.ack?1u<<26:0)|(e.cancel?1u<<24:0);
    else header|=(e.nd0?1u<<25:0)|(e.tsm?1u<<24:0);
    std::size_t offset=0;auto put=[&](std::uint32_t value){store32(output,offset,value);offset+=4;};
    put(header);if(e.stream_id)put(*e.stream_id);
    if(e.class_id){put((std::uint32_t{e.class_id->pad_bits}<<27)|e.class_id->oui);put((std::uint32_t{e.class_id->information_class}<<16)|e.class_id->packet_class);}
    if(e.timestamp.tsi!=Tsi::none)put(e.timestamp.integer);
    if(e.timestamp.tsf!=Tsf::none){put(static_cast<std::uint32_t>(e.timestamp.fractional>>32));put(static_cast<std::uint32_t>(e.timestamp.fractional));}
    if(e.command){put(e.command->cam);put(e.command->message_id);for(const auto* id:{&e.command->controllee,&e.command->controller})for(std::size_t i=0;i<id->size_words();++i)put(id->words[i]);}
    return offset;
}
} // namespace detail
inline Result<std::size_t> encode_prologue(const Envelope& e,std::size_t payload_bytes,MutableBytes output) noexcept {
    auto layout=measure_prologue(e,payload_bytes);if(!layout)return std::unexpected(layout.error());
    if(output.size()<layout->prologue_bytes)return std::unexpected(Error{ErrorCode::short_output,0,layout->prologue_bytes});
    return detail::write_prologue(e,layout->packet_bytes,output);
}
inline Result<std::size_t> encode_trailer(std::uint32_t trailer,MutableBytes output) noexcept {
    if(output.size()<4)return std::unexpected(Error{ErrorCode::short_output,0,4});
    detail::store32(output,0,trailer);return 4;
}
} // namespace vita::codec
