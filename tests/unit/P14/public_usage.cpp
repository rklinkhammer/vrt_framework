#include <vita/codec/packet.hpp>
#include <array>
#include <cassert>
int main() {
    vita::ContextPacket context;
    assert(context.set<vita::Bandwidth>(*vita::Hertz::from_integer(2000000)));
    assert(context.set<vita::Gain>(vita::GainStages{128,-64}));
    assert(context.set<vita::TimestampAdjustment>(vita::Femtoseconds{-1000}));
    assert(context.set<vita::TimestampCalibrationTime>(100));
    assert(context.set<vita::DeviceIdentifier>({0xffffff,1}));
    vita::codec::Envelope header;header.type=vita::codec::PacketType::context;
    header.stream_id=7;header.timestamp={vita::codec::Tsi::gps,vita::codec::Tsf::picoseconds,101,0};
    std::array<std::byte,128> storage{};
    auto size=vita::codec::encode_packet(header,context.freeze(),storage);assert(size);
    auto received=vita::codec::decode_packet(vita::Bytes{storage}.first(*size));assert(received);
    assert(received->fields.size()==5);
}
