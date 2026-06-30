#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "graphics/easing.h"
#include "graphics/animation_types.h"
#include "graphics/animator.h"
#include "graphics/widget.h"

#include <cmath>

using namespace aria;
using Catch::Matchers::WithinRel;

// ─── Easing TDD Tests ──────────────────────────────────────────────
//
// Test Strategy:
//   - Verify each easing function returns 0.0 at t=0 and 1.0 at t=1.
//   - Verify monotonic non-decreasing for in-variants across [0, 1].
//   - Verify out-back overshoots past 1.0 then returns to 1.0.
//
//   TDD notes:
//     - RED:   Tests reference easing.h before it exists.
//     - GREEN: Implement all easing functions in header.
//     - TRIANGULATE: Edge cases, callback invocation.

// =====================================================================
// RED / GREEN – Easing boundaries (t=0 → 0, t=1 → 1)
// =====================================================================

TEST_CASE("Easing: linear maps boundaries correctly",
          "[graphics][animation][easing]")
{
    // GIVEN the linear easing function
    // WHEN t = 0.0 THEN result is 0.0
    CHECK(ease_linear(0.0f) == 0.0f);
    // WHEN t = 1.0 THEN result is 1.0
    CHECK(ease_linear(1.0f) == 1.0f);
    // AND mid-point is linear
    CHECK_THAT(ease_linear(0.5f), WithinRel(0.5f, 0.001f));
}

TEST_CASE("Easing: quad maps boundaries correctly",
          "[graphics][animation][easing]")
{
    CHECK(ease_in_quad(0.0f) == 0.0f);
    CHECK(ease_in_quad(1.0f) == 1.0f);
    CHECK(ease_out_quad(0.0f) == 0.0f);
    CHECK(ease_out_quad(1.0f) == 1.0f);
    CHECK(ease_in_out_quad(0.0f) == 0.0f);
    CHECK(ease_in_out_quad(1.0f) == 1.0f);
}

TEST_CASE("Easing: cubic maps boundaries correctly",
          "[graphics][animation][easing]")
{
    CHECK(ease_in_cubic(0.0f) == 0.0f);
    CHECK(ease_in_cubic(1.0f) == 1.0f);
    CHECK(ease_out_cubic(0.0f) == 0.0f);
    CHECK(ease_out_cubic(1.0f) == 1.0f);
    CHECK(ease_in_out_cubic(0.0f) == 0.0f);
    CHECK(ease_in_out_cubic(1.0f) == 1.0f);
}

TEST_CASE("Easing: quart maps boundaries correctly",
          "[graphics][animation][easing]")
{
    CHECK(ease_in_quart(0.0f) == 0.0f);
    CHECK(ease_in_quart(1.0f) == 1.0f);
    CHECK(ease_out_quart(0.0f) == 0.0f);
    CHECK(ease_out_quart(1.0f) == 1.0f);
    CHECK(ease_in_out_quart(0.0f) == 0.0f);
    CHECK(ease_in_out_quart(1.0f) == 1.0f);
}

TEST_CASE("Easing: quint maps boundaries correctly",
          "[graphics][animation][easing]")
{
    CHECK(ease_in_quint(0.0f) == 0.0f);
    CHECK(ease_in_quint(1.0f) == 1.0f);
    CHECK(ease_out_quint(0.0f) == 0.0f);
    CHECK(ease_out_quint(1.0f) == 1.0f);
    CHECK(ease_in_out_quint(0.0f) == 0.0f);
    CHECK(ease_in_out_quint(1.0f) == 1.0f);
}

TEST_CASE("Easing: sine maps boundaries correctly",
          "[graphics][animation][easing]")
{
    CHECK(ease_in_sine(0.0f) == 0.0f);
    CHECK(ease_in_sine(1.0f) == 1.0f);
    CHECK(ease_out_sine(0.0f) == 0.0f);
    CHECK(ease_out_sine(1.0f) == 1.0f);
    CHECK(ease_in_out_sine(0.0f) == 0.0f);
    CHECK(ease_in_out_sine(1.0f) == 1.0f);
}

