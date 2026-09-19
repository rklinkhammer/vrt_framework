#include <vita/codec/packet.hpp>
#include <cassert>
#include <vector>

using namespace vita;
using namespace vita::codec;
static void append(std::vector<std::byte>& b, std::uint32_t w) {
  for (int shift = 24; shift >= 0; shift -= 8) b.push_back(std::byte(w >> shift));
}
static void put(std::vector<std::byte>& b, std::size_t offset, std::uint32_t w) {
  for (unsigned i = 0; i < 4; ++i) b[offset+i] = std::byte(w >> (24-8*i));
}
static std::vector<std::byte> context(unsigned bit, std::initializer_list<std::uint32_t> field) {
  std::vector<std::byte> b;
  for (auto w : {0u, 1u, 2u, 1u << bit}) append(b,w);
  for (auto w : field) append(b,w);
  put(b,0,0x40000000u | static_cast<unsigned>(b.size()/4));
  return b;
}
static void reject(Bytes b) {
  unsigned calls=0;
  auto r=decode_and_visit(b,{},[&](FieldView) noexcept -> Result<void> { ++calls; return {}; });
  assert(!r && calls==0);
}
static void good(const std::vector<std::byte>& b,unsigned bit) {
  auto decoded=decode_packet(b);
  assert(decoded && decoded->fields.size()==1);
  assert((decoded->fields[0].id==FieldId{1,static_cast<std::uint8_t>(bit)}));
  assert(decoded->fields[0].bytes.size()==b.size()-16);
  for(std::size_t n=0;n<b.size();++n) reject(Bytes{b}.first(n));
}
int main() {
  // Independent §9.3.2 packed widths, including zero entries and final padding.
  auto empty=context(7,{2,0x10000000});good(empty,7);
  auto bytes=context(7,{4,0x10000005,0x01237f80,0xff000000});good(bytes,7);
  auto halves=context(7,{4,0x20000003,0x0123abcd,0xffff0000});good(halves,7);
  auto words=context(7,{4,0x40000002,0x12345678,0xffffffff});good(words,7);
  assert(decode_packet(bytes)->fields[0].indices()->at(4)==255);
  assert(decode_packet(halves)->fields[0].indices()->at(1)==0xabcd);
  assert(decode_packet(words)->fields[0].indices()->at(0)==0x12345678);
  for(auto pair : {std::pair{bytes,31u},std::pair{halves,31u}}) {
    auto bad=pair.first;bad[pair.second]=std::byte{1};reject(bad);
  }
  for(unsigned width : {0u,3u,5u,15u}) {auto bad=empty;put(bad,20,width<<28);reject(bad);}
  for(unsigned bit=20;bit<28;++bit) {auto bad=empty;put(bad,20,0x10000000u|(1u<<bit));reject(bad);}
  auto mismatch=bytes;put(mismatch,16,3);reject(mismatch);
  mismatch=bytes;put(mismatch,20,0x10000009);reject(mismatch);

  // §9.4.1: mandatory vector, optional global and record reference words.
  auto vectors=context(28,{5,0x03001002,0x40000000,0,0x2d005a00});good(vectors,28);
  auto refs=context(28,{8,0x04002002,0xc0000000,5,0x00010000,0,0x0002000a,0xd3005a00});good(refs,28);
  auto pointing=decode_packet(refs)->fields[0].pointing_vectors();assert(pointing&&pointing->size()==2);
  auto global=pointing->global();assert(global&&*global);assert((**global==PointingReference{0,1,1}));
  assert(pointing->at(1)->vector==(PointingAngles{180*128,-90*128}));assert(!pointing->at(2));
  for(auto change : {std::pair{24u,0u},std::pair{24u,0x40000001u},std::pair{20u,0x03002002u}}) {
    auto bad=vectors;put(bad,change.first,change.second);reject(bad);
  }
  for(auto change : {std::pair{28u,0x00010005u},std::pair{28u,3u},std::pair{32u,0x00010010u},std::pair{40u,0x00020003u}}) {
    auto bad=refs;put(bad,change.first,change.second);reject(bad);
  }

  // Required Sector/F1 only: record order and arbitrary sector identifiers survive.
  auto sectors=context(9,{9,0x00003002,0xc0000000,0xffffffff,0xffffffff,0xfff00000,7,0,0x00100000});good(sectors,9);
  auto scan=decode_packet(sectors)->fields[0].sectors();assert(scan&&scan->size()==2&&scan->at(0)->sector==0xffffffff&&scan->at(0)->f1_q20==-(1<<20)&&scan->at(1)->f1_q20==(1<<20));assert(!scan->at(2));
  for(auto mask : {0u,0x80000000u,0x40000000u,0xc0000001u}) {
    auto bad=sectors;put(bad,24,mask);reject(bad);
  }
  // Spectrum is thirteen words, independent of arena capacity.
  auto bad_header=sectors;put(bad_header,20,0x03003002);reject(bad_header);
  auto spectrum=context(10,{0,0,256,256,0,0x00100000,0,0x01000000,1,0xdeadbeef,0,255,0});good(spectrum,10);
  for(auto code : {5u,127u}) {auto bad=spectrum;put(bad,16,code);reject(bad);}
  for(auto code : {44u,99u}) {auto bad=spectrum;put(bad,20,code);reject(bad);}
  for(auto code : {128u,255u}) {auto valid=spectrum;put(valid,16,code);put(valid,20,100);good(valid,10);}
  for(auto mask : {0x004000u,0x040000u,0x100000u}) {auto bad=spectrum;put(bad,16,mask);reject(bad);}
  for(auto change : {std::pair{16u,0x2000u},std::pair{32u,0xffffffffu},std::pair{40u,0xffffffffu}}) {
    auto raw=spectrum;put(raw,change.first,change.second);good(raw,10);
    auto value=decode_packet(raw)->fields[0].spectrum();assert(value);NativeContextPacket<256> b;assert(!b.set<Spectrum>(*value));assert(b.freeze().fields().empty());
  }
  auto percentage=spectrum;put(percentage,16,0x10000);put(percentage,64,100*4096);good(percentage,10);
  put(percentage,64,100*4096+1);good(percentage,10);auto value=decode_packet(percentage)->fields[0].spectrum();assert(value);NativeContextPacket<256> b;assert(!b.set<Spectrum>(*value));
}
