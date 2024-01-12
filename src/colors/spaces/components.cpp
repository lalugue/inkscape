// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Meta data about color channels and how they are presented to users.
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2023 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "components.h"

#include <algorithm>
#include <cmath>
#include <libintl.h> // avoid glib include
#include <map>
#include <utility>

#include "enum.h"

#define _(String) gettext(String)

namespace Inkscape::Colors::Space {

Component::Component(Type type, unsigned int index, std::string id, std::string name, std::string tip, unsigned scale)
    : type(type)
    , index(index)
    , id(std::move(id))
    , name(std::move(name))
    , tip(std::move(tip))
    , scale(scale)
{}

/**
 * Clamp the value to between 0.0 and 1.0, except for hue which is wrapped around.
 */
double Component::normalize(double value) const
{
    if (scale == 360 && (value < 0.0 || value > 1.0)) {
        return value - std::floor(value);
    }
    return std::clamp(value, 0.0, 1.0);
}

void Components::add(std::string id, std::string name, std::string tip, unsigned scale)
{
    _components.emplace_back(_type, _components.size(), std::move(id), std::move(name), std::move(tip), scale);
}

std::map<Type, Components> _build(bool alpha)
{
    std::map<Type, Components> sets;

    sets[Type::RGB].setType(Type::RGB);
    sets[Type::RGB].add("r", _("_R:"), _("Red"), 255);
    sets[Type::RGB].add("g", _("_G:"), _("Green"), 255);
    sets[Type::RGB].add("b", _("_B:"), _("Blue"), 255);

    sets[Type::linearRGB].setType(Type::linearRGB);
    sets[Type::linearRGB].add("r", _("<sub>l</sub>_R:"), _("Linear Red"), 255);
    sets[Type::linearRGB].add("g", _("<sub>l</sub>_G:"), _("Linear Green"), 255);
    sets[Type::linearRGB].add("b", _("<sub>l</sub>_B:"), _("Linear Blue"), 255);

    sets[Type::HSL].setType(Type::HSL);
    sets[Type::HSL].add("h", _("_H:"), _("Hue"), 360);
    sets[Type::HSL].add("s", _("_S:"), _("Saturation"), 100);
    sets[Type::HSL].add("l", _("_L:"), _("Lightness"), 100);

    sets[Type::HSL].setType(Type::HSL);
    sets[Type::CMYK].add("c", _("_C:"), _("Cyan"), 100);
    sets[Type::CMYK].add("m", _("_M:"), _("Magenta"), 100);
    sets[Type::CMYK].add("y", _("_Y:"), _("Yellow"), 100);
    sets[Type::CMYK].add("k", _("_K:"), _("Black"), 100);

    sets[Type::CMY].setType(Type::CMY);
    sets[Type::CMY].add("c", _("_C:"), _("Cyan"), 100);
    sets[Type::CMY].add("m", _("_M:"), _("Magenta"), 100);
    sets[Type::CMY].add("y", _("_Y:"), _("Yellow"), 100);

    sets[Type::HSV].setType(Type::HSV);
    sets[Type::HSV].add("h", _("_H:"), _("Hue"), 360);
    sets[Type::HSV].add("s", _("_S:"), _("Saturation"), 100);
    sets[Type::HSV].add("v", _("_V:"), _("Value"), 100);

    sets[Type::HSLUV].setType(Type::HSLUV);
    sets[Type::HSLUV].add("h", _("_H*"), _("Hue"), 360);
    sets[Type::HSLUV].add("s", _("_S*"), _("Saturation"), 100);
    sets[Type::HSLUV].add("l", _("_L*"), _("Lightness"), 100);

    sets[Type::LUV].setType(Type::LUV);
    sets[Type::LUV].add("l", _("_L*"), _("Luminance"), 100);
    sets[Type::LUV].add("u", _("_u*"), _("Chroma U"), 100);
    sets[Type::LUV].add("v", _("_v*"), _("Chroma V"), 100);

    sets[Type::LCH].setType(Type::LCH);
    sets[Type::LCH].add("l", _("_L"), _("Luminance"), 255);
    sets[Type::LCH].add("c", _("_C"), _("Chroma"), 255);
    sets[Type::LCH].add("h", _("_H"), _("Hue"), 360);

    sets[Type::OKHSL].setType(Type::OKHSL);
    sets[Type::OKHSL].add("h", _("_H<sub>ok</sub>"), _("Hue"), 360);
    sets[Type::OKHSL].add("s", _("_S<sub>ok</sub>"), _("Saturation"), 100);
    sets[Type::OKHSL].add("l", _("_L<sub>ok</sub>"), _("Lightness"), 100);

    sets[Type::OKLAB].setType(Type::OKLAB);
    sets[Type::OKLAB].add("h", _("_L<sub>ok</sub>"), _("Lightness"), 100);
    sets[Type::OKLAB].add("s", _("_A<sub>ok</sub>"), _("Component A"), 100);
    sets[Type::OKLAB].add("l", _("_B<sub>ok</sub>"), _("Component B"), 100);

    sets[Type::OKLCH].setType(Type::OKLCH);
    sets[Type::OKLCH].add("l", _("_L<sub>ok</sub>"), _("Lightness"), 100);
    sets[Type::OKLCH].add("c", _("_C<sub>ok</sub>"), _("Chroma"), 100);
    sets[Type::OKLCH].add("h", _("_H<sub>ok</sub>"), _("Hue"), 360);

    sets[Type::XYZ].setType(Type::XYZ);
    sets[Type::XYZ].add("x", "_X", "X", 255);
    sets[Type::XYZ].add("y", "_Y", "Y", 100);
    sets[Type::XYZ].add("z", "_Z", "Z", 255);

    sets[Type::YCbCr].setType(Type::YCbCr);
    sets[Type::YCbCr].add("y", "_Y", "Y", 255);
    sets[Type::YCbCr].add("cb", "C_b", "Cb", 255);
    sets[Type::YCbCr].add("cr", "C_r", "Cr", 255);

    sets[Type::LAB].setType(Type::LAB);
    sets[Type::LAB].add("l", "_L", "L", 100);
    sets[Type::LAB].add("a", "_a", "a", 256);
    sets[Type::LAB].add("b", "_b", "b", 256);

    sets[Type::YXY].setType(Type::YXY);
    sets[Type::YXY].add("y1", "_Y", "Y", 255);
    sets[Type::YXY].add("x", "_x", "x", 255);
    sets[Type::YXY].add("y2", "y", "y", 255);

    sets[Type::Gray].setType(Type::Gray);
    sets[Type::Gray].add("gray", _("G:"), _("Gray"), 1024);

    if (alpha) {
        for (auto &[key, val] : sets) {
            val.add("a", _("_A:"), _("Alpha"), 255);
        }
    }
    return sets;
}

Components const &Components::get(Type space, bool alpha)
{
    static std::map<Type, Components> sets_no_alpha = _build(false);
    static std::map<Type, Components> sets_with_alpha = _build(true);

    auto &lookup_set = alpha ? sets_with_alpha : sets_no_alpha;
    if (auto search = lookup_set.find(space); search != lookup_set.end()) {
        return search->second;
    }
    return lookup_set[Type::NONE];
}

}; // namespace Inkscape::Colors::Space
