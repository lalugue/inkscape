// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_DISPLAY_CONTROL_CTRL_HANDLE_STYLING_H
#define INKSCAPE_DISPLAY_CONTROL_CTRL_HANDLE_STYLING_H
/**
 * Classes related to control handle styling.
 */
/*
 * Authors:
 *   Sanidhya Singh
 *
 * Copyright (C) 2023 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <compare>
#include <cstdint>
#include <functional> // std::hash
#include <tuple>

#include "canvas-item-enums.h"

namespace Inkscape::Handles {

/**
 * Struct to manage type and state.
 */
struct TypeState
{
    CanvasItemCtrlType type = CANVAS_ITEM_CTRL_TYPE_DEFAULT;
    bool selected = false;
    bool hover = false;
    bool click = false;

    auto operator<=>(TypeState const &) const = default;
};

/**
 * Template struct for properties with specificities.
 */
template <typename T>
class Property
{
public:
    explicit Property(T val) : value(val) {}
    Property(T val, int spec) : value(val), specificity(spec) {}

    /*
     * Set value of property based on specificity.
     */
    void setProperty(T newValue, int newSpecificity)
    {
        if (newSpecificity >= specificity) {
            value = newValue;
            specificity = newSpecificity;
        }
    }

    /*
     * Interface to get the value.
     */
    T const &operator()() const
    {
        return value;
    }

private:
    T value;
    int specificity = 0;
};

/**
 * Struct containing all required styling for handles.
 */
struct Style
{
    Property<CanvasItemCtrlShape> shape{CANVAS_ITEM_CTRL_SHAPE_SQUARE};
    Property<uint32_t> fill{0xffffff};
    Property<uint32_t> stroke{0xffffff};
    Property<uint32_t> outline{0xffffff};
    Property<float> fill_opacity{1.0};
    Property<float> stroke_opacity{1.0};
    Property<float> outline_opacity{1.0};
    Property<float> opacity{1.0};
    Property<int> stroke_width{1};
    Property<int> outline_width{0};

    uint32_t getFill() const;
    uint32_t getStroke() const;
    uint32_t getOutline() const;
};

void ensure_styles_parsed();
Style const &lookup_style(TypeState const &handle);

} // namespace Inkscape::Handles

template <> struct std::hash<Inkscape::Handles::TypeState>
{
    size_t operator()(Inkscape::Handles::TypeState const &handle) const
    {
        return (size_t{handle.type} << 3) |
               (size_t{handle.selected} << 2) |
               (size_t{handle.hover} << 1) |
               (size_t{handle.click});
    }
};

#endif // INKSCAPE_DISPLAY_CONTROL_CTRL_HANDLE_STYLING_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
