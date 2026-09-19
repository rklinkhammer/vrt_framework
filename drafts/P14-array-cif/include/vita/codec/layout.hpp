#pragma once
#include <vita/fields/packet.hpp>
#include <vita/core/fixed_vector.hpp>
#include <bit>
#include <limits>
namespace vita {
constexpr Result<std::size_t> checked_add(std::size_t a,std::size_t b) noexcept {
    if(b>std::numeric_limits<std::size_t>::max()-a)return std::unexpected(Error{ErrorCode::overflow,a});
    return a+b;
}
constexpr Result<std::size_t> checked_multiply(std::size_t a,std::size_t b) noexcept {
    if(a && b>std::numeric_limits<std::size_t>::max()/a)return std::unexpected(Error{ErrorCode::overflow});
    return a*b;
}
struct TraversalLimits { std::size_t association_entries=1024,work_units=4096,index_entries=1024,records=256; };
struct FieldExtent { std::size_t bytes=0,units=1,index_entries=0,records=0; };
struct LayoutElement { FieldId field; Attribute attribute; std::size_t offset, bytes; };
struct LayoutMeasure { std::size_t bytes; std::uint64_t generation, signature; };

template<BodyKind K,std::size_t N,std::size_t A>
constexpr std::uint64_t layout_signature(const PacketSnapshot<K,N,A>& packet) noexcept {
    std::uint64_t hash=14695981039346656037ull;
    auto mix=[&](std::uint64_t value){hash=(hash^value)*1099511628211ull;};
    mix(static_cast<std::uint64_t>(K));mix(packet.layout().attributes);
    mix(static_cast<std::uint64_t>(packet.layout().subtype));mix(packet.layout().packet_class);mix(packet.layout().action);
    mix(packet.layout().timestamp_format.bound);mix(packet.layout().timestamp_format.tsi);mix(packet.layout().timestamp_format.tsf);
    for(const auto& e:packet.fields()) {
        mix(e.id.cif);mix(e.id.bit);
        if constexpr(A!=0)for(const auto& value:e.values)if(value && std::holds_alternative<NativeSlice>(*value)) {
            auto bytes=packet.native_value(*value);
            if(bytes) {
                if(e.id==IndexList::id) {
                    auto h=detail::native_object<detail::NativeIndexHeader>(*bytes);if(h){mix(h->count);mix(h->entry_bytes);}
                } else if(e.id==PointingVectorStructure::id) {
                    auto h=detail::native_object<detail::NativePointingHeader>(*bytes);if(h){mix(h->count);mix(bool(h->global));mix(h->record_reference);}
                } else if(e.id==SectorStepScan::id) {
                    auto h=detail::native_object<detail::NativeSectorHeader>(*bytes);if(h){mix(h->count);mix(h->selectors);if(h->selectors&(1u<<22)){mix(h->start_format.tsi);mix(h->start_format.tsf);}}
                } else if(e.id==GPSASCII::id) {
                    auto header=detail::native_object<detail::NativeAsciiHeader>(*bytes);
                    if(header)mix(header->characters);
                } else if(e.id==ContextAssociationLists::id) {
                    auto header=detail::native_object<detail::NativeAssociationHeader>(*bytes);
                    if(header){for(auto count:header->counts)mix(count);mix(header->tags_present);}
                } else mix(descriptor(e.id)->words);
            }
        }
    }
    return hash;
}

constexpr Result<FieldExtent> duration_extent(TimestampFormatBinding binding) noexcept {
    if(binding.tsi>3||binding.tsf>3)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(!binding.bound || (!binding.tsi&&!binding.tsf))return std::unexpected(Error{ErrorCode::unsupported_capability});
    const std::size_t words=(binding.tsi?1:0)+(binding.tsf?2:0);
    return FieldExtent{words*4,words};
}
inline Result<FieldExtent> native_structure_extent(FieldId id,Bytes bytes,TimestampFormatBinding binding={},Attribute attribute=Attribute::current) noexcept {
    if(id==IndexList::id) {
        auto v=detail::native_indices(bytes);if(!v)return std::unexpected(v.error());
        for(std::size_t i=0;i<v->size();++i){auto n=v->at(i);if(!n)return std::unexpected(n.error());if(v->entry_bytes<4&&*n>=(std::uint32_t{1}<<(8*v->entry_bytes)))return std::unexpected(Error{ErrorCode::invalid_argument});}
        return FieldExtent{8+((v->size()*v->entry_bytes+3)&~std::size_t{3}),2+v->size(),v->size(),0};
    }
    if(id==PointingVectorStructure::id) {
        auto v=detail::native_pointing(bytes);if(!v)return std::unexpected(v.error());
        auto shape=detail::pointing_shape(bool(v->global),v->record_reference,v->records.size());if(!shape)return std::unexpected(shape.error());
        if(v->global){auto valid=detail::validate_reference(*v->global,true);if(!valid)return std::unexpected(valid.error());}
        for(std::size_t i=0;i<v->records.size();++i){auto record=v->records.at(i);if(!record)return std::unexpected(record.error());auto valid=detail::validate_pointing({v->global,v->record_reference,{&*record,1}},attribute==Attribute::current);if(!valid)return std::unexpected(valid.error());}
        return FieldExtent{shape->total_words*4,shape->work,0,shape->records};
    }
    if(id==Spectrum::id) {
        if(bytes.size()!=sizeof(SpectrumValue))return std::unexpected(Error{ErrorCode::invalid_argument});
        auto v=detail::native_object<SpectrumValue>(bytes);if(!v)return std::unexpected(v.error());auto valid=detail::validate_spectrum(*v,attribute==Attribute::current);if(!valid)return std::unexpected(valid.error());return FieldExtent{52,13};
    }
    if(id==SectorStepScan::id) {
        auto v=detail::native_sectors(bytes);if(!v)return std::unexpected(v.error());
        auto shape=detail::sector_shape(v->selectors,v->records.size(),v->start_format);if(!shape)return std::unexpected(shape.error());
        if(v->selectors&(1u<<22)){if(!binding.bound)return std::unexpected(Error{ErrorCode::unsupported_capability});if(binding.tsi!=v->start_format.tsi||binding.tsf!=v->start_format.tsf)return std::unexpected(Error{ErrorCode::invalid_argument});}
        for(std::size_t i=0;i<v->records.size();++i){auto record=v->records.at(i);if(!record)return std::unexpected(record.error());auto valid=detail::validate_sector({v->selectors,v->start_format,{&*record,1}},attribute==Attribute::current);if(!valid)return std::unexpected(valid.error());}
        return FieldExtent{shape->total_words*4,shape->work,0,shape->records};
    }
    if(temporal_duration_field(id)) {
        if(bytes.size()!=sizeof(StateDurationValue))return std::unexpected(Error{ErrorCode::invalid_argument});
        auto value=detail::native_object<StateDurationValue>(bytes);if(!value)return std::unexpected(value.error());
        auto valid=detail::validate_native_attribute(*value,attribute==Attribute::current);if(!valid)return std::unexpected(valid.error());
        auto extent=duration_extent(binding);if(!extent)return std::unexpected(extent.error());
        if(value->tsi!=binding.tsi||value->tsf!=binding.tsf)return std::unexpected(Error{ErrorCode::invalid_argument});
        return extent;
    }
    if(id==ControlleeUUID::id || id==ControllerUUID::id) {
        if(bytes.size()!=sizeof(UuidValue))return std::unexpected(Error{ErrorCode::invalid_argument});
        auto value=detail::native_object<UuidValue>(bytes);if(!value)return std::unexpected(value.error());
        auto valid=detail::validate_native_attribute(*value,attribute==Attribute::current);if(!valid)return std::unexpected(valid.error());
        return FieldExtent{16,4};
    }
    if(id==FormattedGPS::id || id==FormattedINS::id) {
        if(bytes.size()!=sizeof(GeolocationValue))return std::unexpected(Error{ErrorCode::invalid_argument});
        auto value=detail::native_object<GeolocationValue>(bytes);if(!value)return std::unexpected(value.error());
        auto valid=detail::validate_native_attribute(*value,attribute==Attribute::current);if(!valid)return std::unexpected(valid.error());return FieldExtent{44,11};
    }
    if(id==ECEFEphemeris::id || id==RelativeEphemeris::id) {
        if(bytes.size()!=sizeof(EphemerisValue))return std::unexpected(Error{ErrorCode::invalid_argument});
        auto value=detail::native_object<EphemerisValue>(bytes);if(!value)return std::unexpected(value.error());
        auto valid=detail::validate_native_attribute(*value,attribute==Attribute::current);if(!valid)return std::unexpected(valid.error());return FieldExtent{52,13};
    }
    if(id==GPSASCII::id) {
        auto value=detail::native_ascii(bytes);if(!value)return std::unexpected(value.error());
        auto padded=checked_add(value->text.size(),3);if(!padded)return std::unexpected(padded.error());
        auto extent=checked_add(8,*padded&~std::size_t{3});if(!extent)return std::unexpected(extent.error());
        return FieldExtent{*extent,*extent-6};
    }
    if(id==ContextAssociationLists::id) {
        auto value=detail::native_associations(bytes);if(!value)return std::unexpected(value.error());
        const auto count=value->source.size()+value->system.size()+value->vector.size()+value->asynchronous.size()+value->tags.size();
        return FieldExtent{8+4*count,2+count};
    }
    return std::unexpected(Error{ErrorCode::unsupported_layout});
}

// One ordering/offset/work policy for native semantic and checked wire providers.
// Providers validate complete field extents before this core classifies work exhaustion.
struct GlobalFieldDescriptor { constexpr const FieldDescriptor* operator()(FieldId id) const noexcept { return descriptor(id); } };

template<BodyKind Body,bool VisitSelectors=false,class Snapshot,class Provider,class Visitor,class DescriptorResolver=GlobalFieldDescriptor>
constexpr Result<LayoutMeasure> walk_field_layout(const Snapshot& packet,Provider&& extent_provider,
        Visitor&& visitor,std::size_t start_offset=0,TraversalLimits limits={},std::size_t* shared_work=nullptr,DescriptorResolver resolve={}) noexcept {
    std::size_t words=1;
    for(unsigned i=1;i<8;++i)if(packet.layout().cif[0]&(1u<<i))++words;
    auto head=checked_add(start_offset,words*4);if(!head)return std::unexpected(head.error());
    std::size_t offset=*head,work=shared_work?*shared_work:0;
    for(const auto& field:packet.fields()) {
        const auto* d=resolve(field.id);
        if(!d)return std::unexpected(Error{ErrorCode::unsupported_layout,offset});
        const auto attributes=packet.layout().attributes?packet.layout().attributes:attribute_bit(Attribute::current);
        if(attributes&~d->attributes)return std::unexpected(Error{ErrorCode::unsupported_layout,offset});
        for(unsigned i=0;i<13;++i)if(attributes&attribute_bit(static_cast<Attribute>(i))) {
            const auto attribute=static_cast<Attribute>(i);
            Result<FieldExtent> extent=FieldExtent{Body==BodyKind::selectors?0u:4u,1};
            if constexpr(Body==BodyKind::values)extent=extent_provider(field,attribute,offset);
            if(!extent)return std::unexpected(extent.error());
            auto next=checked_add(offset,extent->bytes);if(!next)return std::unexpected(next.error());
            if(field.id==ContextAssociationLists::id && Body==BodyKind::values && base_attribute(attribute) && extent->units-2>limits.association_entries)
                return std::unexpected(Error{ErrorCode::resource_limit,offset});
            if(extent->index_entries>limits.index_entries||extent->records>limits.records)return std::unexpected(Error{ErrorCode::resource_limit,offset});
            if(work>limits.work_units || extent->units>limits.work_units-work)
                return std::unexpected(Error{ErrorCode::resource_limit,offset});
            work+=extent->units;
            if constexpr(Body!=BodyKind::selectors || VisitSelectors) {
                auto result=visitor(LayoutElement{field.id,attribute,offset,extent->bytes});
                if(!result)return std::unexpected(result.error());
            }
            offset=*next;
        }
    }
    if(shared_work)*shared_work=work;
    return LayoutMeasure{offset-start_offset,packet.generation(),layout_signature(packet)};
}

template<BodyKind K,std::size_t N,std::size_t A,class Visitor>
constexpr Result<LayoutMeasure> walk_layout(const PacketSnapshot<K,N,A>& packet,Visitor&& visitor,std::size_t start_offset=0) noexcept {
    auto provider=[&](const FieldEntry& field,Attribute attribute,std::size_t offset) noexcept -> Result<FieldExtent> {
        const auto& value=field.values[static_cast<unsigned>(attribute)];
        if(!value)return std::unexpected(Error{ErrorCode::invalid_argument,offset});
        const auto shape=attribute_shape(field.id,attribute,K);
        if(shape==AttributeShape::probability_word || shape==AttributeShape::belief_word) {
            auto valid=validate_attribute_value(field.id,attribute,*value);if(!valid)return std::unexpected(valid.error());return FieldExtent{4,1};
        }
        if(structured_field(field.id)) {
            auto bytes=packet.native_value(*value);if(!bytes)return std::unexpected(bytes.error());
            return native_structure_extent(field.id,*bytes,packet.layout().timestamp_format,attribute);
        }
        auto valid=validate_attribute_value(field.id,attribute,*value);if(!valid)return std::unexpected(valid.error());
        return FieldExtent{descriptor(field.id)->words*4,1};
    };
    return walk_field_layout<K>(packet,provider,std::forward<Visitor>(visitor),start_offset);
}
template<BodyKind K,std::size_t N,std::size_t A>
constexpr Result<LayoutMeasure> measure(const PacketSnapshot<K,N,A>& packet,std::size_t start_offset=0) noexcept {
    return walk_layout(packet,[](LayoutElement) noexcept -> Result<void>{return {};},start_offset);
}
template<BodyKind K,std::size_t N,std::size_t A>
constexpr Result<void> validate_measure(const PacketSnapshot<K,N,A>& packet,LayoutMeasure cached) noexcept {
    if(cached.generation!=packet.generation() || cached.signature!=layout_signature(packet))return std::unexpected(Error{ErrorCode::stale_generation});
    auto actual=measure(packet);if(!actual)return std::unexpected(actual.error());
    if(actual->bytes!=cached.bytes)return std::unexpected(Error{ErrorCode::invalid_argument});return {};
}
template<std::size_t Capacity,BodyKind K,std::size_t N,std::size_t A>
constexpr Result<FixedVector<LayoutElement,Capacity>> index_layout(const PacketSnapshot<K,N,A>& packet) noexcept {
    FixedVector<LayoutElement,Capacity> index;
    auto result=walk_layout(packet,[&](LayoutElement entry) noexcept -> Result<void>{return index.push_back(entry);});
    if(!result)return std::unexpected(result.error());return index;
}
} // namespace vita
