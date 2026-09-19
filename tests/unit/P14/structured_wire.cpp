#include <vita/codec/packet.hpp>
#include <array>
#include <cassert>
using namespace vita;
using namespace vita::codec;
void word(MutableBytes bytes,std::size_t offset,std::uint32_t value) {
    for(unsigned i=0;i<4;++i)bytes[offset+i]=std::byte((value>>(24-8*i))&255);
}
void header(MutableBytes bytes,std::uint32_t cif) {
    word(bytes,0,0x40000000u|static_cast<std::uint32_t>(bytes.size()/4));word(bytes,4,1);word(bytes,8,cif);
}
int main() {
    std::array<std::byte,28> ascii{};header(ascii,1u<<9);word(ascii,12,0xffffff);word(ascii,16,2);
    for(unsigned i=0;i<6;++i)ascii[20+i]=std::byte(static_cast<unsigned char>("$FIX\r\n"[i]));
    auto decoded=decode_packet(ascii);assert(decoded&&decoded->fields[0].gps_ascii()->text.size()==6);
    ascii[27]=std::byte{'x'};assert(!decode_packet(ascii));ascii[27]=std::byte{};
    ascii[20]=std::byte{0x80};assert(!decode_packet(ascii));ascii[20]=std::byte{'$'};
    word(ascii,12,0x10ffffff);assert(!decode_packet(ascii));word(ascii,12,0xffffff);
    word(ascii,16,UINT32_MAX);assert(!decode_packet(ascii));word(ascii,16,2);
    auto work=decode_packet(ascii,DecodeOptions{{},{},{1024,9}});assert(!work&&work.error().code==ErrorCode::resource_limit);
    assert(decode_packet(ascii,DecodeOptions{{},{},{1024,10}}));
    std::array<std::byte,20+1025*4> large{};header(large,1u<<8);word(large,16,1025u<<16);
    auto too_many=decode_packet(large);assert(!too_many&&too_many.error().code==ErrorCode::resource_limit);
    assert(decode_packet(large,DecodeOptions{{},{},{1025,4096}}));
    word(large,16,1026u<<16);
    auto truncated=decode_packet(large);assert(!truncated&&truncated.error().code==ErrorCode::short_input);
    word(large,16,1025u<<16);word(large,12,0x80000000);assert(!decode_packet(large));
    // Declared tags require matching physical tail, not an inferred missing list.
    std::array<std::byte,24> missing_tag{};header(missing_tag,1u<<8);word(missing_tag,16,0x8001);
    assert(!decode_packet(missing_tag));
    // All three undefined fix words remain present and must be all ones.
    std::array<std::byte,56> gps{};header(gps,1u<<14);word(gps,12,0xffffff);
    for(std::size_t at=16;at<28;at+=4)word(gps,at,UINT32_MAX);
    for(std::size_t at=28;at<gps.size();at+=4)word(gps,at,0x7fffffff);
    auto unknown=decode_packet(gps);assert(unknown&&!unknown->fields[0].geolocation()->latitude_q22);
    word(gps,16,0);assert(!decode_packet(gps));word(gps,16,UINT32_MAX);
    word(gps,12,0x80ffffff);assert(!decode_packet(gps));
    // Larger than representable latitude is structurally readable, not semantically executable.
    word(gps,12,0xffffff);word(gps,28,91u<<22);
    auto invalid_value=decode_packet(gps);assert(invalid_value);
    NativeContextPacket<> destination;
    assert(!invalid_value->fields[0].materialize_into(destination));assert(destination.freeze().fields().empty());
}
