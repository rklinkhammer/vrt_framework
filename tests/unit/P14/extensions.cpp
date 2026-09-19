#include <vita/codec/extensions.hpp>
#include <array>
#include <cassert>
#include <cstdlib>
#include <new>
#include <type_traits>
using namespace vita;
using namespace vita::codec;
using namespace vita::codec::extensions;
static bool monitor=false;static unsigned allocations=0;
void* operator new(std::size_t n){if(monitor)++allocations;if(auto p=std::malloc(n?n:1))return p;std::abort();}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
struct Fixture {unsigned validations=0,executions=0,admissions=0;bool allowed=true,encoder_failure=false;};
Result<void> validate(void* raw,const EnvelopeView& view,WorkBudget& budget) noexcept {
    auto& f=*static_cast<Fixture*>(raw);++f.validations;
    auto work=budget.consume();if(!work)return work;
    // Registered validators may inspect the full prologue and exact backing span
    // on both receive and encode. This fails the former empty-wire encode path.
    auto checked=decode_envelope(view.wire);
    if(!checked || checked->payload_offset!=view.payload_offset || checked->trailer!=view.trailer ||
       checked->payload.data()!=view.payload.data() || checked->payload.size()!=view.payload.size() ||
       checked->envelope.type!=view.envelope.type || checked->envelope.class_id!=view.envelope.class_id)
        return std::unexpected(Error{ErrorCode::invalid_state});
    if(view.payload.size()!=4 || view.payload[0]!=std::byte{0x81} || view.payload[3]!=std::byte{0x19})return std::unexpected(Error{ErrorCode::invalid_argument});
    return {};
}
Result<std::size_t> size(void*,const void*,WorkBudget& budget) noexcept {
    auto w=budget.consume();if(!w)return std::unexpected(w.error());return 4;
}
Result<void> encode(void* raw,const void*,MutableBytes bytes,WorkBudget& budget) noexcept {
    auto w=budget.consume();if(!w)return w;
    bytes[0]=std::byte{0x81};bytes[1]=std::byte{0x23};bytes[2]=std::byte{0x45};bytes[3]=std::byte{0x19};
    if(static_cast<Fixture*>(raw)->encoder_failure)return std::unexpected(Error{ErrorCode::callback_failure});return {};
}
Result<void> dispatch(void* raw,const EnvelopeView&) noexcept {++static_cast<Fixture*>(raw)->executions;return {};}
Result<void> admit(void* raw,ClassKey,const EnvelopeView&) noexcept {
    auto& f=*static_cast<Fixture*>(raw);++f.admissions;
    if(!f.allowed)return std::unexpected(Error{ErrorCode::unsupported_capability});return {};
}
void all_family_contract() {
    Fixture f;Registry<4> registry;
    const std::array families{PacketType::extension_without_sid,PacketType::extension_data,
        PacketType::extension_context,PacketType::extension_command};
    for(auto family:families) {
        Descriptor d;d.key={family,0xff0002,0x100,0x200};d.context=&f;
        d.min_payload=d.max_payload=4;d.validate=&validate;d.measure=&size;d.encode=&encode;d.dispatch=&dispatch;
        d.header.tsi_mask=1u|1u<<static_cast<unsigned>(Tsi::gps);
        d.header.tsf_mask=1u|1u<<static_cast<unsigned>(Tsf::picoseconds);
        d.header.allowed_pad_counts=1u|1u<<3;
        if(is_data(family))d.header.allowed_flags=trailer_flag|nd0_flag|spectrum_flag;
        else if(family==PacketType::extension_context)d.header.allowed_flags=nd0_flag|tsm_flag;
        else {
            d.header.allowed_flags=ack_flag|cancel_flag;
            d.control_cam_extensions=2;d.ack_cam_extensions=4;
            d.header.controllee_kinds=1u<<static_cast<unsigned>(IdentifierKind::short_id);
            d.header.controller_kinds=1u<<static_cast<unsigned>(IdentifierKind::short_id);
        }
        assert(registry.add(d));
    }
    registry.freeze();WorkBudget work{4096};std::array<std::byte,96> output{};
    for(auto family:families) {
        const unsigned cases=is_data(family)?8:4;
        for(unsigned mode=0;mode<cases;++mode) {
            Envelope e;e.type=family;e.class_id=ClassId{0xff0002,0x100,0x200,3};
            if(family!=PacketType::extension_without_sid)e.stream_id=0x12345678;
            e.timestamp={Tsi::gps,Tsf::picoseconds,0x10203040,0x0102030405060708ULL};
            if(is_data(family)) {e.trailer=mode&1;e.nd0=mode&2;e.spectrum=mode&4;}
            else if(family==PacketType::extension_context){e.nd0=mode&1;e.tsm=mode&2;}
            else {
                e.ack=mode&1;e.cancel=mode&2;
                std::uint32_t cam=0xa0000000u; // explicit short Controller/Controllee IDs
                if(e.ack)cam|=(1u<<19)|4u; // AckX uses its own registered custom mask
                else if(e.cancel)cam|=(2u<<23)|(1u<<27)|(1u<<19)|2u;
                else cam|=(2u<<23)|2u;
                e.command=Command{cam,0x12345678,Identifier::short_id(10),Identifier::short_id(20)};
            }
            const auto trailer=e.trailer?std::optional<std::uint32_t>{0xdeadbeef}:std::nullopt;
            const auto encoded=registry.encode(e,nullptr,trailer,output,work);assert(encoded);
            auto packet=registry.validate(Bytes{output}.first(*encoded),work);assert(packet && !packet->opaque());
            const auto& received=packet->envelope();
            assert(received.envelope.type==family && received.envelope.stream_id==e.stream_id);
            assert(received.envelope.class_id==e.class_id && received.trailer==trailer);
            assert(received.envelope.timestamp.integer==0x10203040 && received.envelope.timestamp.fractional==0x0102030405060708ULL);
            assert(received.envelope.ack==e.ack && received.envelope.cancel==e.cancel && received.envelope.nd0==e.nd0 && received.envelope.spectrum==e.spectrum && received.envelope.tsm==e.tsm);
            assert(registry.dispatch(*packet,{&f,&admit}));
            // Header family is independently visible in the literal first nibble.
            assert((std::to_integer<unsigned>(output[0])>>4)==static_cast<unsigned>(family));
            if(e.command)assert(received.envelope.command->cam==e.command->cam);
            // Wrong timestamp/pad/trailer policies fail before output mutation.
            auto reject=[&](Envelope bad,std::optional<std::uint32_t> tail) {
                const auto old=output;const auto before=f.validations;
                assert(!registry.encode(bad,nullptr,tail,output,work));
                assert(output==old && f.validations==before);
            };
            auto bad=e;bad.timestamp.tsi=Tsi::utc;reject(bad,trailer);
            bad=e;bad.timestamp.tsf=Tsf::free_running;reject(bad,trailer);
            bad=e;bad.class_id->pad_bits=1;reject(bad,trailer);
            reject(e,e.trailer?std::nullopt:std::optional<std::uint32_t>{0});
            if(e.command) {
                bad=e;bad.command->cam|=e.ack?2u:4u;reject(bad,trailer);
                bad=e;bad.command->cam|=1u;reject(bad,trailer);
                bad=e;bad.command->controllee=Identifier::uuid({1,2,3,4});bad.command->cam|=1u<<30;reject(bad,trailer);
                bad=e;bad.command->cam&=~(1u<<31);bad.command->controllee={};reject(bad,trailer);
            }
        }
    }
    assert(f.executions==24 && f.admissions==24);
    // Required header option is independently enforced on a checked inbound wire.
    Registry<1> required;Descriptor d;d.key={PacketType::extension_data,0xff0003,1,2};d.context=&f;
    d.min_payload=d.max_payload=4;d.validate=&validate;d.header.allowed_flags=d.header.required_flags=trailer_flag;
    assert(required.add(d));required.freeze();
    Envelope e;e.type=PacketType::extension_data;e.stream_id=1;e.class_id=ClassId{0xff0003,1,2};
    const std::array<std::byte,4> payload{std::byte{0x81},std::byte{0x23},std::byte{0x45},std::byte{0x19}};
    auto size=encode_envelope(e,payload,std::nullopt,output);assert(size);const auto before=f.validations;
    assert(!required.validate(Bytes{output}.first(*size),work));assert(f.validations==before);
}
int main(){
    static_assert(!std::is_move_constructible_v<Registry<2>>);
    static_assert(!std::is_default_constructible_v<Registry<2>::ValidatedPacket>);
    Fixture fixture;Registry<2> registry;
    // Isolated test-only class; no production OUI/peer contract is advertised.
    Descriptor d;d.key={PacketType::extension_command,0xff0001,0x4321,0x1234};d.context=&fixture;
    d.min_payload=d.max_payload=4;d.control_cam_extensions=2;d.validate=&validate;d.measure=&size;d.encode=&encode;d.dispatch=&dispatch;
    assert(registry.add(d));assert(registry.add(d).error().code==ErrorCode::identity_conflict);
    auto bad=d;bad.key.packet_class=0x9999;bad.control_cam_extensions=1;assert(!registry.add(bad));
    bad=d;bad.key.family=PacketType::command;assert(!registry.add(bad));
    auto alternate=d;alternate.key.family=PacketType::extension_context;alternate.control_cam_extensions=0;assert(registry.add(alternate));
    auto extra=d;extra.key.packet_class=0x9999;assert(registry.add(extra).error().code==ErrorCode::capacity_exhausted);
    Envelope e;e.type=PacketType::extension_command;e.stream_id=7;e.class_id=ClassId{0xff0001,0x4321,0x1234};e.command=Command{(2u<<23)|2u,123};
    std::array<std::byte,64> out{};std::array<std::byte,4> body{std::byte{0x81},std::byte{0x23},std::byte{0x45},std::byte{0x19}};
    auto n=encode_envelope(e,body,std::nullopt,out);assert(n);WorkBudget budget;
    assert(!registry.validate(Bytes{out}.first(*n),budget));registry.freeze();assert(!registry.add(extra));
    monitor=true;
    // Validation borrows wire and the fixed-address registry/context; never executes.
    auto packet=registry.validate(Bytes{out}.first(*n),budget);assert(packet && !packet->opaque());
    const std::array<unsigned,28> literal{0x78,0,0,7,0,0,0,7,0,0xff,0,1,0x43,0x21,0x12,0x34,1,0,0,2,0,0,0,0x7b,0x81,0x23,0x45,0x19};
    assert(*n==literal.size());for(std::size_t i=0;i<literal.size();++i)assert(std::to_integer<unsigned>(out[i])==literal[i]);
    assert(fixture.validations==1 && fixture.executions==0 && fixture.admissions==0);
    assert(packet->envelope().payload.data()==out.data()+packet->envelope().payload_offset);
    assert(!registry.dispatch(*packet,{}));assert(fixture.executions==0);
    fixture.allowed=false;assert(!registry.dispatch(*packet,{&fixture,&admit}));assert(fixture.executions==0);
    fixture.allowed=true;assert(registry.dispatch(*packet,{&fixture,&admit}));assert(registry.dispatch(*packet,{&fixture,&admit}));
    assert(fixture.executions==2 && fixture.admissions==3); // admission on each call, not a replay manager
    Registry<2> other;assert(other.add(d));other.freeze();assert(!other.dispatch(*packet,{&fixture,&admit}));
    // Full identity includes InformationClass, packet class and packet family.
    for(unsigned variant=0;variant<3;++variant){
        auto unknown=e;if(variant==0)unknown.class_id->information_class++;if(variant==1)unknown.class_id->packet_class++;if(variant==2)unknown.class_id.reset();
        n=encode_envelope(unknown,body,std::nullopt,out);assert(n);
        auto opaque=registry.validate(Bytes{out}.first(*n),budget);assert(opaque && opaque->opaque());assert(!registry.dispatch(*opaque,{&fixture,&admit}));
    }
    // Different permitted custom mask and bit0 are rejected before class validation.
    const auto before=fixture.validations;
    auto denied=e;denied.command->cam|=4;n=encode_envelope(denied,body,std::nullopt,out);assert(n);assert(!registry.validate(Bytes{out}.first(*n),budget));
    denied=e;denied.command->cam|=1;n=encode_envelope(denied,body,std::nullopt,out);assert(!n);assert(fixture.validations==before);
    denied=e;denied.timestamp={Tsi::gps,Tsf::none,1,0};n=encode_envelope(denied,body,std::nullopt,out);assert(n);assert(!registry.validate(Bytes{out}.first(*n),budget));
    n=encode_envelope(e,body,std::nullopt,out);assert(n);
    WorkBudget exhausted{1};assert(registry.validate(Bytes{out}.first(*n),exhausted).error().code==ErrorCode::resource_limit);
    assert(!registry.validate(Bytes{out}.first(*n-1),budget));out[*n-1]=std::byte{0};assert(!registry.validate(Bytes{out}.first(*n),budget));assert(fixture.executions==2);
    // Preflight errors preserve output; callbacks never run for insufficient capacity.
    for(auto& b:out)b=std::byte{0xaa};auto initial=fixture.validations;
    assert(!registry.encode(e,nullptr,std::nullopt,MutableBytes{out}.first(1),budget));
    for(auto b:out)assert(b==std::byte{0xaa});assert(fixture.validations==initial);
    auto emitted=registry.encode(e,nullptr,std::nullopt,out,budget);assert(emitted);
    auto parsed=registry.validate(Bytes{out}.first(*emitted),budget);assert(parsed && !parsed->opaque());assert(fixture.executions==2);
    // Application-owned native state/context failure has explicitly partial output.
    fixture.encoder_failure=true;assert(registry.encode(e,nullptr,std::nullopt,out,budget).error().code==ErrorCode::callback_failure);
    fixture.encoder_failure=false;
    auto context=e;context.type=PacketType::extension_context;context.command.reset();
    assert(registry.encode(context,nullptr,std::nullopt,out,budget));
    Registry<1> data_registry;auto data_descriptor=d;
    data_descriptor.key.family=PacketType::extension_data;
    data_descriptor.control_cam_extensions=0;
    data_descriptor.header.allowed_flags=trailer_flag;
    data_descriptor.header.required_flags=trailer_flag;
    assert(data_registry.add(data_descriptor));data_registry.freeze();
    auto data=context;data.type=PacketType::extension_data;data.trailer=true;
    const auto with_trailer=data_registry.encode(data,nullptr,0x81234567u,out,budget);assert(with_trailer);
    auto data_packet=data_registry.validate(Bytes{out}.first(*with_trailer),budget);assert(data_packet);
    assert(data_packet->envelope().trailer==0x81234567u);
    assert(data_packet->envelope().wire.size()==*with_trailer);
    all_family_contract();
    monitor=false;assert(allocations==0);
}
