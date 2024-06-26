//   SPDX-License-Identifier: GPL-2.0-or-later
//

#include "color-wheel-factory.h"
#include "ink-color-wheel.h"
#include "oklab-color-wheel.h"

namespace Inkscape::UI::Widget {

static std::pair<ColorWheel*, bool> create_color_wheel_helper(Colors::Space::Type type, bool create_widget) {
    bool can_create = true;
    ColorWheel* wheel = nullptr;

    switch (type) {
    case Colors::Space::Type::HSL:
        if (create_widget) wheel = new ColorWheelHSL();
        break;

    case Colors::Space::Type::HSLUV:
        if (create_widget) wheel = new ColorWheelHSLuv();
        break;

    case Colors::Space::Type::OKHSL:
        if (create_widget) wheel = new OKWheel();
        break;

    default:
        can_create = false;
        break;
    }

    return std::make_pair(wheel, can_create);
}

ColorWheel* create_managed_color_wheel(Colors::Space::Type type) {
    auto [wheel, _] = create_color_wheel_helper(type, true);
    if (wheel) {
        wheel->set_manage();
    }
    return wheel;
}

bool can_create_color_wheel(Colors::Space::Type type) {
    auto [_, ok] = create_color_wheel_helper(type, false);
    return ok;
}

} // Inkscape
