// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_CANVAS_ITEM_CTRL_H
#define SEEN_CANVAS_ITEM_CTRL_H

/**
 * A class to represent a control node.
 */

/*
 * Authors:
 *   Tavmjong Bah
 *   Sanidhya Singh
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * Rewrite of SPCtrl
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <mutex>
#include <memory>
#include <2geom/point.h>
#include <boost/functional/hash.hpp>

#include "canvas-item.h"
#include "canvas-item-enums.h"

#include "enums.h" // SP_ANCHOR_X
#include "display/initlock.h"
#include "display/cairo-utils.h" // argb32_from_rgba()

namespace Inkscape {

/**
 * Struct to manage state and type.
 */
struct Handle {
    CanvasItemCtrlType _type;
    bool _selected = false;
    bool _click = false;
    bool _hover = false;

    Handle() : _type(CANVAS_ITEM_CTRL_TYPE_DEFAULT) {}
    Handle(CanvasItemCtrlType type) : _type(type) {}
    Handle(CanvasItemCtrlType type, bool selected, bool hover, bool click) : _type(type)
    {
        setState(selected, hover, click);
    }

    void setType(CanvasItemCtrlType type)
    {
        _type = type;
    }
    void setState(bool selected, bool hover, bool click)
    {
        setSelected(selected);
        setHover(hover);
        setClick(click);
    }
    void setSelected(bool selected = true)
    {
        _selected = selected;
    }
    bool isSelected() const
    {
        return _selected;
    }
    void setHover(bool hover = true)
    {
        _hover = hover;
    }
    bool isHover() const
    {
        return _hover;
    }
    void setClick(bool click = true)
    {
        _click = click;
    }
    bool isClick() const
    {
        return _click;
    }
    bool operator==(const Handle &other) const
    {
        return (_type == other._type && _selected == other._selected &&
                _hover == other._hover && _click == other._click);
    }
    bool operator!=(const Handle &other) const
    {
        return !(*this == other);
    }
    static bool fits(const Handle &selector, const Handle &handle)
    {
        // type must match for non-default selectors
        if(selector._type != CANVAS_ITEM_CTRL_TYPE_DEFAULT && selector._type != handle._type) {
            return false;
        }
        // any state set in selector must be set in handle
        return !((selector.isSelected() && !handle.isSelected()) ||
                 (selector.isHover() && !handle.isHover()) ||
                 (selector.isClick() && !handle.isClick()));
    }
};

class CanvasItemCtrl : public CanvasItem {
public:
    CanvasItemCtrl(CanvasItemGroup *group);
    CanvasItemCtrl(CanvasItemGroup *group, CanvasItemCtrlType type);
    CanvasItemCtrl(CanvasItemGroup *group, CanvasItemCtrlType type, Geom::Point const &p);

    // Geometry
    void set_position(Geom::Point const &position);

    double closest_distance_to(Geom::Point const &p) const;

    // Selection
    bool contains(Geom::Point const &p, double tolerance = 0) override;

    // Properties
    void set_mode(CanvasItemCtrlMode mode);
    void set_size(int size, bool manual = true);
    void set_fill(uint32_t rgba) override;
    void set_stroke(uint32_t rgba) override;
    void set_shape(CanvasItemCtrlShape shape);
    virtual void set_size_via_index(int size_index);
    void set_size_default(); // Use preference and type to set size.
    void set_size_extra(int extra); // Used to temporary increase size of ctrl.
    void set_anchor(SPAnchorType anchor);
    void set_angle(double angle);
    void set_type(CanvasItemCtrlType type);
    void set_selected(bool selected = true);
    void set_click(bool click = true);
    void set_hover(bool hover = true);
    void set_normal(bool selected = false);
    static void draw_shape(std::shared_ptr<uint32_t[]> cache,
        CanvasItemCtrlShape shape, uint32_t fill, uint32_t stroke, uint32_t outline,
        int stroke_width, int outline_width,
        int width, double angle, int device_scale);

protected:
    ~CanvasItemCtrl() override = default;

    void _update(bool propagate) override;
    void _render(Inkscape::CanvasItemBuffer &buf) const override;

    void build_cache(int device_scale) const;
    void parse_handle_styles() const;

    // Geometry
    Geom::Point _position;

    // Display
    InitLock _built;
    mutable std::shared_ptr<uint32_t[]> _cache;

    // Properties
    Handle _handle = Handle();
    CanvasItemCtrlMode  _mode  = CANVAS_ITEM_CTRL_MODE_NORMAL;
    CanvasItemCtrlShape _shape = CANVAS_ITEM_CTRL_SHAPE_SQUARE;
    uint32_t _fill = 0x000000ff;
    uint32_t _stroke = 0xffffffff;
    bool _shape_set = false;
    bool _fill_set = false;
    bool _stroke_set = false;
    bool _size_set = false;
    int _width  = 5;
    int _extra  = 0; // Used to temporarily increase size.
    double _angle = 0; // Used for triangles, could be used for arrows.
    SPAnchorType _anchor = SP_ANCHOR_CENTER;
};

/**
 * Template struct for properties with specificities.
 */
template <typename T>
struct Property {
private:
    T value;
    int specificity = 0;

public:
    Property(T val) : value(val) {}
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
    const T& operator()()
    {
        return value;
    }
};

/**
 * Struct containing all required styling for handles.
 */
struct HandleStyle {
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

    uint32_t getFill()
    {
        EXTRACT_ARGB32((fill()), a, r, g, b)
        a = int(fill_opacity() * opacity() * 255);
        ASSEMBLE_ARGB32(fill_color, a, r, g, b)
        return rgba_from_argb32(fill_color);
    }

    uint32_t getStroke()
    {
        EXTRACT_ARGB32(stroke(), stroke_a, stroke_r, stroke_g, stroke_b)
        EXTRACT_ARGB32(fill(), fill_a, fill_r, fill_g, fill_b)
        float fill_af = fill_opacity();
        float stroke_af = stroke_opacity();
        float result_af = stroke_af + fill_af * (1 - stroke_af);
        if (result_af == 0) {
            return 0;
        }

        uint8_t result_r = int((stroke_r * stroke_af + fill_r * fill_af * (1 - stroke_af)) / result_af);
        uint8_t result_g = int((stroke_g * stroke_af + fill_g * fill_af * (1 - stroke_af)) / result_af);
        uint8_t result_b = int((stroke_b * stroke_af + fill_b * fill_af * (1 - stroke_af)) / result_af);
        ASSEMBLE_ARGB32(blend, (int(opacity()*result_af * 255)), result_r, result_g, result_b)
        return rgba_from_argb32(blend);
    }

    uint32_t getOutline()
    {
        EXTRACT_ARGB32((outline()), a, r, g, b)
        a = int(outline_opacity() * opacity() * 255);
        ASSEMBLE_ARGB32(outline_color, a, r, g, b)
        return rgba_from_argb32(outline_color);
    }
};

} // namespace Inkscape

#endif // SEEN_CANVAS_ITEM_CTRL_H

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
