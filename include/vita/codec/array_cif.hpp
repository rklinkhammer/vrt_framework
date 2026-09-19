#pragma once
#include <vita/codec/packet.hpp>
#include <array>

namespace vita::codec::array_cif {
inline constexpr FieldId array_id{1,11};
enum class Dialect { i9_five_cifs_header7 };
struct Options {
    Dialect dialect;
    TimestampFormatBinding timestamps;
    Options() = delete;
    constexpr Options(Dialect d,TimestampFormatBinding t) noexcept: dialect(d),timestamps(t) {}
};
// Mutable packet-wide cursor. Callers can share it across sibling/diagnostic groups.
// Public validation commits consumed counts only on success.
struct Budget {
    std::size_t fields=0,views=0,work=0;
    std::size_t max_fields=128,max_views=1664,max_work=4096;
    unsigned max_depth=4;
    std::size_t max_records=256;
    TraversalLimits field_limits{};
};
struct PathStep { std::uint32_t ordinal,index; };
struct Path { std::array<PathStep,4> steps{}; unsigned depth=0; };
struct Element { FieldView field; Path path; };
namespace detail {
inline constexpr FieldDescriptor array_descriptor{array_id,0,0,all_attributes};
struct Resolver {
    constexpr const FieldDescriptor* operator()(FieldId id) const noexcept {
        return id==array_id?&array_descriptor:vita::descriptor(id);
    }
};
inline Result<void> charge(std::size_t& used,std::size_t limit,std::size_t amount) noexcept {
    if(used>limit || amount>limit-used)return std::unexpected(Error{ErrorCode::resource_limit});
    used+=amount;return {};
}
struct Header { std::size_t bytes; unsigned words,records; IndicatorPlan<128> plan; };
inline Result<Header> header(Bytes bytes,Options options) noexcept {
    if(options.dialect!=Dialect::i9_five_cifs_header7)return std::unexpected(Error{ErrorCode::unsupported_capability});
    if(options.timestamps.tsi>3 || options.timestamps.tsf>3)return std::unexpected(Error{ErrorCode::invalid_argument});
    if(bytes.size()<12)return std::unexpected(Error{ErrorCode::short_input,0,12});
    const auto total=vita::codec::detail::load32(bytes,0),header1=vita::codec::detail::load32(bytes,4);
    const unsigned width=(header1>>12)&4095u,records=header1&4095u;
    if(header1>>24!=7 || vita::codec::detail::load32(bytes,8)!=0)return std::unexpected(Error{ErrorCode::unsupported_layout});
    auto product=checked_multiply(width,records);if(!product)return std::unexpected(product.error());
    auto words=checked_add(8,*product);if(!words)return std::unexpected(words.error());
    if(total!=*words || (records && width<1))return std::unexpected(Error{ErrorCode::invalid_argument});
    auto extent=checked_multiply(total,4);if(!extent)return std::unexpected(extent.error());
    if(bytes.size()<*extent)return std::unexpected(Error{ErrorCode::short_input,0,*extent});
    Header out{*extent,width,records,{}};
    auto& cif=out.plan.context.cif;
    for(unsigned i=0;i<4;++i)cif[i]=vita::codec::detail::load32(bytes,12+i*4);
    cif[7]=vita::codec::detail::load32(bytes,28);
    if(cif[0]&0x71u || cif[7]&~all_attributes)return std::unexpected(Error{ErrorCode::invalid_argument});
    const bool enabled=(cif[0]&0x80u)!=0;
    // Registered capability restriction; mandatory zero CIF7 with enable clear is Current.
    if(enabled!=(cif[7]!=0))return std::unexpected(Error{ErrorCode::unsupported_capability});
    out.plan.context.attributes=cif[7];out.plan.context.timestamp_format=options.timestamps;
    out.plan.change=(cif[0]&0x80000000u)!=0;
    out.plan.bytes=4; // overwritten below: walk's synthetic prefix is mapped away, not consumed on wire.
    for(unsigned i=1;i<8;++i)if(cif[0]&(1u<<i))out.plan.bytes+=4;
    for(unsigned c=0;c<4;++c) {
        const auto selected=c==0?cif[c]&0x7fffff00u:cif[c];
        for(int bit=31;bit>=0;--bit)if(selected&(std::uint32_t{1}<<bit)) {
            const FieldId id{static_cast<std::uint8_t>(c),static_cast<std::uint8_t>(bit)};
            if(!Resolver{}(id))return std::unexpected(Error{ErrorCode::unsupported_layout});
            if(out.plan.count==out.plan.selected.size())return std::unexpected(Error{ErrorCode::resource_limit});
            out.plan.selected[out.plan.count++].id=id;
        }
    }
    return out;
}
template<class Visitor> Result<std::size_t> walk(Bytes bytes,Options options,Budget& budget,Path path,Visitor& visitor,const Header* checked_header=nullptr) noexcept {
    auto parsed=checked_header?Result<Header>{*checked_header}:header(bytes,options);if(!parsed)return std::unexpected(parsed.error());
    auto& h=*parsed;
    // Immediate size is known before classifying depth/count resource limits.
    if(path.depth>=4 || path.depth>=budget.max_depth || h.records>budget.max_records)
        return std::unexpected(Error{ErrorCode::resource_limit});
    auto cost=charge(budget.work,budget.max_work,1);if(!cost)return std::unexpected(cost.error());
    for(unsigned record=0;record<h.records;++record) {
        auto body=bytes.subspan(32+std::size_t(record)*h.words*4,std::size_t(h.words)*4);
        auto record_path=path;
        record_path.steps[record_path.depth++]={record,vita::codec::detail::load32(body,0)};
        cost=charge(budget.work,budget.max_work,1);if(!cost)return std::unexpected(cost.error());
        cost=charge(budget.fields,budget.max_fields,h.plan.count);if(!cost)return std::unexpected(cost.error());
        auto provider=[&](const auto& field,Attribute attribute,std::size_t offset) noexcept -> Result<FieldExtent> {
            const auto at=offset-h.plan.bytes+4;
            if(at>body.size())return std::unexpected(Error{ErrorCode::short_input,at});
            FieldExtent extent{};
            if(field.id==array_id && base_attribute(attribute)) {
                auto nested=walk(body.subspan(at),options,budget,record_path,visitor);
                if(!nested)return std::unexpected(nested.error());
                extent={*nested,0}; // Nested array's own traversal already charged all units.
            } else if(base_attribute(attribute) && structured_field(field.id)) {
                auto wire=vita::codec::detail::wire_structure_extent(field.id,body.subspan(at),options.timestamps);
                if(!wire)return std::unexpected(wire.error());extent=*wire;
            } else extent={base_attribute(attribute)?Resolver{}(field.id)->words*4:4,1};
            if(extent.bytes>body.size()-at)return std::unexpected(Error{ErrorCode::short_input,at,at+extent.bytes});
            if(field.id!=array_id || !base_attribute(attribute)) {
                if(field.id==array_id) {
                    auto value=vita::codec::detail::read_attribute_scalar(array_descriptor,attribute,body.subspan(at,extent.bytes));
                    if(!value)return std::unexpected(value.error());
                } else {
                    auto valid=validate_wire_attribute(field.id,attribute,body.subspan(at,extent.bytes),options.timestamps);
                    if(!valid)return std::unexpected(valid.error());
                }
            }
            cost=charge(budget.work,budget.max_work,extent.units);if(!cost)return std::unexpected(cost.error());
            return extent;
        };
        auto limits=budget.field_limits;
        // The packet-wide cursor enforces aggregate work. The shared walker also
        // enforces all existing per-field limits and performs its overflow checks.
        limits.work_units=budget.max_work;
        auto measured=walk_field_layout<BodyKind::values>(h.plan,provider,[&](LayoutElement e) noexcept -> Result<void> {
            const auto at=e.offset-h.plan.bytes+4;
            auto charged=charge(budget.views,budget.max_views,1);if(!charged)return charged;
            return visitor(Element{FieldView{e.field,e.attribute,BodyKind::values,DiagnosticGroup::none,body.subspan(at,e.bytes),options.timestamps},record_path});
        },0,limits,nullptr,Resolver{});
        if(!measured)return std::unexpected(measured.error());
        if(measured->bytes-h.plan.bytes+4!=body.size())return std::unexpected(Error{ErrorCode::invalid_argument});
    }
    return h.bytes;
}
} // namespace detail
class View {
    Bytes bytes_;
    Options options_;
    Budget initial_;
    View(Bytes bytes,Options options,Budget initial) noexcept:bytes_(bytes),options_(options),initial_(initial){}
    friend Result<View> validate(Bytes,Options,Budget&) noexcept;
public:
    // Immutable borrowed bytes must outlive this view and any returned field spans.
    unsigned record_count() const noexcept{return vita::codec::detail::load32(bytes_,4)&4095u;}
    Result<Bytes> record(unsigned index) const noexcept {
        if(index>=record_count())return std::unexpected(Error{ErrorCode::invalid_argument});
        const auto width=(vita::codec::detail::load32(bytes_,4)>>12)&4095u;
        return bytes_.subspan(32+std::size_t(index)*width*4,std::size_t(width)*4);
    }
    Bytes wire() const noexcept{return bytes_;}
    // Structural observation only. No execution, peer agreement or emission API.
    template<class Visitor> Result<void> visit(Visitor&& visitor) const noexcept {
        auto budget=initial_;
        auto walked=detail::walk(bytes_,options_,budget,{},visitor);
        if(!walked)return std::unexpected(walked.error());return {};
    }
};
inline Result<View> validate(Bytes exact_field,Options options,Budget& shared) noexcept {
    auto parsed=detail::header(exact_field,options);if(!parsed)return std::unexpected(parsed.error());
    if(parsed->bytes!=exact_field.size())return std::unexpected(Error{ErrorCode::invalid_argument});
    auto candidate=shared;
    auto charged=detail::charge(candidate.fields,candidate.max_fields,1);if(!charged)return std::unexpected(charged.error());
    auto noop=[](const Element&) noexcept -> Result<void>{return {};};
    const auto initial=candidate;
    auto walked=detail::walk(exact_field,options,candidate,{},noop,&*parsed);if(!walked)return std::unexpected(walked.error());
    if(*walked!=exact_field.size())return std::unexpected(Error{ErrorCode::invalid_argument});
    shared=candidate;return View{exact_field,options,initial};
}
} // namespace vita::codec::array_cif