TEST_CASE("Easing: expo maps boundaries correctly",
          "[graphics][animation][easing]")
{
    CHECK(ease_in_expo(0.0f) == 0.0f);
    CHECK(ease_in_expo(1.0f) == 1.0f);
    CHECK(ease_out_expo(0.0f) == 0.0f);
    CHECK(ease_out_expo(1.0f) == 1.0f);
    CHECK(ease_in_out_expo(0.0f) == 0.0f);
    CHECK(ease_in_out_expo(1.0f) == 1.0f);
}

TEST_CASE("Easing: circ maps boundaries correctly",
          "[graphics][animation][easing]")
{
    CHECK(ease_in_circ(0.0f) == 0.0f);
    CHECK(ease_in_circ(1.0f) == 1.0f);
    CHECK(ease_out_circ(0.0f) == 0.0f);
    CHECK(ease_out_circ(1.0f) == 1.0f);
    CHECK(ease_in_out_circ(0.0f) == 0.0f);
    CHECK(ease_in_out_circ(1.0f) == 1.0f);
}

TEST_CASE("Easing: back maps boundaries correctly",
          "[graphics][animation][easing]")
{
    CHECK(ease_in_back(0.0f) == 0.0f);
    CHECK(ease_in_back(1.0f) == 1.0f);
    CHECK(ease_out_back(0.0f) == 0.0f);
    CHECK(ease_out_back(1.0f) == 1.0f);
    CHECK(ease_in_out_back(0.0f) == 0.0f);
    CHECK(ease_in_out_back(1.0f) == 1.0f);
}

TEST_CASE("Easing: elastic maps boundaries correctly",
          "[graphics][animation][easing]")
{
    CHECK(ease_in_elastic(0.0f) == 0.0f);
    CHECK(ease_in_elastic(1.0f) == 1.0f);
    CHECK(ease_out_elastic(0.0f) == 0.0f);
    CHECK(ease_out_elastic(1.0f) == 1.0f);
    CHECK(ease_in_out_elastic(0.0f) == 0.0f);
    CHECK(ease_in_out_elastic(1.0f) == 1.0f);
}

TEST_CASE("Easing: bounce maps boundaries correctly",
          "[graphics][animation][easing]")
{
    CHECK(ease_in_bounce(0.0f) == 0.0f);
    CHECK(ease_in_bounce(1.0f) == 1.0f);
    CHECK(ease_out_bounce(0.0f) == 0.0f);
    CHECK(ease_out_bounce(1.0f) == 1.0f);
    CHECK(ease_in_out_bounce(0.0f) == 0.0f);
    CHECK(ease_in_out_bounce(1.0f) == 1.0f);
}

// =====================================================================
// TRIANGULATE – Easing monotonicity for in-variants
// =====================================================================

