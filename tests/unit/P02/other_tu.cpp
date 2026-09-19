#include <vita/codec/packet.hpp>
#include <vita/codec/samples.hpp>
bool codec_other_tu() noexcept { return vita::codec::is_command(vita::codec::PacketType::command); }
