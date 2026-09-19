#pragma once
#include <vita/fields/types.hpp>
#include <vita/fields/arena.hpp>
#include <array>
#include <optional>
#include <span>
#include <limits>
#include <bit>
namespace vita {
enum class PacketFamily : std::uint8_t { signal_data=1, context=4, command=6, extension_data=3, extension_context=5, extension_command=7 };
enum class BodyKind { values, selectors, diagnostics };
enum class AttributeShape { selector,diagnostic_word,probability_word,belief_word,base_field };
constexpr AttributeShape attribute_shape(FieldId,Attribute a,BodyKind body) noexcept {
    if(body==BodyKind::selectors)return AttributeShape::selector;
    if(body==BodyKind::diagnostics)return AttributeShape::diagnostic_word;
    return a==Attribute::probability?AttributeShape::probability_word:a==Attribute::belief?AttributeShape::belief_word:AttributeShape::base_field;
}
enum class PacketSubtype { signal, context, control, query, cancel, diagnostic_ack, state_ack };
struct LayoutContext {
    PacketFamily family = PacketFamily::context;
    TimestampFormatBinding timestamp_format{};
    PacketSubtype subtype = PacketSubtype::context;
    std::uint8_t action = 0;
    std::uint16_t packet_class = 0;
    std::array<std::uint32_t,8> cif{};
    std::uint32_t attributes = 0; // zero means implicit Current, no CIF7 word
};
struct FieldEntry {
    FieldId id{};
    std::array<std::optional<SemanticValue>,13> values{};
    std::uint32_t diagnostic = 0;
};
struct AttributeValue { FieldId field; Attribute attribute; SemanticValue value; };
using InputValue=std::variant<SemanticValue,GeolocationValue,EphemerisValue,GpsAsciiInput,AssociationListsInput,UuidValue,StateDurationValue,IndexListInput,PointingVectorInput,SpectrumValue,SectorStepScanInput>;
struct AttributeInput { FieldId field;Attribute attribute;InputValue value; };
template<BodyKind,std::size_t,std::size_t> class EditWorkspace;
template<BodyKind Kind, std::size_t N, std::size_t NativeBytes> class PacketBuilder;
template<BodyKind Kind, std::size_t N = 16, std::size_t NativeBytes = 0> class PacketSnapshot {
    LayoutContext layout_{};
    std::array<FieldEntry,N> entries_{};
    std::size_t size_ = 0;
    std::uint64_t generation_ = 0;
    [[no_unique_address]] detail::NativeArenaStorage<NativeBytes> native_{};
    friend class PacketBuilder<Kind,N,NativeBytes>;
public:
    static constexpr BodyKind body_kind = Kind;
    constexpr const LayoutContext& layout() const noexcept { return layout_; }
    constexpr std::span<const FieldEntry> fields() const noexcept { return {entries_.data(),size_}; }
    constexpr std::uint64_t generation() const noexcept { return generation_; }
    std::size_t native_size() const noexcept { return native_.size(); }
    Result<Bytes> native_value(const SemanticValue& value) const & noexcept {
        bool owned=false;
        for(const auto& entry:fields())for(const auto& candidate:entry.values)
            if(candidate && &*candidate==&value)owned=true;
        if(!owned)return std::unexpected(Error{ErrorCode::invalid_argument});
        const auto* ref=std::get_if<NativeSlice>(&value);
        if(!ref)return std::unexpected(Error{ErrorCode::invalid_argument});
        return native_.view(ref->offset(),ref->bytes());
    }
    Result<Bytes> native_value(const SemanticValue&) const && = delete;
    template<class Field> Result<typename Field::view_type> get(Attribute attribute=Attribute::current) const & noexcept {
        if(!base_attribute(attribute))return std::unexpected(Error{ErrorCode::invalid_argument});
        const auto a=static_cast<unsigned>(attribute);
        const SemanticValue* value=nullptr;
        for(const auto& entry:fields())if(entry.id==Field::id && entry.values[a])value=&*entry.values[a];
        if(!value)return std::unexpected(Error{ErrorCode::invalid_argument});
        auto bytes=native_value(*value);if(!bytes)return std::unexpected(bytes.error());
        if constexpr(std::is_same_v<typename Field::view_type,GpsAsciiView>)return detail::native_ascii(*bytes);
        else if constexpr(std::is_same_v<typename Field::view_type,AssociationListsView>)return detail::native_associations(*bytes);
        else if constexpr(std::is_same_v<typename Field::view_type,IndexListView>)return detail::native_indices(*bytes);
        else if constexpr(std::is_same_v<typename Field::view_type,PointingVectorView>)return detail::native_pointing(*bytes);
        else if constexpr(std::is_same_v<typename Field::view_type,SectorStepScanView>)return detail::native_sectors(*bytes);
        else {
            if(bytes->size()!=sizeof(typename Field::view_type))return std::unexpected(Error{ErrorCode::invalid_argument});
            return detail::native_object<typename Field::view_type>(*bytes);
        }
    }
    template<class Field> Result<typename Field::view_type> get(Attribute=Attribute::current) const && = delete;
    Result<ProbabilityCode> probability(FieldId id) const noexcept {
        for(const auto& e:fields())if(e.id==id&&e.values[11])if(auto p=std::get_if<ProbabilityCode>(&*e.values[11]))return *p;
        return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    Result<BeliefCode> belief(FieldId id) const noexcept {
        for(const auto& e:fields())if(e.id==id&&e.values[12])if(auto p=std::get_if<BeliefCode>(&*e.values[12]))return *p;
        return std::unexpected(Error{ErrorCode::invalid_argument});
    }
};
template<BodyKind Kind,std::size_t N=16,std::size_t NativeBytes=0> class EditWorkspace {
    PacketSnapshot<Kind,N,NativeBytes> candidate_{};
    friend class PacketBuilder<Kind,N,NativeBytes>;
};
template<BodyKind Kind, std::size_t N = 16, std::size_t NativeBytes = 0> class PacketBuilder {
    PacketSnapshot<Kind,N,NativeBytes> state_{};
    Result<void> compact_native(std::optional<FieldId> skip={}) noexcept {
        if constexpr(NativeBytes==0)return {};
        else {
            detail::NativeArenaStorage<NativeBytes> fresh;
            for(std::size_t i=0;i<state_.size_;++i) {
                auto& entry=state_.entries_[i];if(skip && entry.id==*skip)continue;
                for(auto& value:entry.values)if(value && std::holds_alternative<NativeSlice>(*value)) {
                    auto bytes=state_.native_value(*value);if(!bytes)return std::unexpected(bytes.error());
                    auto offset=fresh.append(*bytes);if(!offset)return std::unexpected(offset.error());
                    value=NativeSlice{*offset,static_cast<std::uint32_t>(bytes->size())};
                }
            }
            state_.native_=fresh;return {};
        }
    }
    template<class T> Result<void> append_native(PacketSnapshot<Kind,N,NativeBytes>& candidate,FieldId id,Attribute attr,const T& value,SemanticValue& result) noexcept {
        bool matching=false;
        if constexpr(std::is_same_v<T,GeolocationValue>)matching=id==FormattedGPS::id||id==FormattedINS::id;
        else if constexpr(std::is_same_v<T,EphemerisValue>)matching=id==ECEFEphemeris::id||id==RelativeEphemeris::id;
        else if constexpr(std::is_same_v<T,GpsAsciiInput>)matching=id==GPSASCII::id;
        else if constexpr(std::is_same_v<T,AssociationListsInput>)matching=id==ContextAssociationLists::id;
        else if constexpr(std::is_same_v<T,UuidValue>)matching=id==ControlleeUUID::id||id==ControllerUUID::id;
        else if constexpr(std::is_same_v<T,StateDurationValue>)matching=temporal_duration_field(id);
        else if constexpr(std::is_same_v<T,IndexListInput>)matching=id==IndexList::id;
        else if constexpr(std::is_same_v<T,PointingVectorInput>)matching=id==PointingVectorStructure::id;
        else if constexpr(std::is_same_v<T,SpectrumValue>)matching=id==Spectrum::id;
        else if constexpr(std::is_same_v<T,SectorStepScanInput>)matching=id==SectorStepScan::id;
        if(!matching||!base_attribute(attr))return std::unexpected(Error{ErrorCode::invalid_argument});
        if constexpr(std::is_same_v<T,GpsAsciiInput>) {
            if(value.text.size()>UINT32_MAX)return std::unexpected(Error{ErrorCode::overflow});
            if constexpr(NativeBytes<sizeof(detail::NativeAsciiHeader))return std::unexpected(Error{ErrorCode::capacity_exhausted});
            else if(value.text.size()>NativeBytes-sizeof(detail::NativeAsciiHeader))return std::unexpected(Error{ErrorCode::capacity_exhausted});
        }
        auto valid=detail::validate_native_attribute(value,attr==Attribute::current);if(!valid)return valid;
        if constexpr(std::is_same_v<T,StateDurationValue>) {
            const auto binding=candidate.layout_.timestamp_format;
            if(binding.bound&&(binding.tsi!=value.tsi||binding.tsf!=value.tsf))return std::unexpected(Error{ErrorCode::invalid_argument});
        }
        if constexpr(std::is_same_v<T,SectorStepScanInput>)if(value.selectors&(1u<<22)) {
            const auto binding=candidate.layout_.timestamp_format;
            if(binding.bound&&(binding.tsi!=value.start_format.tsi||binding.tsf!=value.start_format.tsf))return std::unexpected(Error{ErrorCode::invalid_argument});
        }
        const auto start=candidate.native_.size();
        auto append=[&](Bytes bytes) noexcept -> Result<void> {auto r=candidate.native_.append(bytes);if(!r)return std::unexpected(r.error());return {};};
        if constexpr(std::is_same_v<T,GpsAsciiInput>) {
            const detail::NativeAsciiHeader h{value.oui,static_cast<std::uint32_t>(value.text.size())};
            auto r=append(detail::object_bytes(h));if(!r)return r;
            r=append({reinterpret_cast<const std::byte*>(value.text.data()),value.text.size()});if(!r)return r;
        } else if constexpr(std::is_same_v<T,AssociationListsInput>) {
            const std::array lists{value.source,value.system,value.vector,value.asynchronous,value.tags};
            detail::NativeAssociationHeader h{};h.tags_present=value.tags_present;
            for(unsigned i=0;i<5;++i)h.counts[i]=static_cast<std::uint32_t>(lists[i].size());
            auto r=append(detail::object_bytes(h));if(!r)return r;
            for(auto list:lists){r=append({reinterpret_cast<const std::byte*>(list.data()),list.size_bytes()});if(!r)return r;}
        } else if constexpr(std::is_same_v<T,IndexListInput>) {
            const detail::NativeIndexHeader h{static_cast<std::uint32_t>(value.entries.size()),value.entry_bytes};
            auto r=append(detail::object_bytes(h));if(!r)return r;
            r=append({reinterpret_cast<const std::byte*>(value.entries.data()),value.entries.size_bytes()});if(!r)return r;
        } else if constexpr(std::is_same_v<T,PointingVectorInput>) {
            const detail::NativePointingHeader h{static_cast<std::uint32_t>(value.records.size()),value.global,value.record_reference};
            auto r=append(detail::object_bytes(h));if(!r)return r;
            r=append({reinterpret_cast<const std::byte*>(value.records.data()),value.records.size_bytes()});if(!r)return r;
        } else if constexpr(std::is_same_v<T,SectorStepScanInput>) {
            const detail::NativeSectorHeader h{static_cast<std::uint32_t>(value.records.size()),value.selectors,value.start_format};
            auto r=append(detail::object_bytes(h));if(!r)return r;
            r=append({reinterpret_cast<const std::byte*>(value.records.data()),value.records.size_bytes()});if(!r)return r;
        } else {auto r=append(detail::object_bytes(value));if(!r)return r;}
        result=NativeSlice{static_cast<std::uint32_t>(start),static_cast<std::uint32_t>(candidate.native_.size()-start)};return {};
    }
    template<class Input> constexpr Result<void> edit(std::uint32_t mask,std::span<const Input> supplied,EditWorkspace<Kind,N,NativeBytes>& workspace,std::optional<FieldId> added={}) noexcept {
        if(mask&~all_attributes || supplied.size()>N*13)return std::unexpected(Error{ErrorCode::invalid_argument});
        if(state_.generation_==UINT64_MAX)return std::unexpected(Error{ErrorCode::overflow});
        const auto selected=mask?mask:attribute_bit(Attribute::current);
        if constexpr(Kind!=BodyKind::values)if(!supplied.empty())return std::unexpected(Error{ErrorCode::invalid_argument});
        auto& candidate=workspace.candidate_;
        candidate.layout_=state_.layout_;candidate.layout_.attributes=mask;candidate.size_=state_.size_;candidate.native_.clear();
        for(auto& entry:candidate.entries_)entry={};
        for(std::size_t i=0;i<state_.size_;++i){candidate.entries_[i].id=state_.entries_[i].id;candidate.entries_[i].diagnostic=state_.entries_[i].diagnostic;}
        if(added) {
            if(!descriptor(*added))return std::unexpected(Error{ErrorCode::unsupported_layout});
            std::size_t at=0;while(at<candidate.size_&&field_before(candidate.entries_[at].id,*added))++at;
            if(at==candidate.size_||candidate.entries_[at].id!=*added) {
                if(candidate.size_==N)return std::unexpected(Error{ErrorCode::capacity_exhausted});
                for(auto i=candidate.size_;i>at;--i)candidate.entries_[i]=candidate.entries_[i-1];
                candidate.entries_[at]={};candidate.entries_[at].id=*added;++candidate.size_;
            }
        }
        std::array<std::uint16_t,N> seen{};
        for(const auto& input:supplied) {
            const auto a=static_cast<unsigned>(input.attribute);
            if(a>=13||!(selected&attribute_bit(input.attribute)))return std::unexpected(Error{ErrorCode::invalid_argument});
            std::size_t i=0;while(i<candidate.size_&&candidate.entries_[i].id!=input.field)++i;
            if(i==candidate.size_||(seen[i]&(1u<<a)))return std::unexpected(Error{ErrorCode::invalid_argument});
            seen[i]|=1u<<a;
        }
        for(std::size_t i=0;i<candidate.size_;++i) {
            auto& entry=candidate.entries_[i];const FieldEntry* old=nullptr;
            for(const auto& previous:state_.fields())if(previous.id==entry.id)old=&previous;
            if(!descriptor(entry.id))return std::unexpected(Error{ErrorCode::unsupported_layout});
            for(unsigned a=0;a<13;++a)if(selected&attribute_bit(static_cast<Attribute>(a))) {
                if constexpr(Kind==BodyKind::values) {
                    const Input* input=nullptr;for(const auto& value:supplied)if(value.field==entry.id&&static_cast<unsigned>(value.attribute)==a)input=&value;
                    SemanticValue value{};
                    if(input) {
                        Result<void> valid;
                        if constexpr(std::is_same_v<Input,AttributeValue>) {valid=validate_attribute_value(entry.id,static_cast<Attribute>(a),input->value);value=input->value;}
                        else valid=std::visit([&](const auto& v) noexcept -> Result<void> {
                            using T=std::remove_cvref_t<decltype(v)>;
                            if constexpr(std::is_same_v<T,SemanticValue>){auto r=validate_attribute_value(entry.id,static_cast<Attribute>(a),v);if(r)value=v;return r;}
                            else return append_native(candidate,entry.id,static_cast<Attribute>(a),v,value);
                        },input->value);
                        if(!valid)return valid;
                    } else {
                        if(!old||!old->values[a])return std::unexpected(Error{ErrorCode::invalid_argument});
                        value=*old->values[a];
                        if(std::holds_alternative<NativeSlice>(value)) {
                            auto bytes=state_.native_value(*old->values[a]);if(!bytes)return std::unexpected(bytes.error());
                            auto offset=candidate.native_.append(*bytes);if(!offset)return std::unexpected(offset.error());
                            value=NativeSlice{*offset,static_cast<std::uint32_t>(bytes->size())};
                        } else {auto valid=validate_attribute_value(entry.id,static_cast<Attribute>(a),value);if(!valid)return valid;}
                    }
                    entry.values[a]=value;
                }
            }
        }
        candidate.generation_=state_.generation_+1;
        candidate.layout_.cif.fill(0);
        for(const auto& entry:candidate.fields()){candidate.layout_.cif[entry.id.cif]|=std::uint32_t{1}<<entry.id.bit;if(entry.id.cif)candidate.layout_.cif[0]|=1u<<entry.id.cif;}
        if(mask){candidate.layout_.cif[0]|=1u<<7;candidate.layout_.cif[7]=mask;}
        state_=candidate;return {};
    }
    template<class T> Result<void> set_native(FieldId id,const T& value,bool replace) noexcept {
        if(state_.layout_.attributes&&state_.layout_.attributes!=attribute_bit(Attribute::current))return std::unexpected(Error{ErrorCode::invalid_argument});
        bool found=false;for(const auto& entry:state_.fields())if(entry.id==id)found=true;
        if(replace&&!found)return std::unexpected(Error{ErrorCode::invalid_argument});
        const AttributeInput input{id,Attribute::current,InputValue{value}};EditWorkspace<Kind,N,NativeBytes> workspace;
        return edit(state_.layout_.attributes,std::span<const AttributeInput>{&input,1},workspace,id);
    }
    constexpr Result<void> bump() noexcept {
        if (state_.generation_ == std::numeric_limits<std::uint64_t>::max()) return std::unexpected(Error{ErrorCode::overflow});
        ++state_.generation_; return {};
    }
    constexpr void indicators() noexcept {
        state_.layout_.cif.fill(0);
        for (auto entry : state_.fields()) {
            state_.layout_.cif[entry.id.cif] |= std::uint32_t{1} << entry.id.bit;
            if (entry.id.cif != 0) state_.layout_.cif[0] |= std::uint32_t{1} << entry.id.cif;
        }
        if (state_.layout_.attributes) { state_.layout_.cif[0] |= 1u << 7; state_.layout_.cif[7] = state_.layout_.attributes; }
    }
    constexpr Result<void> insert(FieldEntry entry, bool replace) noexcept {
        if (!valid_field_selector(entry.id)) return std::unexpected(Error{ErrorCode::invalid_argument});
        auto candidate = *this;
        std::size_t index = 0;
        while (index < candidate.state_.size_ && field_before(candidate.state_.entries_[index].id,entry.id)) ++index;
        const bool found = index < candidate.state_.size_ && candidate.state_.entries_[index].id == entry.id;
        if (replace && !found) return std::unexpected(Error{ErrorCode::invalid_argument});
        if (!found) {
            if (candidate.state_.size_ == N) return std::unexpected(Error{ErrorCode::capacity_exhausted});
            for (auto j=candidate.state_.size_; j>index; --j) candidate.state_.entries_[j]=candidate.state_.entries_[j-1];
            ++candidate.state_.size_;
        }
        candidate.state_.entries_[index]=entry;
        auto result=candidate.bump(); if (!result) return result;
        candidate.indicators();
        if constexpr(NativeBytes!=0){auto compact=candidate.compact_native();if(!compact)return compact;}
        *this=candidate; return {};
    }
protected:
    explicit constexpr PacketBuilder(PacketSubtype subtype) noexcept {
        state_.layout_.subtype=subtype;
        state_.layout_.action=(subtype==PacketSubtype::control || subtype==PacketSubtype::cancel) ? 2 : 0;
        state_.layout_.family=(subtype==PacketSubtype::context ? PacketFamily::context : subtype==PacketSubtype::signal ? PacketFamily::signal_data : PacketFamily::command);
    }
public:
    constexpr PacketBuilder() noexcept : PacketBuilder(Kind==BodyKind::values ? PacketSubtype::context : Kind==BodyKind::selectors ? PacketSubtype::query : PacketSubtype::diagnostic_ack) {}
    constexpr Result<void> configure(std::uint16_t packet_class,std::uint8_t action) noexcept {
        const auto subtype=state_.layout_.subtype;
        if(action>2 || (subtype==PacketSubtype::query && action!=0) ||
            (subtype==PacketSubtype::cancel && action!=2) ||
            (subtype==PacketSubtype::control && action==0) ||
            ((subtype==PacketSubtype::context || subtype==PacketSubtype::signal) && action!=0))
            return std::unexpected(Error{ErrorCode::invalid_argument});
        auto candidate=*this;auto result=candidate.bump();if(!result)return result;
        candidate.state_.layout_.packet_class=packet_class;candidate.state_.layout_.action=action;
        *this=candidate;return {};
    }
    Result<void> bind_timestamp_format(std::uint8_t tsi,std::uint8_t tsf) noexcept {
        if(tsi>3||tsf>3)return std::unexpected(Error{ErrorCode::invalid_argument});
        if constexpr(Kind==BodyKind::values)for(const auto& entry:state_.fields())if(temporal_duration_field(entry.id)||entry.id==SectorStepScan::id) {
            for(unsigned a=0;a<11;++a)if(entry.values[a]) {
                auto bytes=state_.native_value(*entry.values[a]);if(!bytes)return std::unexpected(bytes.error());
                auto binding=native_timestamp_binding(entry.id,*bytes);if(!binding)return std::unexpected(binding.error());
                if(*binding && ((*binding)->tsi!=tsi||(*binding)->tsf!=tsf))return std::unexpected(Error{ErrorCode::invalid_argument});
            }
        }
        auto candidate=*this;auto result=candidate.bump();if(!result)return result;
        candidate.state_.layout_.timestamp_format={tsi,tsf,true};*this=candidate;return {};
    }
    constexpr PacketSnapshot<Kind,N,NativeBytes> freeze() const noexcept { return state_; }
    constexpr std::uint64_t generation() const noexcept { return state_.generation_; }
    constexpr bool has_attribute(FieldId id,Attribute attribute) const noexcept {
        const auto a=static_cast<unsigned>(attribute);if(a>=13)return false;
        for(const auto& entry:state_.fields())if(entry.id==id&&entry.values[a])return true;
        return false;
    }
    template<class Field> constexpr Result<void> set(typename Field::value_type value) noexcept requires (Kind==BodyKind::values) {
        if constexpr(structured_field(Field::id))return set_native(Field::id,value,false);
        else return set_value(Field::id, SemanticValue{value});
    }
    constexpr Result<void> set_value(FieldId id, SemanticValue value, bool replace=false) noexcept requires (Kind==BodyKind::values) {
        auto valid=validate_value(id,value); if (!valid) return valid;
        // Attribute-bearing edits require the complete transactional API below.
        if (state_.layout_.attributes && state_.layout_.attributes != attribute_bit(Attribute::current)) return std::unexpected(Error{ErrorCode::invalid_argument});
        FieldEntry entry{};entry.id=id;entry.values[0]=value;return insert(entry,replace);
    }
    template<class Field> constexpr Result<void> replace(typename Field::value_type value) noexcept requires (Kind==BodyKind::values) {
        if constexpr(structured_field(Field::id))return set_native(Field::id,value,true);
        else return set_value(Field::id,SemanticValue{value},true);
    }
    template<class Field> constexpr Result<void> select() noexcept requires (Kind==BodyKind::selectors) { return select(Field::id); }
    constexpr Result<void> select(FieldId id) noexcept requires (Kind==BodyKind::selectors) { FieldEntry entry{};entry.id=id;return insert(entry,false); }
    constexpr Result<void> diagnostic(FieldId id,std::uint32_t bits) noexcept requires (Kind==BodyKind::diagnostics) { FieldEntry entry{};entry.id=id;entry.diagnostic=bits;return insert(entry,false); }
    template<class Field> constexpr Result<void> remove() noexcept { return remove(Field::id); }
    constexpr Result<void> remove(FieldId id) noexcept {
        auto candidate=*this;
        std::size_t i=0;while(i<candidate.state_.size_ && candidate.state_.entries_[i].id!=id) ++i;
        if(i==candidate.state_.size_) return std::unexpected(Error{ErrorCode::invalid_argument});
        for(auto j=i+1;j<candidate.state_.size_;++j) candidate.state_.entries_[j-1]=candidate.state_.entries_[j];
        candidate.state_.entries_[--candidate.state_.size_]={};
        auto result=candidate.bump();if(!result)return result;candidate.indicators();
        if constexpr(NativeBytes!=0){auto compact=candidate.compact_native();if(!compact)return compact;}
        *this=candidate;return {};
    }
    constexpr Result<void> with_attributes(std::uint32_t mask,std::span<const AttributeValue> supplied={}) noexcept {
        EditWorkspace<Kind,N,NativeBytes> workspace;return edit(mask,supplied,workspace);
    }
    constexpr Result<void> with_attributes(std::uint32_t mask,std::span<const AttributeValue> supplied,EditWorkspace<Kind,N,NativeBytes>& workspace) noexcept {return edit(mask,supplied,workspace);}
    Result<void> with_attribute_inputs(std::uint32_t mask,std::span<const AttributeInput> supplied,EditWorkspace<Kind,N,NativeBytes>& workspace) noexcept {return edit(mask,supplied,workspace);}
    Result<void> with_attribute_inputs(std::uint32_t mask,std::span<const AttributeInput> supplied) noexcept {EditWorkspace<Kind,N,NativeBytes> workspace;return edit(mask,supplied,workspace);}
    template<class Field> Result<void> set_field_attributes(std::span<const AttributeInput> supplied,EditWorkspace<Kind,N,NativeBytes>& workspace) noexcept requires(Kind==BodyKind::values) {
        const auto mask=state_.layout_.attributes?state_.layout_.attributes:attribute_bit(Attribute::current);
        if(supplied.size()!=static_cast<std::size_t>(std::popcount(mask)))return std::unexpected(Error{ErrorCode::invalid_argument});
        for(const auto& input:supplied)if(input.field!=Field::id)return std::unexpected(Error{ErrorCode::invalid_argument});
        return edit(state_.layout_.attributes,supplied,workspace,Field::id);
    }
    template<class Field> Result<void> set_field_attributes(std::span<const AttributeInput> supplied) noexcept requires(Kind==BodyKind::values) {EditWorkspace<Kind,N,NativeBytes> workspace;return set_field_attributes<Field>(supplied,workspace);}
    Result<void> set_attribute_input(const AttributeInput& input) noexcept requires(Kind==BodyKind::values) {
        const auto a=static_cast<unsigned>(input.attribute);bool found=false;
        if(a>=13)return std::unexpected(Error{ErrorCode::invalid_argument});
        for(const auto& entry:state_.fields())if(entry.id==input.field&&entry.values[a])found=true;
        if(!found)return std::unexpected(Error{ErrorCode::invalid_argument});
        return with_attribute_inputs(state_.layout_.attributes,{&input,1});
    }
    template<class Field> Result<void> set_attribute(Attribute attribute,typename Field::value_type value) noexcept requires(Kind==BodyKind::values) {
        if constexpr(structured_field(Field::id))return set_attribute_input({Field::id,attribute,InputValue{value}});
        else return set_attribute_input({Field::id,attribute,InputValue{SemanticValue{value}}});
    }
    template<class Field> Result<void> replace_attribute(Attribute attribute,typename Field::value_type value) noexcept requires(Kind==BodyKind::values) {return set_attribute<Field>(attribute,value);}
    Result<void> set_probability(FieldId id,ProbabilityCode value) noexcept requires(Kind==BodyKind::values) {return set_attribute_input({id,Attribute::probability,InputValue{SemanticValue{value}}});}
    Result<void> set_belief(FieldId id,BeliefCode value) noexcept requires(Kind==BodyKind::values) {return set_attribute_input({id,Attribute::belief,InputValue{SemanticValue{value}}});}

};
constexpr bool compatible_body(BodyKind kind,PacketSubtype subtype) noexcept {
    switch(subtype) {
    case PacketSubtype::query: case PacketSubtype::cancel: return kind==BodyKind::selectors;
    case PacketSubtype::diagnostic_ack: return kind==BodyKind::diagnostics;
    default: return kind==BodyKind::values;
    }
}
template<BodyKind Kind, PacketSubtype Subtype, std::size_t N=16, std::size_t NativeBytes=0>
    requires (compatible_body(Kind,Subtype))
class TypedPacket : public PacketBuilder<Kind,N,NativeBytes> {
public:
    constexpr TypedPacket() noexcept : PacketBuilder<Kind,N,NativeBytes>(Subtype) {}
};
using ContextPacket = TypedPacket<BodyKind::values,PacketSubtype::context>;
using ControlPacket = TypedPacket<BodyKind::values,PacketSubtype::control>;
using QueryPacket = TypedPacket<BodyKind::selectors,PacketSubtype::query>;
using CancelPacket = TypedPacket<BodyKind::selectors,PacketSubtype::cancel>;
using DiagnosticAck = TypedPacket<BodyKind::diagnostics,PacketSubtype::diagnostic_ack>;
using StateAck = TypedPacket<BodyKind::values,PacketSubtype::state_ack>;
using SignalMetadata = TypedPacket<BodyKind::values,PacketSubtype::signal>;
template<std::size_t NativeBytes=8192,std::size_t N=16>
using NativeContextPacket = TypedPacket<BodyKind::values,PacketSubtype::context,N,NativeBytes>;
template<std::size_t NativeBytes=8192,std::size_t N=16>
using NativeControlPacket = TypedPacket<BodyKind::values,PacketSubtype::control,N,NativeBytes>;
template<std::size_t NativeBytes=8192,std::size_t N=16>
using NativeStateAck = TypedPacket<BodyKind::values,PacketSubtype::state_ack,N,NativeBytes>;
} // namespace vita
