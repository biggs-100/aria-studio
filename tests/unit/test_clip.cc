#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "model/clip.h"

#include <cmath>

using namespace aria;

// ─── evaluate_fade ───────────────────────────────────────────────

TEST_CASE("evaluate_fade — at t=0 (fade start)", "[model][clip][fade]") {
    SECTION("None → 1.0") {
        REQUIRE(Clip::evaluate_fade(FadeShape::None, 0.0) == Catch::Approx(1.0));
    }

    SECTION("LinearIn → 0.0") {
        REQUIRE(Clip::evaluate_fade(FadeShape::LinearIn, 0.0) == Catch::Approx(0.0));
    }

    SECTION("LinearOut → 1.0") {
        REQUIRE(Clip::evaluate_fade(FadeShape::LinearOut, 0.0) == Catch::Approx(1.0));
    }

    SECTION("EqualPowerIn → 0.0") {
        REQUIRE(Clip::evaluate_fade(FadeShape::EqualPowerIn, 0.0) == Catch::Approx(0.0));
    }

    SECTION("EqualPowerOut → 1.0") {
        REQUIRE(Clip::evaluate_fade(FadeShape::EqualPowerOut, 0.0) == Catch::Approx(1.0));
    }

    SECTION("ExponentialIn → 0.0") {
        REQUIRE(Clip::evaluate_fade(FadeShape::ExponentialIn, 0.0) == Catch::Approx(0.0));
    }

    SECTION("ExponentialOut → 1.0") {
        REQUIRE(Clip::evaluate_fade(FadeShape::ExponentialOut, 0.0) == Catch::Approx(1.0));
    }
}

TEST_CASE("evaluate_fade — at t=0.5 (midpoint)", "[model][clip][fade]") {
    SECTION("None → 1.0") {
        REQUIRE(Clip::evaluate_fade(FadeShape::None, 0.5) == Catch::Approx(1.0));
    }

    SECTION("LinearIn → 0.5") {
        REQUIRE(Clip::evaluate_fade(FadeShape::LinearIn, 0.5) == Catch::Approx(0.5));
    }

    SECTION("LinearOut → 0.5") {
        REQUIRE(Clip::evaluate_fade(FadeShape::LinearOut, 0.5) == Catch::Approx(0.5));
    }

    SECTION("EqualPowerIn → sqrt(0.5) ≈ 0.707") {
        REQUIRE(Clip::evaluate_fade(FadeShape::EqualPowerIn, 0.5)
                == Catch::Approx(std::sqrt(0.5)));
    }

    SECTION("EqualPowerOut → sqrt(0.5) ≈ 0.707") {
        REQUIRE(Clip::evaluate_fade(FadeShape::EqualPowerOut, 0.5)
                == Catch::Approx(std::sqrt(0.5)));
    }

    SECTION("ExponentialIn → 0.25") {
        REQUIRE(Clip::evaluate_fade(FadeShape::ExponentialIn, 0.5) == Catch::Approx(0.25));
    }

    SECTION("ExponentialOut → 0.25") {
        REQUIRE(Clip::evaluate_fade(FadeShape::ExponentialOut, 0.5) == Catch::Approx(0.25));
    }
}

TEST_CASE("evaluate_fade — at t=1.0 (fade end)", "[model][clip][fade]") {
    SECTION("None → 1.0") {
        REQUIRE(Clip::evaluate_fade(FadeShape::None, 1.0) == Catch::Approx(1.0));
    }

    SECTION("LinearIn → 1.0") {
        REQUIRE(Clip::evaluate_fade(FadeShape::LinearIn, 1.0) == Catch::Approx(1.0));
    }

    SECTION("LinearOut → 0.0") {
        REQUIRE(Clip::evaluate_fade(FadeShape::LinearOut, 1.0) == Catch::Approx(0.0));
    }

    SECTION("EqualPowerIn → 1.0") {
        REQUIRE(Clip::evaluate_fade(FadeShape::EqualPowerIn, 1.0) == Catch::Approx(1.0));
    }

    SECTION("EqualPowerOut → 0.0") {
        REQUIRE(Clip::evaluate_fade(FadeShape::EqualPowerOut, 1.0) == Catch::Approx(0.0));
    }

    SECTION("ExponentialIn → 1.0") {
        REQUIRE(Clip::evaluate_fade(FadeShape::ExponentialIn, 1.0) == Catch::Approx(1.0));
    }

    SECTION("ExponentialOut → 0.0") {
        REQUIRE(Clip::evaluate_fade(FadeShape::ExponentialOut, 1.0) == Catch::Approx(0.0));
    }
}

// ─── Clip end() ─────────────────────────────────────────────────

