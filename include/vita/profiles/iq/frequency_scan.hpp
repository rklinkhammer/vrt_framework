#pragma once
#include <vita/profiles/iq/source.hpp>
#include <numbers>
#include <optional>

namespace vita::profiles::iq {
struct SceneConfig {
    std::uint64_t sample_rate=100'000,tone_hz=100'050'000;
    double amplitude=0.5;
};
class VirtualRfScene {
    SceneConfig config_;
    std::uint64_t center_=0,next_=0,tone_phase_=0,lo_phase_=0;
    bool initialized_=false,faulted_=false,recovery_origin_=false;
    explicit VirtualRfScene(SceneConfig c) noexcept:config_(c){}
    static Result<std::uint64_t> hertz(const runtime::StateSnapshot& state,FieldId id) noexcept {
        for(const auto& f:state.fields)if(f.id==id) {
            const auto* value=std::get_if<Hertz>(&f.value);
            if(f.validity!=runtime::Validity::known || !value || value->q20<0 || value->q20%(1LL<<20))
                return std::unexpected(Error{ErrorCode::invalid_state});
            return static_cast<std::uint64_t>(value->q20>>20);
        }
        return std::unexpected(Error{ErrorCode::invalid_state});
    }
    Result<void> fail(ErrorCode code=ErrorCode::invalid_state) noexcept {
        faulted_=true;return std::unexpected(Error{code});
    }
    void advance(std::uint64_t count) noexcept {
        // Every factor is below100M; multiplication fits uint64 even for arbitrary elapsed ordinals.
        const auto delta=count%config_.sample_rate;
        tone_phase_=(tone_phase_+delta*(config_.tone_hz%config_.sample_rate))%config_.sample_rate;
        lo_phase_=(lo_phase_+delta*(center_%config_.sample_rate))%config_.sample_rate;
        next_+=count;
    }
public:
    static Result<VirtualRfScene> create(SceneConfig config={}) noexcept {
        if(!config.sample_rate || config.sample_rate>100'000'000 || config.tone_hz>6'000'000'000ULL
           || !std::isfinite(config.amplitude) || config.amplitude<0 || config.amplitude>1)
            return std::unexpected(Error{ErrorCode::invalid_argument});
        return VirtualRfScene{config};
    }
    // Explicitly starts a NEW scene phase epoch at the first confirmed recovery
    // ordinal. It does not rearm an existing faulted scene or alter ordinary tunes.
    static Result<VirtualRfScene> create_for_recovery(SceneConfig config={}) noexcept {
        auto scene=create(config);
        if(!scene)return std::unexpected(scene.error());
        scene->recovery_origin_=true;
        return scene;
    }
    // The provider is caller-owned and all calls occur on the serialized runtime owner.
    Result<void> effective(const runtime::EffectiveEvent& event) noexcept {
        if(faulted_)return std::unexpected(Error{ErrorCode::invalid_state});
        auto center=hertz(event.state,RFReferenceFrequency::id),rate=hertz(event.state,SampleRate::id);
        if(!center || !rate || *rate!=config_.sample_rate || *center<1'000'000 || *center>6'000'000'000ULL
           || event.outcome.simulated || !event.ordinal_known)return fail();
        if(!initialized_) {
            if(!recovery_origin_ && event.sample_ordinal!=0)return fail();
            next_=event.sample_ordinal;
            center_=*center;initialized_=true;return {};
        }
        if(event.sample_ordinal<next_)return fail();
        advance(event.sample_ordinal-next_); // Integrate the old center through every unproduced gap.
        center_=*center;return {};
    }
    Result<void> produce(SampleWriteWindow& window) noexcept {
        if(!initialized_ || faulted_)return std::unexpected(Error{ErrorCode::invalid_state});
        const auto rate=hertz(window.config(),SampleRate::id),center=hertz(window.config(),RFReferenceFrequency::id);
        if(!rate || !center || *rate!=config_.sample_rate || *center!=center_ || window.first_ordinal()<next_)
            return fail();
        if(window.count()>UINT64_MAX-window.first_ordinal())return fail(ErrorCode::overflow);
        advance(window.first_ordinal()-next_);
        const auto offset=static_cast<std::int64_t>(config_.tone_hz)-static_cast<std::int64_t>(center_);
        const auto twice=offset*2;
        const bool audible=twice>=-static_cast<std::int64_t>(config_.sample_rate) && twice<static_cast<std::int64_t>(config_.sample_rate);
        for(std::size_t i=0;i<window.count();++i) {
            const auto phase=phase_numerator();
            const double angle=2*std::numbers::pi_v<double>*static_cast<double>(phase)/static_cast<double>(config_.sample_rate);
            auto wrote=window.write(i,audible?config_.amplitude*std::cos(angle):0,
                                     audible?config_.amplitude*std::sin(angle):0);
            if(!wrote)return fail(wrote.error().code);
            advance(1);
        }
        return {};
    }
    static Result<void> produce_callback(void* context,SampleWriteWindow& window) noexcept {
        if(!context)return std::unexpected(Error{ErrorCode::invalid_argument});
        return static_cast<VirtualRfScene*>(context)->produce(window);
    }
    static Result<void> effective_callback(void* context,const runtime::EffectiveEvent& event) noexcept {
        if(!context)return std::unexpected(Error{ErrorCode::invalid_argument});
        return static_cast<VirtualRfScene*>(context)->effective(event);
    }
    std::uint64_t phase_numerator() const noexcept{return (tone_phase_+config_.sample_rate-lo_phase_)%config_.sample_rate;}
    std::uint64_t next_ordinal() const noexcept{return next_;}
    std::uint64_t center_hz() const noexcept{return center_;}
    bool faulted() const noexcept{return faulted_;}
};
struct SweepConfig {
    std::uint64_t start_hz=100'000'000,stop_hz=100'200'000,step_hz=25'000;
    std::uint64_t dwell_ns=100'000'000,timeout_ns=1'000'000'000;
    std::uint64_t sweeps=1;
    bool continuous=false;
};
enum class SweepPhase { ready,awaiting,dwelling,complete,failed };
struct TuneExecution { bool executed=false,simulated=false,partial=false,unknown=false; };
class SweepPolicy {
    SweepConfig config_;
    SweepPhase phase_=SweepPhase::ready;
    std::uint64_t current_,deadline_=0,last_now_=0,completed_=0,completed_sweeps_=0;
    bool execution_=false,readback_=false;
    explicit SweepPolicy(SweepConfig c) noexcept:config_(c),current_(c.start_hz){}
    Result<void> check_time(runtime::timing::MonoTime now) noexcept {
        if(now.ns<last_now_)return std::unexpected(Error{ErrorCode::invalid_argument});
        last_now_=now.ns;
        if(phase_==SweepPhase::awaiting && now.ns>=deadline_) {
            phase_=SweepPhase::failed;return std::unexpected(Error{ErrorCode::invalid_state});
        }
        return {};
    }
    Result<void> finish_evidence(runtime::timing::MonoTime now) noexcept {
        if(execution_&&readback_) {
            if(config_.dwell_ns>UINT64_MAX-now.ns){phase_=SweepPhase::failed;return std::unexpected(Error{ErrorCode::overflow});}
            phase_=SweepPhase::dwelling;deadline_=now.ns+config_.dwell_ns;
        }
        return {};
    }
public:
    static Result<SweepPolicy> create(SweepConfig config={}) noexcept {
        if(config.start_hz<1'000'000 || config.stop_hz>6'000'000'000ULL || config.start_hz>config.stop_hz
           || !config.step_hz || !config.timeout_ns || !config.sweeps)return std::unexpected(Error{ErrorCode::invalid_argument});
        return SweepPolicy{config};
    }
    // A returned frequency means one submission attempt; no implicit retry/cancel.
    // Zero dwell is allowed. The caller must filter evidence by the current transaction identity.
    Result<std::optional<std::uint64_t>> next(runtime::timing::MonoTime now) noexcept {
        auto clock=check_time(now);if(!clock)return std::unexpected(clock.error());
        if(phase_==SweepPhase::failed)return std::unexpected(Error{ErrorCode::invalid_state});
        if(phase_==SweepPhase::dwelling && now.ns>=deadline_) {
            if(completed_==UINT64_MAX){phase_=SweepPhase::failed;return std::unexpected(Error{ErrorCode::overflow});}
            ++completed_;
            if(config_.step_hz>config_.stop_hz-current_) {
                if(completed_sweeps_==UINT64_MAX){phase_=SweepPhase::failed;return std::unexpected(Error{ErrorCode::overflow});}
                ++completed_sweeps_;
                if(!config_.continuous && completed_sweeps_==config_.sweeps){phase_=SweepPhase::complete;return std::optional<std::uint64_t>{};}
                current_=config_.start_hz;
            } else current_+=config_.step_hz;
            phase_=SweepPhase::ready;
        }
        if(phase_!=SweepPhase::ready)return std::optional<std::uint64_t>{};
        if(config_.timeout_ns>UINT64_MAX-now.ns){phase_=SweepPhase::failed;return std::unexpected(Error{ErrorCode::overflow});}
        deadline_=now.ns+config_.timeout_ns;execution_=readback_=false;phase_=SweepPhase::awaiting;
        return std::optional<std::uint64_t>{current_};
    }
    Result<void> execution(TuneExecution evidence,runtime::timing::MonoTime now) noexcept {
        auto clock=check_time(now);if(!clock)return clock;
        if(phase_==SweepPhase::dwelling && execution_ && evidence.executed && !evidence.simulated && !evidence.partial && !evidence.unknown)return {};
        if(phase_!=SweepPhase::awaiting)return std::unexpected(Error{ErrorCode::invalid_state});
        if(!evidence.executed || evidence.simulated || evidence.partial || evidence.unknown){phase_=SweepPhase::failed;return std::unexpected(Error{ErrorCode::invalid_state});}
        execution_=true;return finish_evidence(now);
    }
    Result<void> readback(std::uint64_t hz,bool known,runtime::timing::MonoTime now) noexcept {
        auto clock=check_time(now);if(!clock)return clock;
        if(phase_==SweepPhase::dwelling && readback_ && known && hz==current_)return {};
        if(phase_!=SweepPhase::awaiting)return std::unexpected(Error{ErrorCode::invalid_state});
        if(!known || hz!=current_){phase_=SweepPhase::failed;return std::unexpected(Error{ErrorCode::invalid_state});}
        readback_=true;return finish_evidence(now);
    }
    void submission_failed() noexcept{phase_=SweepPhase::failed;}
    SweepPhase phase()const noexcept{return phase_;}
    std::uint64_t deadline_ns()const noexcept{return deadline_;}
    std::uint64_t completed_points()const noexcept{return completed_;}
};
} // namespace vita::profiles::iq