TEST_CASE("Easing: ease_in_cubic is monotonic non-decreasing",
          "[graphics][animation][easing][monotonic]")
{
    // GIVEN ease_in_cubic sampled at increasing t values
    float samples[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    float prev = ease_in_cubic(samples[0]);
    for (int i = 1; i < 5; ++i) {
        float cur = ease_in_cubic(samples[i]);
        // THEN each result is >= the previous
        CHECK(cur >= prev);
        prev = cur;
    }
}

TEST_CASE("Easing: ease_out_cubic is monotonic non-decreasing",
          "[graphics][animation][easing][monotonic]")
{
    float samples[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    float prev = ease_out_cubic(samples[0]);
    for (int i = 1; i < 5; ++i) {
        float cur = ease_out_cubic(samples[i]);
        CHECK(cur >= prev);
        prev = cur;
    }
}

TEST_CASE("Easing: all in-variants are monotonic non-decreasing",
          "[graphics][animation][easing][monotonic]")
{
    // GIVEN all "in" easing functions
    // WHEN sampled at increasing points
    // THEN each is monotonic non-decreasing
    float t[] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};

    auto check_monotonic = [&](auto ease_fn) {
        float prev = ease_fn(t[0]);
        for (int i = 1; i < 6; ++i) {
            float cur = ease_fn(t[i]);
            CHECK(cur >= prev - 0.0001f);
            prev = cur;
        }
    };

    check_monotonic(ease_in_quad);
    check_monotonic(ease_in_cubic);
    check_monotonic(ease_in_quart);
    check_monotonic(ease_in_quint);
    check_monotonic(ease_in_sine);
    check_monotonic(ease_in_expo);
    check_monotonic(ease_in_circ);
    check_monotonic(ease_in_back);
}

// =====================================================================
// TRIANGULATE – Out-back overshoot
// =====================================================================

TEST_CASE("Easing: ease_out_back overshoots past 1.0",
          "[graphics][animation][easing][overshoot]")
{
    // GIVEN ease_out_back at t=1.0
    // THEN it returns 1.0
    CHECK(ease_out_back(1.0f) == 1.0f);

    // AND for some t < 1.0, value exceeds 1.0
    bool overshoots = false;
    for (float t = 0.0f; t < 1.0f; t += 0.05f) {
        if (ease_out_back(t) > 1.0f) {
            overshoots = true;
            break;
        }
    }
    CHECK(overshoots);
}

TEST_CASE("Easing: ease_in_back undershoots below 0.0",
          "[graphics][animation][easing][overshoot]")
{
    // GIVEN ease_in_back at t=0
    // THEN it returns 0.0
    CHECK(ease_in_back(0.0f) == 0.0f);

    // AND for some t > 0, value is below 0.0
    bool undershoots = false;
    for (float t = 0.05f; t < 1.0f; t += 0.05f) {
        if (ease_in_back(t) < 0.0f) {
            undershoots = true;
            break;
        }
    }
    CHECK(undershoots);
}

// =====================================================================
// RED / GREEN – Animation descriptor
// =====================================================================

TEST_CASE("Animation: default construction",
          "[graphics][animation][descriptor]")
{
    // GIVEN a default Animation
    Animation anim;

    // THEN fields have sensible defaults
    CHECK(anim.target_id == kInvalidWidgetID);
    CHECK(anim.property == AnimatableProperty::kOpacity);
    CHECK(anim.from == 0.0f);
    CHECK(anim.to == 0.0f);
    CHECK(anim.duration == 0.0f);
    CHECK(anim.easing == EasingCurve::kLinear);
    CHECK_FALSE(anim.on_complete);  // no callback
}

TEST_CASE("Animation: created with valid parameters",
          "[graphics][animation][descriptor]")
{
    // GIVEN a widget ID and target property
    WidgetID id = 42;
    bool completed = false;

    // WHEN an Animation is constructed
    Animation anim;
    anim.target_id = id;
    anim.property = AnimatableProperty::kOpacity;
    anim.from = 0.0f;
    anim.to = 1.0f;
    anim.duration = 0.3f;
    anim.easing = EasingCurve::kEaseOutCubic;
    anim.on_complete = [&] { completed = true; };

    // THEN fields match
    CHECK(anim.target_id == id);
    CHECK(anim.property == AnimatableProperty::kOpacity);
    CHECK(anim.from == 0.0f);
    CHECK(anim.to == 1.0f);
    CHECK_THAT(anim.duration, WithinRel(0.3f, 0.001f));
    CHECK(anim.easing == EasingCurve::kEaseOutCubic);
    CHECK(anim.on_complete);  // callback is callable

    // AND callback works
    anim.on_complete();
    CHECK(completed);
}

TEST_CASE("Animation: move transfers ownership",
          "[graphics][animation][descriptor]")
{
    // GIVEN an Animation with a callback
    bool called = false;
    Animation a;
    a.on_complete = [&] { called = true; };
    CHECK(a.on_complete);

    // WHEN Animation b = std::move(a)
    Animation b = std::move(a);

    // THEN b.on_complete is callable
    CHECK(b.on_complete);
    b.on_complete();
    CHECK(called);

    // AND a.on_complete is empty (moved-from state)
    CHECK_FALSE(a.on_complete);
}

// =====================================================================
// RED / GREEN – Animator lifecycle
// =====================================================================

TEST_CASE("Animator: starts with no active animations",
          "[graphics][animation][animator]")
{
    Animator animator;

    // GIVEN a fresh Animator
    // THEN no animations are active
    CHECK_FALSE(animator.is_animating(WidgetID(1)));
    CHECK(animator.active_count() == 0);
    CHECK(animator.completed().empty());
}

TEST_CASE("Animator: tick advances animation and fires completion",
          "[graphics][animation][animator]")
{
    // GIVEN an Animator with one animation of 0.5s duration
    Animator animator;
    bool completed = false;

    Animation anim;
    anim.target_id = WidgetID(1);
    anim.property = AnimatableProperty::kOpacity;
    anim.from = 0.0f;
    anim.to = 1.0f;
    anim.duration = 0.5f;
    anim.easing = EasingCurve::kLinear;
    anim.on_complete = [&] { completed = true; };

    animator.start(std::move(anim));

    // WHEN tick(0.5) is called (full duration)
    animator.tick(0.5f);

    // THEN animation completes
    CHECK(completed);
    CHECK_FALSE(animator.is_animating(WidgetID(1)));
    CHECK(animator.active_count() == 0);

    // AND completed() returns the widget ID
    auto comp = animator.completed();
    REQUIRE(comp.size() == 1);
    CHECK(comp[0] == WidgetID(1));
}

TEST_CASE("Animator: tick partial step produces interpolated value",
          "[graphics][animation][animator]")
{
    // GIVEN an Animator with from=0, to=100, duration=1.0s
    Animator animator;

    Animation anim;
    anim.target_id = WidgetID(1);
    anim.property = AnimatableProperty::kX;
    anim.from = 0.0f;
    anim.to = 100.0f;
    anim.duration = 1.0f;
    anim.easing = EasingCurve::kLinear;

    animator.start(std::move(anim));

    // WHEN tick(0.5) is called
    animator.tick(0.5f);

    // THEN value is 50.0 (linear at t=0.5)
    CHECK_THAT(animator.current_value(WidgetID(1), AnimatableProperty::kX),
               WithinRel(50.0f, 0.001f));

    // AND animation is still active
    CHECK(animator.is_animating(WidgetID(1)));
}

TEST_CASE("Animator: cancel removes all animations for a widget",
          "[graphics][animation][animator]")
{
    Animator animator;

    // GIVEN two animations for widget ID 42
    for (int i = 0; i < 2; ++i) {
        Animation anim;
        anim.target_id = WidgetID(42);
        anim.property = AnimatableProperty::kOpacity;
        anim.from = 0.0f;
        anim.to = 1.0f;
        anim.duration = 1.0f;
        anim.easing = EasingCurve::kLinear;
        animator.start(std::move(anim));
    }

    CHECK(animator.active_count() == 2);

    // WHEN cancel(42) is called
    animator.cancel(WidgetID(42));

    // THEN no animations remain for that widget
    CHECK_FALSE(animator.is_animating(WidgetID(42)));
    CHECK(animator.active_count() == 0);
}

TEST_CASE("Animator: cancel only affects specified widget",
          "[graphics][animation][animator]")
{
    Animator animator;

    // GIVEN animations for two different widgets
    for (auto id : {WidgetID(1), WidgetID(2)}) {
        Animation anim;
        anim.target_id = id;
        anim.property = AnimatableProperty::kOpacity;
        anim.from = 0.0f;
        anim.to = 1.0f;
        anim.duration = 1.0f;
        anim.easing = EasingCurve::kLinear;
        animator.start(std::move(anim));
    }

    CHECK(animator.active_count() == 2);

    // WHEN cancel(1) is called
    animator.cancel(WidgetID(1));

    // THEN widget 2 is still animating
    CHECK_FALSE(animator.is_animating(WidgetID(1)));
    CHECK(animator.is_animating(WidgetID(2)));
    CHECK(animator.active_count() == 1);
}

TEST_CASE("Animator: tick with zero dt is a no-op",
          "[graphics][animation][animator]")
{
    // GIVEN an Animator with one active animation
    Animator animator;
    bool completed = false;

    Animation anim;
    anim.target_id = WidgetID(1);
    anim.property = AnimatableProperty::kOpacity;
    anim.from = 0.0f;
    anim.to = 1.0f;
    anim.duration = 1.0f;
    anim.easing = EasingCurve::kLinear;
    anim.on_complete = [&] { completed = true; };

    animator.start(std::move(anim));

    // WHEN tick(0.0) is called
    animator.tick(0.0f);

    // THEN animation remains at t=0
    CHECK_FALSE(completed);
    CHECK(animator.is_animating(WidgetID(1)));

    // AND current value equals 'from'
    CHECK_THAT(animator.current_value(WidgetID(1), AnimatableProperty::kOpacity),
               WithinRel(0.0f, 0.001f));
}

TEST_CASE("Animator: tick with dt exceeding duration completes immediately",
          "[graphics][animation][animator]")
{
    Animator animator;
    bool completed = false;

    Animation anim;
    anim.target_id = WidgetID(1);
    anim.property = AnimatableProperty::kOpacity;
    anim.from = 0.0f;
    anim.to = 1.0f;
    anim.duration = 0.5f;
    anim.easing = EasingCurve::kLinear;
    anim.on_complete = [&] { completed = true; };

    animator.start(std::move(anim));

    // WHEN tick(2.0) exceeds the 0.5s duration
    animator.tick(2.0f);

    // THEN animation completes and value == 'to'
    CHECK(completed);
    CHECK_FALSE(animator.is_animating(WidgetID(1)));
    CHECK_THAT(animator.current_value(WidgetID(1), AnimatableProperty::kOpacity),
               WithinRel(1.0f, 0.001f));
}

TEST_CASE("Animator: completed set is consumed after tick",
          "[graphics][animation][animator]")
{
    Animator animator;

    // GIVEN a short animation
    {
        Animation anim;
        anim.target_id = WidgetID(1);
        anim.property = AnimatableProperty::kOpacity;
        anim.from = 0.0f;
        anim.to = 1.0f;
        anim.duration = 0.1f;
        anim.easing = EasingCurve::kLinear;
        animator.start(std::move(anim));
    }

    // WHEN tick completes the animation
    animator.tick(0.5f);

    // THEN completed() reports the widget
    CHECK(animator.completed().size() == 1);

    // Tick again with no active animations
    animator.tick(0.5f);

    // THEN completed is empty (consumed)
    CHECK(animator.completed().empty());
}

// =====================================================================
// TRIANGULATE – Animator with EasingCurve enum mapping
// =====================================================================

TEST_CASE("Animator: easing curve is applied during interpolation",
          "[graphics][animation][animator]")
{
    // GIVEN two animations with different easing curves, same params
    Animator linear_anim;
    Animator cubic_anim;

    {
        Animation a;
        a.target_id = WidgetID(1);
        a.property = AnimatableProperty::kOpacity;
        a.from = 0.0f;
        a.to = 1.0f;
        a.duration = 1.0f;
        a.easing = EasingCurve::kLinear;
        linear_anim.start(std::move(a));
    }
    {
        Animation a;
        a.target_id = WidgetID(2);
        a.property = AnimatableProperty::kOpacity;
        a.from = 0.0f;
        a.to = 1.0f;
        a.duration = 1.0f;
        a.easing = EasingCurve::kEaseInCubic;
        cubic_anim.start(std::move(a));
    }

    // WHEN both advance by 0.5s
    linear_anim.tick(0.5f);
    cubic_anim.tick(0.5f);

    // THEN linear = 0.5, cubic < 0.5 (cubic is slower at t=0.5)
    float linear_val = linear_anim.current_value(WidgetID(1), AnimatableProperty::kOpacity);
    float cubic_val = cubic_anim.current_value(WidgetID(2), AnimatableProperty::kOpacity);

    CHECK_THAT(linear_val, WithinRel(0.5f, 0.001f));
    CHECK(cubic_val < 0.5f);
    CHECK(cubic_val > 0.0f);
}

// =====================================================================
// RED / GREEN – Animator::tick with multiple simultaneous completions
// =====================================================================

TEST_CASE("Animator: multiple completions all reported",
          "[graphics][animation][animator]")
{
    Animator animator;

    // GIVEN three widgets with short animations
    for (auto id : {WidgetID(10), WidgetID(20), WidgetID(30)}) {
        Animation anim;
        anim.target_id = id;
        anim.property = AnimatableProperty::kOpacity;
        anim.from = 0.0f;
        anim.to = 1.0f;
        anim.duration = 0.1f;
        anim.easing = EasingCurve::kLinear;
        animator.start(std::move(anim));
    }

    // WHEN tick completes all three
    animator.tick(1.0f);

    // THEN all three IDs are reported
    auto comp = animator.completed();
    CHECK(comp.size() == 3);
}

// =====================================================================
// RED / GREEN – Widget animated properties
// =====================================================================

TEST_CASE("Widget: animated properties default to unset",
          "[graphics][animation][widget]")
{
    // GIVEN a concrete widget
    // (use a bare widget with an inline subclass for testing)
    struct TestWidget : Widget {
        const char* type_name() const noexcept override { return "TestWidget"; }
        Vec2 measure(float, float) override { return {100, 100}; }
        void render(SkiaCanvas*) override {}
    };

    TestWidget widget;

    // THEN animated_value returns 0 for any property
    // (default state before set_animated_property is called)
    CHECK(widget.animated_value(AnimatableProperty::kOpacity) == 0.0f);
    CHECK(widget.animated_value(AnimatableProperty::kX) == 0.0f);
    CHECK(widget.animated_value(AnimatableProperty::kRotation) == 0.0f);
}

TEST_CASE("Widget: set_animated_property stores and retrieves values",
          "[graphics][animation][widget]")
{
    struct TestWidget : Widget {
        const char* type_name() const noexcept override { return "TestWidget"; }
        Vec2 measure(float, float) override { return {100, 100}; }
        void render(SkiaCanvas*) override {}
    };

    TestWidget widget;

    // WHEN animated properties are set
    widget.set_animated_property(AnimatableProperty::kOpacity, 0.5f);
    widget.set_animated_property(AnimatableProperty::kX, 42.0f);
    widget.set_animated_property(AnimatableProperty::kRotation, 180.0f);

    // THEN they can be retrieved
    CHECK_THAT(widget.animated_value(AnimatableProperty::kOpacity), WithinRel(0.5f, 0.001f));
    CHECK_THAT(widget.animated_value(AnimatableProperty::kX), WithinRel(42.0f, 0.001f));
    CHECK_THAT(widget.animated_value(AnimatableProperty::kRotation), WithinRel(180.0f, 0.001f));
}

// =====================================================================
// RED / GREEN – set_animator() interface on RenderLoop
// =====================================================================

TEST_CASE("RenderLoop: set_animator compiles and accepts nullptr",
          "[graphics][animation][integration]")
{
    RenderLoop loop;

    // GIVEN a RenderLoop
    // WHEN set_animator is called with nullptr
    // THEN no crash (default state is clean)
    CHECK_NOTHROW(loop.set_animator(nullptr));
}

TEST_CASE("RenderLoop: set_animator accepts a valid Animator pointer",
          "[graphics][animation][integration]")
{
    RenderLoop loop;
    Animator animator;

    // GIVEN a valid Animator
    // WHEN set_animator is called
    // THEN the setter compiles and works
    CHECK_NOTHROW(loop.set_animator(&animator));

    // AND can be cleared back to nullptr
    CHECK_NOTHROW(loop.set_animator(nullptr));
}

// =====================================================================
// RED / GREEN – Widget::find_by_id tree traversal
// =====================================================================

TEST_CASE("Widget: find_by_id returns matching widget",
          "[graphics][animation][widget]")
{
    struct TestWidget : Widget {
        const char* type_name() const noexcept override { return "TestWidget"; }
        Vec2 measure(float, float) override { return {100, 100}; }
        void render(SkiaCanvas*) override {}
    };

    auto root = std::make_unique<TestWidget>();
    root->set_id(WidgetID(1));
    auto* root_ptr = root.get();

    auto child = std::make_unique<TestWidget>();
    child->set_id(WidgetID(2));
    auto* child_ptr = child.get();
    root_ptr->add_child(std::move(child));

    // WHEN find_by_id is called
    // THEN it finds the child
    CHECK(root_ptr->find_by_id(WidgetID(2)) == child_ptr);
    CHECK(root_ptr->find_by_id(WidgetID(1)) == root_ptr);

    // AND returns nullptr for unknown ID
    CHECK(root_ptr->find_by_id(WidgetID(999)) == nullptr);
}
