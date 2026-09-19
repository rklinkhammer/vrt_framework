#include <vita/fields/cif1_structured.hpp>
#include <array>
#include <cstdio>
int main(){std::array<std::byte,sizeof(vita::SectorRecord)> bytes;bytes.fill(std::byte{0xff});vita::NativeRecordView<vita::SectorRecord> view{bytes};auto record=view.at(0);if(record)std::printf("%d\n",int(record->start.has_value()));}
