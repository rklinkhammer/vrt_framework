#pragma once
#include <vita/runtime/state/contracts.hpp>
#include <optional>
struct VerifySink {
    struct Saved { vita::runtime::EffectiveEvent event;vita::runtime::AdmissionBundle credits; };
    struct State:std::enable_shared_from_this<State>{
        std::array<std::optional<Saved>,16> records;
        std::array<std::optional<std::size_t>,16> reservations;
        std::size_t capacity=16,occupied=0,count=0;bool invalid=false;
        static void release(void* p,std::uint64_t token) noexcept{auto& s=*static_cast<State*>(p);if(token>=16||!s.reservations[token]){s.invalid=true;return;}s.occupied-=*s.reservations[token];s.reservations[token].reset();}
        static vita::Result<vita::runtime::RevisionReservation> reserve(void* p,std::size_t count) noexcept{
            auto& s=*static_cast<State*>(p);if(count>s.capacity-s.occupied)return std::unexpected(vita::Error{vita::ErrorCode::capacity_exhausted});
            for(unsigned i=0;i<16;++i)if(!s.reservations[i]){s.reservations[i]=count;s.occupied+=count;return vita::runtime::RevisionReservation{s.shared_from_this(),&s,i,release};}
            return std::unexpected(vita::Error{vita::ErrorCode::capacity_exhausted});
        }
        static void record(void* p,const vita::runtime::EffectiveEvent& e,vita::runtime::RevisionReservation& reservation,vita::runtime::AdmissionBundle credits) noexcept{
            auto& s=*static_cast<State*>(p);auto token=reservation.token();
            if(token>=16||!s.reservations[token]||!*s.reservations[token]){s.invalid=true;return;}
            for(auto& slot:s.records)if(!slot){slot.emplace(Saved{e,std::move(credits)});--*s.reservations[token];++s.count;return;}s.invalid=true;
        }
    };
    std::shared_ptr<State> state=std::make_shared<State>();
    vita::runtime::EffectSink binding(){return {state.get(),state,State::reserve,State::record};}
    void clear(){for(auto& r:state->records)if(r){r.reset();--state->occupied;}state->count=0;}
};
