#include <vita/codec/packet.hpp>
#include <cassert>
#include <type_traits>
using namespace vita;using namespace vita::codec;
static Result<void> sentence(void*,std::span<const char> s)noexcept {return s.size()==6&&s[0]=='$'&&s[4]=='\r'&&s[5]=='\n'?Result<void>{}:Result<void>{std::unexpected(Error{ErrorCode::invalid_argument})};}
template<class T> concept TemporaryView = requires(T&& t){std::move(t).template get<GPSASCII>();};
static_assert(!TemporaryView<PacketSnapshot<BodyKind::values,16,8192>>);
static_assert(!std::is_constructible_v<NativeSlice,std::uint32_t,std::uint32_t>);
static Result<void> counted(void* p,std::span<const char>)noexcept{++*static_cast<unsigned*>(p);return {};}
static Result<void> one_character(void*,std::span<const char> text)noexcept{return text.size()==1&&text[0]=='X'?Result<void>{}:Result<void>{std::unexpected(Error{ErrorCode::invalid_argument})};}
static auto frozen(){NativeContextPacket<> p;char text[]={'$','A',',','1','\r','\n'};std::uint32_t source[]{7,7},sys[]{8},vec[]{9},async[]{10},tags[]{99};
 assert(p.set<GPSASCII>(GpsAsciiInput{0x123456,text,{nullptr,sentence}}));assert(p.set<ContextAssociationLists>(AssociationListsInput{source,sys,vec,async,tags,true}));GeolocationValue g;g.latitude_q22=1;assert(p.set<FormattedGPS>(g));auto snap=p.freeze();text[1]='Z';source[0]=100;assert(p.remove<GPSASCII>());g.latitude_q22=2;assert(p.replace<FormattedGPS>(g));return snap;}
int main(){auto old=frozen();auto ascii=old.get<GPSASCII>();assert(ascii&&ascii->text.size()==6&&ascii->text[1]=='A');auto lists=old.get<ContextAssociationLists>();assert(lists&&lists->source.size()==2&&*lists->source.at(0)==7&&*lists->source.at(1)==7&&*lists->tags.at(0)==99&&!lists->source.at(2));assert(old.get<FormattedGPS>()->latitude_q22==1);
 auto copy=old;const auto& raw=*old.fields()[0].values[0];assert(!copy.native_value(raw));NativeContextPacket<> other;assert(!other.set_value(old.fields()[0].id,raw));std::array<AttributeValue,1> attrs{{{old.fields()[0].id,Attribute::current,raw}}};assert(!other.with_attributes(attribute_bit(Attribute::current),attrs));
 std::array<std::byte,512> before{},after{};Envelope env;env.type=PacketType::context;env.stream_id=1;auto encoded=encode_packet(env,old,before);assert(encoded);auto encoded_copy=encode_packet(env,copy,after);assert(encoded_copy&&*encoded_copy==*encoded&&before==after);
 NativeContextPacket<128> small;char good[]{'$','A',',','1','\r','\n'};assert(small.set<GPSASCII>(GpsAsciiInput{1,good,{nullptr,sentence}}));auto first=small.freeze();auto first_size=first.native_size();for(unsigned i=0;i<100;++i){assert(small.replace<GPSASCII>(GpsAsciiInput{i,good,{nullptr,sentence}}));assert(small.freeze().native_size()==first_size);}assert(first.get<GPSASCII>()->oui==1);
 auto pre=small.freeze();std::array<std::uint32_t,64> large{};assert(!small.set<ContextAssociationLists>(AssociationListsInput{{},{},large,{},{},false}));assert(small.freeze().generation()==pre.generation()&&small.freeze().native_size()==pre.native_size());unsigned validator_calls=0;std::array<char,512> overlarge_text{};overlarge_text.fill('X');assert(!small.replace<GPSASCII>(GpsAsciiInput{1,overlarge_text,{&validator_calls,counted}}));assert(validator_calls==0&&small.freeze().generation()==pre.generation()&&small.freeze().native_size()==pre.native_size());
 assert(!small.replace<GPSASCII>(GpsAsciiInput{1,good,{}}));assert(small.freeze().generation()==pre.generation());GeolocationValue bad;bad.latitude_q22=91*(1<<22);assert(!small.set<FormattedGPS>(bad));assert(small.freeze().generation()==pre.generation());assert(small.remove<GPSASCII>());assert(small.freeze().native_size()==0);
 // Equal total bytes/generation with different field partitions cannot reuse the layout signature.
 NativeContextPacket<> shape_a,shape_b;char x[]{'X'};std::uint32_t one[]{1},two[]{1,2};
 assert(shape_a.set<GPSASCII>(GpsAsciiInput{1,good,{nullptr,sentence}}));assert(shape_a.set<ContextAssociationLists>(AssociationListsInput{one,{},{},{},{},false}));
 assert(shape_b.set<GPSASCII>(GpsAsciiInput{1,x,{nullptr,one_character}}));assert(shape_b.set<ContextAssociationLists>(AssociationListsInput{two,{},{},{},{},false}));
 auto sa=shape_a.freeze(),sb=shape_b.freeze();auto ma=measure(sa),mb=measure(sb);assert(ma&&mb&&ma->bytes==mb->bytes&&sa.generation()==sb.generation());assert(!validate_measure(sb,*ma));NativeContextPacket<> shape_c;std::uint32_t changed[]{999};assert(shape_c.set<GPSASCII>(GpsAsciiInput{99,good,{nullptr,sentence}}));assert(shape_c.set<ContextAssociationLists>(AssociationListsInput{changed,{},{},{},{},false}));auto sc=shape_c.freeze();assert(validate_measure(sc,*ma));
 // A second typed field after removal must fit, with no arena consumption leak.
 assert(small.set<FormattedGPS>(GeolocationValue{}));auto unknown=small.freeze();auto gv=unknown.get<FormattedGPS>();assert(gv&&!gv->latitude_q22&&!gv->track_q22);
}
