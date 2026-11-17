#include "View/SliderWithLabel.h"

#include <QLabel>
#include <QSlider>

#include <catch2/catch.hpp>

namespace TrenchBroom::View {
TEST_CASE("Deferred sliders preview positions without repeatedly applying expensive settings", "[SliderWithLabel]") {
    SliderWithLabel control{-50, 50};
    control.setValue(0);
    control.setTracking(false);
    auto* slider = control.findChild<QSlider*>();
    auto* label = control.findChild<QLabel*>();
    REQUIRE(slider);
    REQUIRE(label);
    int commits = 0;
    QObject::connect(&control, &SliderWithLabel::valueChanged, [&] { ++commits; });
    slider->setSliderDown(true);
    for (auto position = 1; position <= 20; ++position) {
        slider->setSliderPosition(position);
        CHECK(control.value() == 0);
        CHECK(label->text() == QString::number(position));
        CHECK(commits == 0);
    }
    slider->setSliderDown(false);
    CHECK(control.value() == 20);
    CHECK(commits == 1);
    CHECK(label->text() == "20");
}

TEST_CASE("Ordinary sliders retain immediate value tracking", "[SliderWithLabel]") {
    SliderWithLabel control{0, 100};
    auto* slider = control.findChild<QSlider*>();
    REQUIRE(slider);
    int commits = 0;
    QObject::connect(&control, &SliderWithLabel::valueChanged, [&] { ++commits; });
    slider->setSliderDown(true);
    slider->setSliderPosition(42);
    CHECK(control.value() == 42);
    CHECK(commits == 1);
    slider->setSliderDown(false);
    CHECK(commits == 1);
}
} // namespace TrenchBroom::View
