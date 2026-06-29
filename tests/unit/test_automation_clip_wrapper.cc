#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "model/automation_clip_wrapper.h"
#include "automation/automation_clip.h"

#include <cstdint>

using namespace aria;

TEST_CASE("AutomationClipWrapper — construction", "[model][automation][wrapper]") {
    SECTION("wraps an AutomationClip") {
        automation::AutomationClip inner;
        AutomationClipWrapper wrapper(inner);

        // Wrapper is a Clip
        Clip* base = &wrapper;
        REQUIRE(base != nullptr);
    }

    SECTION("wrapper has a valid ClipID") {
        automation::AutomationClip inner;
        AutomationClipWrapper wrapper(inner);
        REQUIRE(wrapper.id().value > 0);
    }
}

TEST_CASE("AutomationClipWrapper — delegates to AutomationClip", "[model][automation][wrapper]") {
    automation::AutomationClip inner;
    inner.set_length(3840);
    inner.set_name("Volume Automation");

    AutomationClipWrapper wrapper(inner);

    SECTION("length delegates to inner clip") {
        REQUIRE(wrapper.length() == 3840);
    }

    SECTION("name delegates to inner clip") {
        REQUIRE(wrapper.name() == "Volume Automation");
    }

    SECTION("set_length updates inner clip") {
        wrapper.set_length(1920);
        REQUIRE(inner.length() == 1920);
        REQUIRE(wrapper.length() == 1920);
    }

    SECTION("set_name updates inner clip") {
        wrapper.set_name("Pan Automation");
        REQUIRE(inner.name() == "Pan Automation");
        REQUIRE(wrapper.name() == "Pan Automation");
    }
}

TEST_CASE("AutomationClipWrapper — independent properties", "[model][automation][wrapper]") {
    automation::AutomationClip inner;
    AutomationClipWrapper wrapper(inner);

    SECTION("position is owned by wrapper") {
        wrapper.set_start(960);
        REQUIRE(wrapper.start() == 960);
        // length comes from inner AutomationClip (default 15360)
        REQUIRE(wrapper.end() == wrapper.start() + wrapper.length());
    }

    SECTION("fade data is owned by wrapper") {
        wrapper.set_fade_in(48);
        wrapper.set_fade_in_shape(FadeShape::LinearIn);
        REQUIRE(wrapper.fade_in() == 48);
        REQUIRE(wrapper.fade_in_shape() == FadeShape::LinearIn);
    }

    SECTION("gain is owned by wrapper") {
        wrapper.set_gain(0.75);
        REQUIRE(wrapper.gain() == Catch::Approx(0.75));
    }

    SECTION("mute is owned by wrapper") {
        wrapper.set_muted(true);
        REQUIRE(wrapper.is_muted());
    }

    SECTION("color is owned by wrapper") {
        wrapper.set_color(0xFF3366);
        REQUIRE(wrapper.color() == 0xFF3366);
    }
}

TEST_CASE("AutomationClipWrapper — evaluate delegates to inner clip", "[model][automation][wrapper]") {
    automation::AutomationClip inner;
    inner.set_length(960);
    {
        automation::AutomationPoint pt;
        pt.ppqn = 0;
        pt.value = 0.0;
        pt.interpolation = automation::InterpolationType::Linear;
        inner.add_point(pt);
    }
    {
        automation::AutomationPoint pt;
        pt.ppqn = 960;
        pt.value = 1.0;
        pt.interpolation = automation::InterpolationType::Linear;
        inner.add_point(pt);
    }

    AutomationClipWrapper wrapper(inner);

    SECTION("evaluate delegates to inner clip") {
        REQUIRE(wrapper.evaluate(0) == Catch::Approx(0.0));
        REQUIRE(wrapper.evaluate(480) == Catch::Approx(0.5));
        REQUIRE(wrapper.evaluate(960) == Catch::Approx(1.0));
    }
}
