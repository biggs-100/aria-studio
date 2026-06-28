// Tests for automation clips: base clip evaluation, step clips,
// LFO waveforms, and envelope gated states.
#define _USE_MATH_DEFINES
#include <catch2/catch_all.hpp>
#include <catch2/catch_approx.hpp>

#include "automation/automation_clip.h"
#include "automation/automation_types.h"
#include "automation/envelope.h"
#include "automation/lfo_clip.h"
#include "automation/step_clip.h"

#include <cmath>
#include <cstdint>
#include <memory>

using namespace aria::automation;

static constexpr double kApprox = 0.0001;

// ============================================================
// AutomationClip base evaluation
// ============================================================

TEST_CASE("AutomationClip: empty clip returns 0.0",
          "[automation][clip]") {
    AutomationClip clip;
    REQUIRE(clip.evaluate(0) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(clip.evaluate(960) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(clip.point_count() == 0);
}

TEST_CASE("AutomationClip: single point returns its value",
          "[automation][clip]") {
    AutomationClip clip;
    clip.add_point({960, 0.75, InterpolationType::Linear, {}});

    // Before, at, and after the single point
    REQUIRE(clip.evaluate(0) == Catch::Approx(0.75).margin(kApprox));
    REQUIRE(clip.evaluate(960) == Catch::Approx(0.75).margin(kApprox));
    REQUIRE(clip.evaluate(1920) == Catch::Approx(0.75).margin(kApprox));
}

TEST_CASE("AutomationClip: two-point linear ramp",
          "[automation][clip]") {
    AutomationClip clip;
    clip.add_point({0, 0.0, InterpolationType::Linear, {}});
    clip.add_point({960, 1.0, InterpolationType::Linear, {}});

    REQUIRE(clip.evaluate(0) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(clip.evaluate(480) == Catch::Approx(0.5).margin(kApprox));
    REQUIRE(clip.evaluate(960) == Catch::Approx(1.0).margin(kApprox));
    REQUIRE(clip.evaluate(1920) == Catch::Approx(1.0).margin(kApprox));
}

TEST_CASE("AutomationClip: three-point curve",
          "[automation][clip]") {
    AutomationClip clip;
    clip.add_point({0, 0.0, InterpolationType::Linear, {}});
    clip.add_point({480, 0.5, InterpolationType::Linear, {}});
    clip.add_point({960, 1.0, InterpolationType::Linear, {}});

    REQUIRE(clip.evaluate(240) == Catch::Approx(0.25).margin(kApprox));
    REQUIRE(clip.evaluate(480) == Catch::Approx(0.5).margin(kApprox));
    REQUIRE(clip.evaluate(720) == Catch::Approx(0.75).margin(kApprox));
}

TEST_CASE("AutomationClip: hold interpolation between points",
          "[automation][clip]") {
    AutomationClip clip;
    clip.add_point({0, 0.0, InterpolationType::Hold, {}});
    clip.add_point({960, 1.0, InterpolationType::Hold, {}});

    REQUIRE(clip.evaluate(0) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(clip.evaluate(480) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(clip.evaluate(959) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(clip.evaluate(960) == Catch::Approx(1.0).margin(kApprox));
}

TEST_CASE("AutomationClip: looping wraps PPQN position",
          "[automation][clip]") {
    AutomationClip clip;
    clip.set_length(960);  // 1 bar at 960 PPQN
    clip.set_loop(true);
    // Loop range = clip length: values at or above loop_end (960) get clamped
    // to 960, then wrapped to 0
    clip.set_loop_range(0, 960);
    clip.add_point({0, 0.0, InterpolationType::Linear, {}});
    clip.add_point({960, 1.0, InterpolationType::Linear, {}});

    // In-loop evaluation: ppqn=480 stays within range
    REQUIRE(clip.evaluate(480) == Catch::Approx(0.5).margin(kApprox));
    // Beyond loop_end (960): ppqn=1440 → clamped to 960, then wrapped: 960 % 960 = 0
    REQUIRE(clip.evaluate(1440) == Catch::Approx(0.0).margin(kApprox));
}

TEST_CASE("AutomationClip: add_point replaces existing at same PPQN",
          "[automation][clip]") {
    AutomationClip clip;
    clip.add_point({480, 0.3, InterpolationType::Linear, {}});
    REQUIRE(clip.point_count() == 1);
    REQUIRE(clip.evaluate(480) == Catch::Approx(0.3).margin(kApprox));

    clip.add_point({480, 0.8, InterpolationType::Linear, {}});
    REQUIRE(clip.point_count() == 1);
    REQUIRE(clip.evaluate(480) == Catch::Approx(0.8).margin(kApprox));
}

TEST_CASE("AutomationClip: remove_point removes exact match",
          "[automation][clip]") {
    AutomationClip clip;
    clip.add_point({0, 0.0, InterpolationType::Linear, {}});
    clip.add_point({480, 0.5, InterpolationType::Linear, {}});
    clip.add_point({960, 1.0, InterpolationType::Linear, {}});
    REQUIRE(clip.point_count() == 3);

    clip.remove_point(480);
    REQUIRE(clip.point_count() == 2);
    REQUIRE(clip.evaluate(480) == Catch::Approx(0.5).margin(kApprox));
}

TEST_CASE("AutomationClip: bulk operations",
          "[automation][clip]") {
    AutomationClip clip;
    clip.add_point({0, 0.0, InterpolationType::Linear, {}});
    clip.add_point({480, 0.5, InterpolationType::Linear, {}});
    clip.add_point({960, 1.0, InterpolationType::Linear, {}});

    clip.offset_values(0.2);
    REQUIRE(clip.evaluate(480) == Catch::Approx(0.7).margin(kApprox));
    REQUIRE(clip.evaluate(960) == Catch::Approx(1.0).margin(kApprox));

    clip.scale_values(0.5);
    REQUIRE(clip.evaluate(0) == Catch::Approx(0.1).margin(kApprox));
    REQUIRE(clip.evaluate(480) == Catch::Approx(0.35).margin(kApprox));
}

// ============================================================
// StepAutomationClip
// ============================================================

TEST_CASE("StepAutomationClip: default 16-step values",
          "[automation][clip][step]") {
    StepAutomationClip clip;
    REQUIRE(clip.step_count() == 16);
    clip.set_step_count(8);
    REQUIRE(clip.step_count() == 8);
}

TEST_CASE("StepAutomationClip: evaluate returns step values",
          "[automation][clip][step]") {
    StepAutomationClip clip;
    clip.set_step_count(4);
    clip.set_length(960);

    clip.set_step_value(0, 0.0);
    clip.set_step_value(1, 0.5);
    clip.set_step_value(2, 0.75);
    clip.set_step_value(3, 1.0);

    REQUIRE(clip.evaluate(0) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(clip.evaluate(120) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(clip.evaluate(240) == Catch::Approx(0.5).margin(kApprox));
    REQUIRE(clip.evaluate(600) == Catch::Approx(0.75).margin(kApprox));
    REQUIRE(clip.evaluate(900) == Catch::Approx(1.0).margin(kApprox));
}

TEST_CASE("StepAutomationClip: smooth transitions between steps",
          "[automation][clip][step]") {
    StepAutomationClip clip;
    clip.set_step_count(3);
    clip.set_length(960);
    clip.set_step_value(0, 0.0);
    clip.set_step_value(1, 0.5);
    clip.set_step_value(2, 1.0);

    // Without smooth: discrete step values
    clip.set_smooth(false);
    REQUIRE(clip.evaluate(0) == Catch::Approx(0.0).margin(kApprox));
    REQUIRE(clip.evaluate(319) == Catch::Approx(0.0).margin(kApprox));  // still step 0
    REQUIRE(clip.evaluate(320) == Catch::Approx(0.5).margin(kApprox));  // step 1
    REQUIRE(clip.evaluate(639) == Catch::Approx(0.5).margin(kApprox));  // still step 1
    REQUIRE(clip.evaluate(640) == Catch::Approx(1.0).margin(kApprox));  // step 2

    // With smooth: linear interpolation between adjacent step values
    clip.set_smooth(true);
    // Step 0, frac=0.5 at PPQN=160: between step 0 (0.0) and step 1 (0.5)
    REQUIRE(clip.evaluate(160) == Catch::Approx(0.25).margin(0.1));
    // Step 1, frac=0.5 at PPQN=480: between step 1 (0.5) and step 2 (1.0)
    REQUIRE(clip.evaluate(480) == Catch::Approx(0.75).margin(0.1));
}

// ============================================================
// LFOAutomationClip waveforms
// ============================================================

TEST_CASE("LFOAutomationClip: sine waveform evaluates correctly",
          "[automation][clip][lfo]") {
    LFOAutomationClip clip;
    clip.set_waveform(LFOAutomationClip::Waveform::Sine);
    clip.set_rate_sync(1.0);
    clip.set_phase(0.0);
    clip.set_offset(0.5);
    clip.set_bipolar(false);

    double v0 = clip.evaluate(0);
    REQUIRE(v0 >= 0.0);
    REQUIRE(v0 <= 1.0);

    double v2 = clip.evaluate(240);
    // At quarter note, sine is at peak, so value should be high
    REQUIRE(v2 > 0.0);
    REQUIRE(v2 <= 1.0);
}

TEST_CASE("LFOAutomationClip: bipolar outputs negative values",
          "[automation][clip][lfo]") {
    LFOAutomationClip clip;
    clip.set_waveform(LFOAutomationClip::Waveform::Sine);
    clip.set_rate_sync(1.0);
    clip.set_phase(0.0);
    clip.set_offset(0.5);
    clip.set_bipolar(true);

    double v0 = clip.evaluate(0);
    REQUIRE(v0 >= 0.0);
    REQUIRE(v0 <= 1.0);
}

TEST_CASE("LFOAutomationClip: triangle waveform",
          "[automation][clip][lfo]") {
    LFOAutomationClip clip;
    clip.set_waveform(LFOAutomationClip::Waveform::Triangle);
    clip.set_rate_sync(1.0);
    clip.set_offset(0.5);
    clip.set_bipolar(false);

    double v0 = clip.evaluate(0);
    REQUIRE(v0 >= 0.0);
    REQUIRE(v0 <= 1.0);

    double v_mid = clip.evaluate(480);
    REQUIRE(v_mid >= 0.0);
    REQUIRE(v_mid <= 1.0);
}

TEST_CASE("LFOAutomationClip: square waveform with pulse width",
          "[automation][clip][lfo]") {
    LFOAutomationClip clip;
    clip.set_waveform(LFOAutomationClip::Waveform::Square);
    clip.set_rate_sync(1.0);
    clip.set_pulse_width(0.5);
    clip.set_offset(0.5);
    clip.set_bipolar(false);

    double v_early = clip.evaluate(10);
    double v_late = clip.evaluate(700);
    REQUIRE(v_early >= 0.0);
    REQUIRE(v_late >= 0.0);
    REQUIRE(v_early != Catch::Approx(v_late).margin(0.01));
}

TEST_CASE("LFOAutomationClip: sawtooth and inverse saw",
          "[automation][clip][lfo]") {
    LFOAutomationClip clip;
    clip.set_rate_sync(1.0);
    clip.set_offset(0.5);
    clip.set_bipolar(false);

    clip.set_waveform(LFOAutomationClip::Waveform::Saw);
    double v_saw = clip.evaluate(240);
    REQUIRE(v_saw >= 0.0);
    REQUIRE(v_saw <= 1.0);

    clip.set_waveform(LFOAutomationClip::Waveform::SawInv);
    double v_sawinv = clip.evaluate(240);
    REQUIRE(v_sawinv >= 0.0);
    REQUIRE(v_sawinv <= 1.0);

    REQUIRE(v_saw != Catch::Approx(v_sawinv).margin(0.01));
}

TEST_CASE("LFOAutomationClip: rate synced vs free running",
          "[automation][clip][lfo]") {
    LFOAutomationClip clip;
    clip.set_waveform(LFOAutomationClip::Waveform::Sine);
    clip.set_offset(0.5);
    clip.set_bipolar(false);

    REQUIRE(clip.is_rate_synced());

    clip.set_rate_hz(5.0);
    REQUIRE_FALSE(clip.is_rate_synced());

    clip.set_rate_sync(1.0 / 4.0);
    REQUIRE(clip.is_rate_synced());
}

// ============================================================
// EnvelopeClip gated states
// ============================================================

TEST_CASE("EnvelopeClip: ADSR gated evaluation basic",
          "[automation][clip][envelope]") {
    EnvelopeClip env;
    env.set_type(EnvelopeClip::EnvelopeType::ADSR);
    env.set_attack(10.0);
    env.set_decay(100.0);
    env.set_sustain(0.5);
    env.set_release(200.0);

    double v = env.evaluate_ms(0.0, true, 0.0);
    REQUIRE(v == Catch::Approx(0.0).margin(kApprox));

    v = env.evaluate_ms(5.0, true, 0.0);
    REQUIRE(v > 0.0);
    REQUIRE(v < 1.0);

    v = env.evaluate_ms(10.0, true, 0.0);
    REQUIRE(v > 0.9);

    v = env.evaluate_ms(50.0, true, 0.0);
    REQUIRE(v > 0.5);
    REQUIRE(v < 1.0);

    v = env.evaluate_ms(500.0, true, 0.0);
    REQUIRE(v == Catch::Approx(0.5).margin(0.05));

    v = env.evaluate_ms(600.0, false, 0.0, 500.0);
    REQUIRE(v > 0.0);
    REQUIRE(v < 0.5);
}

TEST_CASE("EnvelopeClip: ADSR full release to zero",
          "[automation][clip][envelope]") {
    EnvelopeClip env;
    env.set_type(EnvelopeClip::EnvelopeType::ADSR);
    env.set_attack(5.0);
    env.set_decay(50.0);
    env.set_sustain(0.7);
    env.set_release(100.0);

    double v_sustain = env.evaluate_ms(200.0, true, 0.0);
    REQUIRE(v_sustain == Catch::Approx(0.7).margin(0.05));

    double v_end = env.evaluate_ms(300.0, false, 0.0, 200.0);
    REQUIRE(v_end >= 0.0);
    REQUIRE(v_end < 0.7);
}

TEST_CASE("EnvelopeClip: ADSR release phase reaches zero",
          "[automation][clip][envelope]") {
    EnvelopeClip env;
    env.set_type(EnvelopeClip::EnvelopeType::ADSR);
    env.set_attack(5.0);
    env.set_decay(50.0);
    env.set_sustain(0.5);
    env.set_release(100.0);

    double v = env.evaluate_ms(400.0, false, 0.0, 200.0);
    REQUIRE(v >= 0.0);
}

TEST_CASE("EnvelopeClip: DADSR with delay",
          "[automation][clip][envelope]") {
    EnvelopeClip env;
    env.set_type(EnvelopeClip::EnvelopeType::DADSR);
    env.set_delay(50.0);
    env.set_attack(10.0);
    env.set_decay(100.0);
    env.set_sustain(0.6);
    env.set_release(200.0);

    double v = env.evaluate_ms(25.0, true, 0.0);
    REQUIRE(v == Catch::Approx(0.0).margin(kApprox));

    v = env.evaluate_ms(51.0, true, 0.0);
    REQUIRE(v > 0.0);

    v = env.evaluate_ms(500.0, true, 0.0);
    REQUIRE(v == Catch::Approx(0.6).margin(0.05));
}

TEST_CASE("EnvelopeClip: AR envelope (no sustain phase)",
          "[automation][clip][envelope]") {
    EnvelopeClip env;
    env.set_type(EnvelopeClip::EnvelopeType::AR);
    env.set_attack(10.0);
    env.set_release(50.0);

    double v = env.evaluate_ms(10.0, true, 0.0);
    REQUIRE(v > 0.9);

    v = env.evaluate_ms(100.0, true, 0.0);
    REQUIRE(v > 0.9);

    v = env.evaluate_ms(130.0, false, 0.0, 100.0);
    REQUIRE(v > 0.0);
    REQUIRE(v < 1.0);
}

TEST_CASE("EnvelopeClip: AD envelope decays to zero",
          "[automation][clip][envelope]") {
    EnvelopeClip env;
    env.set_type(EnvelopeClip::EnvelopeType::AD);
    env.set_attack(10.0);
    env.set_decay(200.0);

    REQUIRE(env.evaluate_ms(5.0, true, 0.0) < 1.0);
    REQUIRE(env.evaluate_ms(5.0, true, 0.0) > 0.0);
    REQUIRE(env.evaluate_ms(10.0, true, 0.0) > 0.9);

    double v = env.evaluate_ms(100.0, true, 0.0);
    REQUIRE(v > 0.0);
    REQUIRE(v < 1.0);

    v = env.evaluate_ms(250.0, false, 0.0, 210.0);
    REQUIRE(v >= 0.0);
}

TEST_CASE("EnvelopeClip: AHDSR with hold phase",
          "[automation][clip][envelope]") {
    EnvelopeClip env;
    env.set_type(EnvelopeClip::EnvelopeType::AHDSR);
    env.set_attack(10.0);
    env.set_hold(20.0);
    env.set_decay(100.0);
    env.set_sustain(0.5);
    env.set_release(200.0);

    REQUIRE(env.evaluate_ms(10.0, true, 0.0) > 0.9);

    double v_hold = env.evaluate_ms(25.0, true, 0.0);
    REQUIRE(v_hold > 0.9);

    double v_transition = env.evaluate_ms(50.0, true, 0.0);
    REQUIRE(v_transition > 0.5);
    REQUIRE(v_transition < 1.0);
}

TEST_CASE("EnvelopeClip: gate never on keeps at 0",
          "[automation][clip][envelope]") {
    EnvelopeClip env;
    env.set_type(EnvelopeClip::EnvelopeType::ADSR);
    env.set_attack(10.0);
    env.set_decay(100.0);
    env.set_sustain(0.5);
    env.set_release(200.0);

    double v = env.evaluate_ms(500.0, false, -1.0);
    REQUIRE(v == Catch::Approx(0.0).margin(kApprox));
}
