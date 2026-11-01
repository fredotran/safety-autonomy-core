// @req SR-CONFIG-001
// Coverage-driven tests targeting branches that were previously uncovered:
// config validation error paths, alpha-beta timestamped dt branches, the
// steady platform clock, diagnostic payload truncation, config migration
// versions, and safety-envelope zone classification.

#include "safety_core/config/env_loader.hpp"
#include "safety_core/config/migration.hpp"
#include "safety_core/config/validation.hpp"
#include "safety_core/diag/diagnostic_transport.hpp"
#include "safety_core/diag/logging_health_monitor.hpp"
#include "safety_core/filters/alpha_beta_filter.hpp"
#include "safety_core/filters/complementary_filter.hpp"
#include "safety_core/platform/clock.hpp"
#include "safety_core/platform/manual_clock.hpp"
#include "safety_core/safety/safety_envelope.hpp"
#include "safety_core/safety/safety_supervisor.hpp"
#include "safety_core/state_machine/state_machine.hpp"
#include "safety_core/system/context_factory.hpp"
#include "test_support.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace
{
    using safety_core::test_support::check;

    safety_core::config::SystemConfig valid_config()
    {
        return safety_core::test_support::make_context_defaults();
    }
} // namespace

int main()
{
    using namespace std::chrono_literals;
    using safety_core::config::SystemConfig;
    using safety_core::config::ValidationPolicy;

    bool ok = true;

    // ------------------------------------------------------------------
    // config::validate — every error branch
    // ------------------------------------------------------------------
    {
        SystemConfig cfg = valid_config();
        ok &= check(safety_core::config::validate(cfg).ok, "valid config should pass strict validation");

        cfg                = valid_config();
        cfg.config_version = 99U;
        ok &= check(!safety_core::config::validate(cfg).ok, "wrong config_version should fail");

        cfg           = valid_config();
        cfg.max_tasks = 0U;
        ok &= check(!safety_core::config::validate(cfg).ok, "max_tasks=0 should fail");

        cfg           = valid_config();
        cfg.max_tasks = 101U;
        ok &= check(!safety_core::config::validate(cfg).ok, "max_tasks>100 should fail");

        cfg                       = valid_config();
        cfg.timing.control_period = 0ns;
        ok &= check(!safety_core::config::validate(cfg).ok, "control_period<=0 should fail");

        cfg                       = valid_config();
        cfg.timing.control_period = 500us;
        ok &= check(!safety_core::config::validate(cfg).ok, "control_period<1ms should fail");

        cfg                        = valid_config();
        cfg.timing.control_period  = 100ms;
        cfg.timing.watchdog_period = 50ms;
        ok &= check(!safety_core::config::validate(cfg).ok, "watchdog<control should fail");

        cfg                             = valid_config();
        cfg.timing.localization_timeout = 0ms;
        ok &= check(!safety_core::config::validate(cfg).ok, "localization_timeout<=0 should fail");

        cfg                        = valid_config();
        cfg.envelope.max_speed_mps = std::numeric_limits<double>::quiet_NaN();
        ok &= check(!safety_core::config::validate(cfg).ok, "NaN max_speed should fail");

        cfg                            = valid_config();
        cfg.envelope.control_latency_s = std::numeric_limits<double>::infinity();
        ok &= check(!safety_core::config::validate(cfg).ok, "Inf latency should fail");

        cfg                        = valid_config();
        cfg.envelope.max_speed_mps = -1.0;
        ok &= check(!safety_core::config::validate(cfg).ok, "negative max_speed should fail");

        cfg                         = valid_config();
        cfg.envelope.max_accel_mps2 = 0.0;
        ok &= check(!safety_core::config::validate(cfg).ok, "zero max_accel should fail");

        cfg                        = valid_config();
        cfg.envelope.max_speed_mps = 101.0;
        ok &= check(!safety_core::config::validate(cfg).ok, "max_speed>100 should fail");

        cfg                         = valid_config();
        cfg.envelope.max_accel_mps2 = 51.0;
        ok &= check(!safety_core::config::validate(cfg).ok, "max_accel>50 should fail");

        cfg                            = valid_config();
        cfg.envelope.control_latency_s = -0.1;
        ok &= check(!safety_core::config::validate(cfg).ok, "negative latency should fail");

        cfg                            = valid_config();
        cfg.envelope.control_latency_s = 1.5;
        ok &= check(!safety_core::config::validate(cfg).ok, "latency>1s should fail");

        cfg                          = valid_config();
        cfg.envelope.safety_buffer_m = -0.1;
        ok &= check(!safety_core::config::validate(cfg).ok, "negative buffer should fail");

        cfg                          = valid_config();
        cfg.envelope.safety_buffer_m = 101.0;
        ok &= check(!safety_core::config::validate(cfg).ok, "buffer>100m should fail");

        // Zero buffer: Strict rejects, AllowWarnings warns but passes.
        cfg                          = valid_config();
        cfg.envelope.safety_buffer_m = 0.0;
        const auto strict            = safety_core::config::validate(cfg, ValidationPolicy::Strict);
        ok &= check(!strict.ok, "zero buffer should fail strict policy");
        const auto lenient = safety_core::config::validate(cfg, ValidationPolicy::AllowWarnings);
        ok &= check(lenient.ok && lenient.warning, "zero buffer should warn under AllowWarnings");
    }

    // ------------------------------------------------------------------
    // AlphaBetaFilter — timestamped dt branches and config fallback
    // ------------------------------------------------------------------
    {
        safety_core::filters::AlphaBetaFilter filter;

        // First timestamped update: last_timestamp_ns_ == 0 -> default dt.
        const double first = filter.update(1.0, 1000000U);
        ok &= check(std::isfinite(first), "first timestamped update should be finite");

        // Increasing timestamp -> dt derived from timestamps.
        const double second = filter.update(1.1, 101000000U);
        ok &= check(std::isfinite(second), "second update should use measured dt");

        // Non-increasing timestamp -> dt <= 0 -> fallback to configured dt.
        const double third = filter.update(1.2, 50000000U);
        ok &= check(std::isfinite(third), "non-monotonic timestamp should fall back to default dt");

        // With an applied config, dt_seconds() uses control_period.
        safety_core::filters::AlphaBetaFilter configured;
        SystemConfig cfg = valid_config();
        configured.apply_config(cfg);
        const double with_cfg = configured.update(0.5);
        ok &= check(std::isfinite(with_cfg), "configured filter should produce finite output");

        // Config with non-positive control period falls back to 0.1s.
        safety_core::filters::AlphaBetaFilter zero_period;
        cfg.timing.control_period = 0ns;
        zero_period.apply_config(cfg);
        const double fallback = zero_period.update(0.5);
        ok &= check(std::isfinite(fallback), "zero control_period should fall back to default dt");

        // set_params and reset reach the remaining setters.
        configured.set_params(safety_core::filters::AlphaBetaParams{0.5, 0.01});
        configured.reset(3.0, 1.0);
        ok &= check(configured.position() == 3.0 && configured.velocity() == 1.0,
                    "reset should restore position and velocity");
    }

    // ------------------------------------------------------------------
    // SteadyClock singleton
    // ------------------------------------------------------------------
    {
        using safety_core::platform::SteadyClock;
        SteadyClock& first  = SteadyClock::instance();
        SteadyClock& second = SteadyClock::instance();
        ok &= check(&first == &second, "instance() should be a singleton");
        ok &= check(SteadyClock::instance_ptr() == &first, "instance_ptr() should return the singleton");
        ok &= check(first.now().time_since_epoch().count() > 0, "now() should return a positive steady time");
    }

    // ------------------------------------------------------------------
    // DiagnosticEvent truncation boundaries
    // ------------------------------------------------------------------
    {
        safety_core::diag::DiagnosticEvent event{};

        const std::string short_topic(10U, 't');
        event.set_topic(short_topic);
        ok &= check(!event.topic_truncated, "short topic should not truncate");
        ok &= check(event.topic_view() == short_topic, "short topic should be stored verbatim");

        const std::string exact_topic(safety_core::diag::kDiagnosticTopicCapacity - 1U, 'x');
        event.set_topic(exact_topic);
        ok &= check(!event.topic_truncated, "topic at capacity-1 should not truncate");
        ok &= check(event.topic_view() == exact_topic, "capacity-1 topic should be stored verbatim");

        const std::string long_topic(safety_core::diag::kDiagnosticTopicCapacity + 8U, 'y');
        event.set_topic(long_topic);
        ok &= check(event.topic_truncated, "oversized topic should set truncation flag");
        ok &= check(event.topic_view().size() == safety_core::diag::kDiagnosticTopicCapacity - 1U,
                    "truncated topic should be capped at capacity-1");

        const std::string short_payload(10U, 'p');
        event.set_payload(short_payload);
        ok &= check(!event.payload_truncated, "short payload should not truncate");

        const std::string long_payload(safety_core::diag::kDiagnosticPayloadCapacity + 16U, 'z');
        event.set_payload(long_payload);
        ok &= check(event.payload_truncated, "oversized payload should set truncation flag");
        ok &= check(event.payload_view().size() == safety_core::diag::kDiagnosticPayloadCapacity - 1U,
                    "truncated payload should be capped at capacity-1");
    }

    // ------------------------------------------------------------------
    // migrate_to_current — all version branches
    // ------------------------------------------------------------------
    {
        SystemConfig current = valid_config();
        const auto no_op     = safety_core::config::migrate_to_current(current);
        ok &= check(no_op.ok && !no_op.migrated, "current version should pass through unmigrated");

        SystemConfig legacy    = valid_config();
        legacy.config_version  = safety_core::config::kLegacySystemConfigVersion;
        const auto migrated_v1 = safety_core::config::migrate_to_current(legacy);
        ok &= check(migrated_v1.ok && migrated_v1.migrated, "legacy v1 config should migrate to current");
        ok &= check(migrated_v1.config.config_version == safety_core::config::kCurrentSystemConfigVersion,
                    "migrated config should carry current version");

        SystemConfig unversioned   = valid_config();
        unversioned.config_version = 0U;
        const auto migrated_v0     = safety_core::config::migrate_to_current(unversioned);
        ok &= check(migrated_v0.ok && migrated_v0.migrated, "unversioned config should migrate to current");

        SystemConfig future    = valid_config();
        future.config_version  = 999U;
        const auto unsupported = safety_core::config::migrate_to_current(future);
        ok &= check(!unsupported.ok, "unsupported future version should fail migration");
    }

    // ------------------------------------------------------------------
    // evaluate_stop_distance — all four zones + overloads
    // ------------------------------------------------------------------
    {
        using safety_core::safety::evaluate_stop_distance;
        using safety_core::safety::SafetyZone;

        // Clear: distance far beyond required clearance.
        auto eval = evaluate_stop_distance(100.0, 1.0, 1.0, 0.1, 0.2);
        ok &= check(eval.zone == SafetyZone::Clear, "far obstacle should classify as Clear");
        ok &= check(eval.recommended_speed_limit_mps == 1.0, "Clear zone should report current speed");

        // Warning: within envelope but inside 1.5x required clearance.
        // speed=1.0, decel=1.0, latency=0.1, buffer=0.2 -> required = 0.1+0.5+0.2 = 0.8
        eval = evaluate_stop_distance(1.0, 1.0, 1.0, 0.1, 0.2);
        ok &= check(eval.zone == SafetyZone::Warning, "obstacle inside 1.5x clearance should be Warning");
        ok &= check(eval.recommended_speed_limit_mps > 0.0 && std::isfinite(eval.recommended_speed_limit_mps),
                    "Warning zone should recommend a finite positive speed");

        // Protective: within buffer check fails but obstacle beyond buffer.
        eval = evaluate_stop_distance(0.5, 1.5, 0.8, 0.2, 0.1);
        ok &= check(eval.zone == SafetyZone::Protective && !eval.within_envelope,
                    "breached envelope should classify as Protective");
        ok &= check(eval.recommended_speed_limit_mps == 0.0, "Protective zone should command zero speed");

        // Emergency: obstacle inside the safety buffer.
        eval = evaluate_stop_distance(0.05, 0.5, 1.0, 0.1, 0.2);
        ok &= check(eval.zone == SafetyZone::Emergency, "obstacle inside buffer should be Emergency");
        ok &= check(eval.recommended_speed_limit_mps == 0.0, "Emergency zone should command zero speed");

        // Non-positive decel clamps to zero -> infinite braking distance ->
        // envelope is always breached (Protective).
        eval = evaluate_stop_distance(10.0, 1.0, 0.0, 0.1, 0.2);
        ok &= check(eval.zone == SafetyZone::Protective && std::isinf(eval.required_clearance),
                    "zero decel should force infinite clearance and Protective zone");

        // Negative speed clamps to zero: stationary vehicle is always within
        // envelope and (at buffer distance) lands in the Warning band.
        eval = evaluate_stop_distance(0.5, -2.0, 1.0, 0.0, 0.5);
        ok &= check(eval.zone == SafetyZone::Warning, "negative speed should clamp to zero");

        // Warning zone with zero available headroom: distance == buffer while
        // stopped -> required == buffer -> within envelope, Warning band, but
        // available == 0 so no speed recommendation is made.
        eval = evaluate_stop_distance(0.5, 0.0, 1.0, 0.0, 0.5);
        ok &= check(eval.zone == SafetyZone::Warning, "boundary distance should classify as Warning");
        ok &= check(eval.recommended_speed_limit_mps == 0.0, "zero headroom should leave speed recommendation at zero");

        // MotionEnvelopeConfig overload.
        safety_core::config::MotionEnvelopeConfig envelope{};
        envelope.max_comfort_decel_mps2 = 1.0;
        envelope.control_latency_s      = 0.1;
        envelope.safety_buffer_m        = 0.2;
        eval                            = evaluate_stop_distance(100.0, 1.0, envelope);
        ok &= check(eval.zone == SafetyZone::Clear, "envelope-config overload should classify Clear");

        // Footprint overload: front overhang reduces effective distance.
        safety_core::safety::RobotFootprint footprint{};
        footprint.front_overhang_m = 0.4;
        eval                       = evaluate_stop_distance(0.5, 0.5, envelope, footprint);
        ok &= check(eval.zone == SafetyZone::Emergency, "overhang should push close obstacle to Emergency");
    }

    // ------------------------------------------------------------------
    // ComplementaryFilter — derived alpha, timestamped dt, NaN clamp
    // ------------------------------------------------------------------
    {
        safety_core::filters::ComplementaryFilter filter;
        filter.set_params(safety_core::filters::ComplementaryFilterParams{0.98, 1000.0, 5.0});

        // First timestamped update -> default dt.
        const double first = filter.update(0.1, 1.0, 1000000U);
        ok &= check(std::isfinite(first), "first complementary update should be finite");

        // Increasing timestamp -> measured dt, cutoff-derived alpha.
        const double second = filter.update(0.1, 1.0, 101000000U);
        ok &= check(std::isfinite(second), "cutoff-derived alpha should produce finite state");

        // Non-increasing timestamp -> fallback dt path.
        const double third = filter.update(0.1, 1.0, 1U);
        ok &= check(std::isfinite(third), "non-monotonic timestamp should fall back to default dt");

        // cutoff_frequency_hz == 0 -> params_.alpha clamped into [0,1].
        safety_core::filters::ComplementaryFilter static_alpha;
        static_alpha.set_params(safety_core::filters::ComplementaryFilterParams{2.5, 1000.0, 0.0});
        const double over_alpha = static_alpha.update(0.0, 1.0);
        ok &= check(std::isfinite(over_alpha), "alpha > 1 should clamp and stay finite");

        // NaN absolute measurement must clamp state to 0, not propagate.
        filter.reset(0.0);
        const double nan_result = filter.update(0.0, std::numeric_limits<double>::quiet_NaN(), 200000000U);
        ok &= check(nan_result == 0.0, "NaN measurement should clamp state to zero");

        // Configured control period feeds dt_seconds().
        safety_core::filters::ComplementaryFilter configured;
        SystemConfig cfg = valid_config();
        configured.apply_config(cfg);
        ok &= check(std::isfinite(configured.update(0.0, 0.5)), "configured filter should stay finite");

        cfg.timing.control_period = 0ns;
        safety_core::filters::ComplementaryFilter zero_period;
        zero_period.apply_config(cfg);
        ok &= check(std::isfinite(zero_period.update(0.0, 0.5)), "zero control period should fall back to default dt");
    }

    // ------------------------------------------------------------------
    // SafetySupervisor — remaining escalation edges
    // ------------------------------------------------------------------
    {
        using safety_core::safety::MonitorEvent;
        using safety_core::safety::MonitorSeverity;
        using safety_core::safety::MonitorType;
        using safety_core::safety::SafetySupervisor;
        using safety_core::sm::Mode;
        using safety_core::sm::ModeStateMachine;

        // Degraded escalation without a state machine is a no-op, not a fault.
        SafetySupervisor no_machine;
        const MonitorEvent degraded{MonitorType::TrajectoryEnvelope, MonitorSeverity::Degraded, 5U, "envelope"};
        ok &=
            check(no_machine.process_monitor_event(degraded).ok(), "degraded event without machine should be a no-op");

        // Warning escalation with a machine but no transport: publish path
        // must return early without crashing.
        ModeStateMachine machine;
        static_cast<void>(machine.transition_to(Mode::Idle));
        SafetySupervisor no_transport(&machine, nullptr, nullptr);
        const MonitorEvent warning{MonitorType::TrajectoryEnvelope, MonitorSeverity::Warning, 6U, "env warn"};
        ok &=
            check(no_transport.process_monitor_event(warning).ok(), "warning with null transport should still succeed");
        ok &= check(machine.mode() == Mode::Idle, "warning must not change mode");

        // monitor_name coverage: trajectory_envelope + diagnostic_drop events
        // exercise the remaining switch arms via event publication.
        safety_core::test_support::CaptureTransport transport;
        ModeStateMachine named_machine(nullptr, &transport);
        static_cast<void>(named_machine.transition_to(Mode::Idle));
        SafetySupervisor named(&named_machine, &transport, nullptr);
        const MonitorEvent env_warning{MonitorType::TrajectoryEnvelope, MonitorSeverity::Warning, 7U, "env"};
        static_cast<void>(named.process_monitor_event(env_warning));
        ok &= check(!transport.events.empty(), "named monitor event should publish");
    }

    // ------------------------------------------------------------------
    // env_loader — bounded-uint and non-finite-double fallbacks
    // ------------------------------------------------------------------
    {
        using safety_core::test_support::clear_config_env_vars;
        clear_config_env_vars();

        const SystemConfig defaults = valid_config();

        setenv("SAFETY_CORE_MAX_TASKS", "999999", 1);
        SystemConfig cfg = safety_core::config::load_from_env(defaults);
        ok &= check(cfg.max_tasks == defaults.max_tasks, "out-of-range task count should fall back");

        setenv("SAFETY_CORE_MAX_SPEED_MPS", "nan", 1);
        cfg = safety_core::config::load_from_env(defaults);
        ok &=
            check(cfg.envelope.max_speed_mps == defaults.envelope.max_speed_mps, "NaN speed override should fall back");

        setenv("SAFETY_CORE_MAX_SPEED_MPS", "inf", 1);
        cfg = safety_core::config::load_from_env(defaults);
        ok &= check(cfg.envelope.max_speed_mps == defaults.envelope.max_speed_mps,
                    "infinite speed override should fall back");

        setenv("SAFETY_CORE_MAX_TASKS", "4", 1);
        setenv("SAFETY_CORE_MAX_SPEED_MPS", "1.25", 1);
        cfg = safety_core::config::load_from_env(defaults);
        ok &= check(cfg.max_tasks == 4U && cfg.envelope.max_speed_mps == 1.25, "valid overrides should apply");

        clear_config_env_vars();
    }

    // ------------------------------------------------------------------
    // build_context — null transport / null clock edges
    // ------------------------------------------------------------------
    {
        safety_core::system::ContextWithConfig out{};
        safety_core::system::ContextFactoryOptions options{};
        options.defaults          = valid_config();
        options.validation_policy = safety_core::config::ValidationPolicy::AllowWarnings;

        const auto res = safety_core::system::build_context(out, options);
        ok &= check(res.ok(), "build_context with minimal options should succeed");
        ok &= check(out.context.clock != nullptr, "context should default to the steady clock");
        ok &= check(out.context.config == &out.config, "context config should point at built config");
    }

    // ------------------------------------------------------------------
    // LoggingHealthMonitor — both callbacks, with and without clock
    // ------------------------------------------------------------------
    {
        std::ostringstream sink;
        safety_core::diag::LoggingHealthMonitor monitor(sink);
        monitor.on_mode_transition(safety_core::sm::Mode::Init, safety_core::sm::Mode::Idle);
        monitor.on_fault_latched(11U);
        ok &= check(sink.str().find("mode_transition") != std::string::npos &&
                        sink.str().find("fault_latched") != std::string::npos,
                    "monitor should log transition and fault events");

        std::ostringstream sink2;
        safety_core::platform::ManualClock clock;
        clock.advance(std::chrono::nanoseconds{42});
        safety_core::diag::LoggingHealthMonitor timed(sink2, &clock);
        timed.on_fault_latched(3U);
        ok &= check(sink2.str().find("ts=42") != std::string::npos, "clock should feed event timestamps");
    }

    if (!ok)
    {
        return 1;
    }

    std::cout << "[PASS] coverage gaps tests" << '\n';
    return 0;
}
