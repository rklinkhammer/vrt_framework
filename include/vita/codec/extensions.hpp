#pragma once
#include <vita/codec/wire.hpp>
#include <array>
#include <limits>

namespace vita::codec::extensions {
struct ClassKey {
    PacketType family=PacketType::extension_data;
    std::uint32_t oui=0;
    std::uint16_t information_class=0,packet_class=0;
    friend bool operator==(ClassKey,ClassKey)=default;
};
enum HeaderFlag : unsigned { trailer_flag=1,nd0_flag=2,spectrum_flag=4,tsm_flag=8,ack_flag=16,cancel_flag=32 };
struct HeaderRules {
    unsigned allowed_flags=0,required_flags=0;
    unsigned tsi_mask=1,tsf_mask=1; // bit(code) allowed, default no timestamp
    unsigned controllee_kinds=7,controller_kinds=7; // absent/short/UUID
    std::uint32_t allowed_pad_counts=1; // bit(count), default zero padding
};
class WorkBudget {
    std::size_t remaining_;
public:
    explicit WorkBudget(std::size_t units=4096) noexcept:remaining_(units){}
    std::size_t remaining() const noexcept { return remaining_; }
    Result<void> consume(std::size_t units=1) noexcept {
        if(units>remaining_)return std::unexpected(Error{ErrorCode::resource_limit});
        remaining_-=units;return {};
    }
};
struct Descriptor {
    ClassKey key{};
    HeaderRules header{};
    std::size_t min_payload=0,max_payload=65535u*4;
    std::uint8_t control_cam_extensions=0,ack_cam_extensions=0;
    // Borrowed setup context; must outlive registry and every validated view.
    void* context=nullptr;
    Result<void>(*validate)(void*,const EnvelopeView&,WorkBudget&) noexcept=nullptr;
    Result<std::size_t>(*measure)(void*,const void*,WorkBudget&) noexcept=nullptr;
    Result<void>(*encode)(void*,const void*,MutableBytes,WorkBudget&) noexcept=nullptr;
    Result<void>(*dispatch)(void*,const EnvelopeView&) noexcept=nullptr;
};
struct Authorization {
    void* context=nullptr;
    // Host performs authorization AND reserves required admission here, per dispatch.
    Result<void>(*admit)(void*,ClassKey,const EnvelopeView&) noexcept=nullptr;
};
namespace detail {
inline unsigned flags(const Envelope& e) noexcept {
    return (e.trailer?trailer_flag:0u)|(e.nd0?nd0_flag:0u)|(e.spectrum?spectrum_flag:0u)|
        (e.tsm?tsm_flag:0u)|(e.ack?ack_flag:0u)|(e.cancel?cancel_flag:0u);
}
inline Result<void> class_policy(const Descriptor& d,const Envelope& e,std::size_t payload) noexcept {
    if(!e.class_id || ClassKey{e.type,e.class_id->oui,e.class_id->information_class,e.class_id->packet_class}!=d.key)
        return std::unexpected(Error{ErrorCode::unsupported_capability});
    const auto f=flags(e);
    if((f&~d.header.allowed_flags) || (f&d.header.required_flags)!=d.header.required_flags ||
       !(d.header.tsi_mask&(1u<<static_cast<unsigned>(e.timestamp.tsi))) ||
       !(d.header.tsf_mask&(1u<<static_cast<unsigned>(e.timestamp.tsf))) ||
       !(d.header.allowed_pad_counts&(std::uint32_t{1}<<e.class_id->pad_bits)))
        return std::unexpected(Error{ErrorCode::unsupported_capability});
    if(payload<d.min_payload || payload>d.max_payload)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(e.command) {
        const auto permitted=e.ack?d.ack_cam_extensions:d.control_cam_extensions;
        if((e.command->cam&0xffu)&~permitted)return std::unexpected(Error{ErrorCode::unsupported_capability});
        if(!(d.header.controllee_kinds&(1u<<static_cast<unsigned>(e.command->controllee.kind))) ||
           !(d.header.controller_kinds&(1u<<static_cast<unsigned>(e.command->controller.kind))))
            return std::unexpected(Error{ErrorCode::unsupported_capability});
    }
    return {};
}
}

template<std::size_t Capacity> class Registry {
    std::array<Descriptor,Capacity> entries_{};
    std::size_t size_=0;
    bool frozen_=false;
    static constexpr std::size_t absent=std::numeric_limits<std::size_t>::max();
    std::size_t find(const Envelope& e) const noexcept {
        if(!e.class_id)return absent;
        const ClassKey key{e.type,e.class_id->oui,e.class_id->information_class,e.class_id->packet_class};
        for(std::size_t i=0;i<size_;++i)if(entries_[i].key==key)return i;
        return absent;
    }
public:
    class ValidatedPacket {
        const Registry* registry_;
        std::size_t index_;
        EnvelopeView view_;
        ValidatedPacket(const Registry* registry,std::size_t index,EnvelopeView view) noexcept:registry_(registry),index_(index),view_(view){}
        friend class Registry;
    public:
        bool opaque() const noexcept { return index_==absent; }
        const EnvelopeView& envelope() const & noexcept { return view_; }
        const EnvelopeView& envelope() const &&=delete;
    };
    Registry()=default;
    Registry(const Registry&)=delete;Registry& operator=(const Registry&)=delete;
    Registry(Registry&&)=delete;Registry& operator=(Registry&&)=delete;
    Result<void> add(Descriptor d) noexcept {
        if(frozen_)return std::unexpected(Error{ErrorCode::invalid_state});
        if(!is_extension(d.key.family) || static_cast<unsigned>(d.key.family)>7 || d.key.oui>0xffffff ||
           d.min_payload>d.max_payload || d.max_payload>65535u*4 || d.min_payload%4 || d.max_payload%4 ||
           !d.validate || bool(d.measure)!=bool(d.encode) ||
           ((d.control_cam_extensions|d.ack_cam_extensions)&~0xfeu) ||
           (d.header.allowed_flags&~63u) || (d.header.required_flags&~d.header.allowed_flags) ||
           !d.header.tsi_mask || (d.header.tsi_mask&~15u) || !d.header.tsf_mask || (d.header.tsf_mask&~15u) ||
           !d.header.controllee_kinds || (d.header.controllee_kinds&~7u) || !d.header.controller_kinds || (d.header.controller_kinds&~7u) || !d.header.allowed_pad_counts)
            return std::unexpected(Error{ErrorCode::invalid_argument});
        if(d.key.family!=PacketType::extension_command && (d.control_cam_extensions || d.ack_cam_extensions))
            return std::unexpected(Error{ErrorCode::invalid_argument});
        for(std::size_t i=0;i<size_;++i)if(entries_[i].key==d.key)return std::unexpected(Error{ErrorCode::identity_conflict});
        if(size_==Capacity)return std::unexpected(Error{ErrorCode::capacity_exhausted});
        entries_[size_++]=d;return {};
    }
    void freeze() noexcept { frozen_=true; }
    bool frozen() const noexcept { return frozen_; }
    std::size_t size() const noexcept { return size_; }
    // Registry, descriptor contexts, and immutable wire must outlive this borrow.
    Result<ValidatedPacket> validate(Bytes wire,WorkBudget& work) const & noexcept {
        if(!frozen_)return std::unexpected(Error{ErrorCode::invalid_state});
        auto view=decode_envelope(wire);if(!view)return std::unexpected(view.error());
        if(!is_extension(view->envelope.type))return std::unexpected(Error{ErrorCode::invalid_argument});
        auto charged=work.consume(1+view->payload.size()/4);if(!charged)return std::unexpected(charged.error());
        // Bit0 is not an allowed vendor CAM bit, even for opaque unknown classes.
        if(view->envelope.command && (view->envelope.command->cam&1u))return std::unexpected(Error{ErrorCode::invalid_argument});
        const auto index=find(view->envelope);
        if(index!=absent) {
            const auto& d=entries_[index];auto policy=detail::class_policy(d,view->envelope,view->payload.size());
            if(!policy)return std::unexpected(policy.error());
            auto valid=d.validate(d.context,*view,work);if(!valid)return std::unexpected(valid.error());
        }
        return ValidatedPacket{this,index,*view};
    }
    Result<ValidatedPacket> validate(Bytes,WorkBudget&) const &&=delete;
    Result<void> dispatch(const ValidatedPacket& packet,Authorization authorization) const noexcept {
        if(!frozen_ || packet.registry_!=this || packet.opaque())return std::unexpected(Error{ErrorCode::unsupported_capability});
        const auto& d=entries_[packet.index_];
        if(!authorization.admit || !d.dispatch)return std::unexpected(Error{ErrorCode::unsupported_capability});
        auto admitted=authorization.admit(authorization.context,d.key,packet.view_);if(!admitted)return admitted;
        return d.dispatch(d.context,packet.view_);
    }
    // Framework preflight failures leave output untouched. Once the user encoder
    // runs, callback failure may leave partial payload bytes: no hidden staging.
    Result<std::size_t> encode(const Envelope& envelope,const void* state,std::optional<std::uint32_t> trailer,
                               MutableBytes output,WorkBudget& work) const noexcept {
        if(!frozen_)return std::unexpected(Error{ErrorCode::invalid_state});
        const auto index=find(envelope);
        if(index==absent)return std::unexpected(Error{ErrorCode::unsupported_capability});
        const auto& d=entries_[index];
        if(!d.measure || !d.encode)return std::unexpected(Error{ErrorCode::unsupported_capability});
        auto charged=work.consume();if(!charged)return std::unexpected(charged.error());
        auto payload=d.measure(d.context,state,work);if(!payload)return std::unexpected(payload.error());
        // measure_envelope validates enum ranges before class_policy shifts codes.
        auto total=measure_envelope(envelope,*payload);if(!total)return std::unexpected(total.error());
        if(envelope.trailer!=trailer.has_value())return std::unexpected(Error{ErrorCode::invalid_argument});
        auto policy=detail::class_policy(d,envelope,*payload);if(!policy)return std::unexpected(policy.error());
        if(output.size()<*total)return std::unexpected(Error{ErrorCode::short_output,0,*total});
        charged=work.consume(*payload/4);if(!charged)return std::unexpected(charged.error());
        const auto offset=*total-*payload-(trailer?4:0);
        auto target=output.subspan(offset,*payload);
        auto encoded=d.encode(d.context,state,target,work);if(!encoded)return std::unexpected(encoded.error());
        auto written=encode_envelope(envelope,target,trailer,output);
        if(!written)return std::unexpected(written.error());
        // Validate the complete serialized packet, with exactly the same checked
        // EnvelopeView contract as receive. No semantic dispatch or allocation.
        auto candidate=decode_envelope(Bytes{output}.first(*written));
        if(!candidate)return std::unexpected(candidate.error());
        auto valid=d.validate(d.context,*candidate,work);if(!valid)return std::unexpected(valid.error());
        return *written;
    }
};
} // namespace vita::codec::extensions
