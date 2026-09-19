#include <vita/codec/layout.hpp>
#include <vita/fields/arena.hpp>
#include <cassert>
#include <memory>
int main() {
    using namespace vita;
    static_assert(sizeof(PacketSnapshot<BodyKind::values>) < 8192);
    ControlPacket control;assert(control.freeze().layout().action==2);assert(control.configure(32,1));assert(!control.configure(32,0));
    CancelPacket cancel;assert(cancel.freeze().layout().subtype==PacketSubtype::cancel);assert(cancel.freeze().layout().action==2);
    ContextPacket p;
    auto rate=Hertz::from_integer(1'000'000);assert(rate);
    assert(p.set<SampleRate>(*rate));
    const auto old=p.freeze();const auto old_measure=measure(old);assert(old_measure && old_measure->bytes==12);
    const auto mask=supported_scalar_attributes;
    assert(!p.with_attributes(mask));assert(p.generation()==old.generation());
    std::array additions{AttributeValue{SampleRate::id,Attribute::minimum,Hertz{1<<20}},AttributeValue{SampleRate::id,Attribute::maximum,Hertz{100'000'000ll<<20}}};
    assert(p.with_attributes(mask,additions));assert(measure(p.freeze())->bytes==32);
    assert(!validate_measure(p.freeze(),*old_measure));assert(measure(old)->bytes==12);
    auto index=index_layout<3>(p.freeze());assert(index && index->size()==3);
    assert(!index_layout<2>(p.freeze()));
    QueryPacket query;assert(!query.select(FieldId{0,7}));assert(!query.select(FieldId{0,1}));assert(!query.select(FieldId{0,31}));assert(query.select<SampleRate>());assert(measure(query.freeze())->bytes==4);
    assert(query.with_attributes(mask));assert(measure(query.freeze())->bytes==8);
    DiagnosticAck ack;assert(ack.diagnostic(SampleRate::id,0x80000000));assert(measure(ack.freeze())->bytes==8);
    assert(ack.freeze().layout().subtype==PacketSubtype::diagnostic_ack);
    assert(!checked_add(SIZE_MAX,1));assert(!checked_multiply(SIZE_MAX,2));
    assert(!measure(old,SIZE_MAX-3));
    QueryPacket unknown;assert(unknown.select(FieldId{1,31}));assert(!measure(unknown.freeze()));
    PacketBuilder<BodyKind::values,1> bounded;assert(bounded.set<SampleRate>(*rate));assert(!bounded.set<ReferencePoint>(4));assert(bounded.freeze().fields().size()==1);
    SemanticArena<2> arena;std::array<std::uint32_t,2> words{5,6};auto slice=arena.append(words);assert(slice && (*arena.view(*slice))[1]==6);assert(!arena.append(words));assert(arena.size()==2);assert(!arena.view({SIZE_MAX,1}));
    FixedVector<std::unique_ptr<int>,1> fixed;assert(fixed.push_back(std::make_unique<int>(7)));assert(!fixed.push_back(std::make_unique<int>(8)));assert(*fixed[0]==7);fixed.clear();assert(fixed.empty());
    return 0;
}