namespace {

/// Concrete subclass for testing the abstract Clip base.
class TestClip : public Clip {
public:
    using Clip::Clip;  // inherit constructors
};

} // anonymous namespace

TEST_CASE("Clip — position and length", "[model][clip]") {
    SECTION("end = start + length") {
        TestClip clip;
        clip.set_start(0);
        clip.set_length(960);
        REQUIRE(clip.start() == 0);
        REQUIRE(clip.length() == 960);
        REQUIRE(clip.end() == 960);
    }

    SECTION("non-zero start") {
        TestClip clip;
        clip.set_start(960);
        clip.set_length(480);
        REQUIRE(clip.start() == 960);
        REQUIRE(clip.length() == 480);
        REQUIRE(clip.end() == 1440);
    }

    SECTION("zero-length clip") {
        TestClip clip;
        clip.set_start(480);
        clip.set_length(0);
        REQUIRE(clip.start() == 480);
        REQUIRE(clip.length() == 0);
        REQUIRE(clip.end() == 480);
    }
}

TEST_CASE("Clip — fade data", "[model][clip]") {
    SECTION("default fade values") {
        TestClip clip;
        REQUIRE(clip.fade_in() == 0);
        REQUIRE(clip.fade_out() == 0);
        REQUIRE(clip.fade_in_shape() == FadeShape::None);
        REQUIRE(clip.fade_out_shape() == FadeShape::None);
    }

    SECTION("set fade in") {
        TestClip clip;
        clip.set_fade_in(96);
        clip.set_fade_in_shape(FadeShape::LinearIn);
        REQUIRE(clip.fade_in() == 96);
        REQUIRE(clip.fade_in_shape() == FadeShape::LinearIn);
    }

    SECTION("set fade out") {
        TestClip clip;
        clip.set_fade_out(48);
        clip.set_fade_out_shape(FadeShape::ExponentialOut);
        REQUIRE(clip.fade_out() == 48);
        REQUIRE(clip.fade_out_shape() == FadeShape::ExponentialOut);
    }
}

TEST_CASE("Clip — gain and mute", "[model][clip]") {
    SECTION("default gain is 1.0 (unity)") {
        TestClip clip;
        REQUIRE(clip.gain() == Catch::Approx(1.0));
    }

    SECTION("set gain") {
        TestClip clip;
        clip.set_gain(0.5);
        REQUIRE(clip.gain() == Catch::Approx(0.5));
    }

    SECTION("default is not muted") {
        TestClip clip;
        REQUIRE_FALSE(clip.is_muted());
    }

    SECTION("set muted") {
        TestClip clip;
        clip.set_muted(true);
        REQUIRE(clip.is_muted());
        clip.set_muted(false);
        REQUIRE_FALSE(clip.is_muted());
    }
}

TEST_CASE("Clip — looping", "[model][clip]") {
    SECTION("default looping is disabled") {
        TestClip clip;
        REQUIRE_FALSE(clip.is_looping());
        REQUIRE(clip.loop_start() == 0);
        REQUIRE(clip.loop_end() == 0);
    }

    SECTION("enable looping and set range") {
        TestClip clip;
        clip.set_looping(true);
        clip.set_loop_range(0, 480);
        REQUIRE(clip.is_looping());
        REQUIRE(clip.loop_start() == 0);
        REQUIRE(clip.loop_end() == 480);
    }

    SECTION("clip_time_at wraps within loop range") {
        TestClip clip;
        clip.set_start(0);
        clip.set_length(960);
        clip.set_looping(true);
        clip.set_loop_range(0, 480);

        // Inside loop range: direct position
        REQUIRE(clip.clip_time_at(240) == 240);
        // At loop boundary: maps to loop_start
        REQUIRE(clip.clip_time_at(480) == 0);
        // Beyond loop: wraps
        REQUIRE(clip.clip_time_at(960) == 0);
        REQUIRE(clip.clip_time_at(720) == 240);
    }

    SECTION("clip_time_at without looping returns raw offset") {
        TestClip clip;
        clip.set_start(0);
        clip.set_length(960);
        REQUIRE_FALSE(clip.is_looping());
        REQUIRE(clip.clip_time_at(500) == 500);
    }
}

TEST_CASE("Clip — name and color", "[model][clip]") {
    SECTION("default name is empty") {
        TestClip clip;
        REQUIRE(clip.name().empty());
    }

    SECTION("set name") {
        TestClip clip;
        clip.set_name("Kick verse");
        REQUIRE(clip.name() == "Kick verse");
    }

    SECTION("default color is 0") {
        TestClip clip;
        REQUIRE(clip.color() == 0);
    }

    SECTION("set color") {
        TestClip clip;
        clip.set_color(0xFF3366);
        REQUIRE(clip.color() == 0xFF3366);
    }
}
