// SPDX-License-Identifier: GPL-2.0-or-later
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

#include <2geom/transforms.h>
#include <glibmm/regex.h>

#include "3rdparty/libcroco/src/cr-selector.h"
#include "3rdparty/libcroco/src/cr-doc-handler.h"
#include "3rdparty/libcroco/src/cr-string.h"
#include "3rdparty/libcroco/src/cr-term.h"
#include "3rdparty/libcroco/src/cr-parser.h"
#include "3rdparty/libcroco/src/cr-rgb.h"
#include "3rdparty/libcroco/src/cr-utils.h"

#include "canvas-item-ctrl.h"
#include "helper/geom.h"

#include "io/resource.h"
#include "io/sys.h"

#include "preferences.h" // Default size. 
#include "display/cairo-utils.h" // 32bit color handling.

#include "ui/widget/canvas.h"

namespace std {
template <>
struct hash<Inkscape::Handle> {
    size_t operator()(Inkscape::Handle const &handle) const
    {
        uint32_t typeandstate = (handle._type << 3) | (handle.isSelected() << 2) | (handle.isHover() << 1) | (handle.isClick());
        return hash<uint32_t>()(typeandstate);
    }
};
template <>
struct hash<std::tuple<Inkscape::Handle, int, double>> {
    size_t operator()(const std::tuple<Inkscape::Handle, int, double>& key) const {
        size_t hashValue = hash<Inkscape::Handle>{}(std::get<0>(key));
        boost::hash_combine(hashValue, std::get<1>(key));
        boost::hash_combine(hashValue, std::get<2>(key));
        return hashValue;
    }
};
} // namespace std

namespace Inkscape {

/** 
 * For Handle Styling (shared between all handles)
 */
std::unordered_map<Handle, HandleStyle *> handle_styles;
std::unordered_map<std::tuple<Handle, int, double>, std::shared_ptr<uint32_t[]>> handle_cache;
std::mutex cache_mutex;
InitLock _parsed;

/**
 * Create a null control node.
 */
CanvasItemCtrl::CanvasItemCtrl(CanvasItemGroup *group)
    : CanvasItem(group)
{
    _name = "CanvasItemCtrl:Null";
    _pickable = true; // Everybody gets events from this class!
}

/**
 * Create a control with type.
 */
CanvasItemCtrl::CanvasItemCtrl(CanvasItemGroup *group, CanvasItemCtrlType type)
    : CanvasItem(group)
    , _handle(type)
{
    _name = "CanvasItemCtrl:Type_" + std::to_string(_handle._type);
    _pickable = true; // Everybody gets events from this class!
    set_size_default();
}

/**
 * Create a control ctrl. Point is in document coordinates.
 */
CanvasItemCtrl::CanvasItemCtrl(CanvasItemGroup *group, CanvasItemCtrlType type, Geom::Point const &p)
    : CanvasItemCtrl(group, type)
{
    _position = p;
    request_update();
}

/**
 * Set the position. Point is in document coordinates.
 */
void CanvasItemCtrl::set_position(Geom::Point const &position)
{
    defer([=, this] {
        if (_position == position) return;
        _position = position;
        request_update();
    });
}

/**
 * Returns distance between point in canvas units and position of ctrl.
 */
double CanvasItemCtrl::closest_distance_to(Geom::Point const &p) const
{
    // TODO: Different criteria for different shapes.
    return Geom::distance(p, _position * affine());
}

/**
 * If tolerance is zero, returns true if point p (in canvas units) is inside bounding box,
 * else returns true if p (in canvas units) is within tolerance (canvas units) distance of ctrl.
 * The latter assumes ctrl center anchored.
 */
bool CanvasItemCtrl::contains(Geom::Point const &p, double tolerance)
{
    // TODO: Different criteria for different shapes.
    if (!_bounds) {
        return false;
    }
    if (tolerance == 0) {
        return _bounds->interiorContains(p);
    } else {
        return closest_distance_to(p) <= tolerance;
    }
}

static auto angle_of(Geom::Affine const &affine)
{
    return std::atan2(affine[1], affine[0]);
}

void CanvasItemCtrl::set_fill(uint32_t fill)
{
    defer([=, this] {
        _fill_set = true;
        if (_fill == fill) return;
        _fill = fill;
        _built.reset();
        request_redraw();
    });
}

void CanvasItemCtrl::set_stroke(uint32_t stroke)
{
    defer([=, this] {
        _stroke_set = true;
        if (_stroke == stroke) return;
        _stroke = stroke;
        _built.reset();
        request_redraw();
    });
}

void CanvasItemCtrl::set_shape(CanvasItemCtrlShape shape)
{
    defer([=, this] {
        _shape_set = true;
        if (_shape == shape) return;
        _shape = shape;
        _built.reset();
        request_update(); // Geometry could change
    });
}

void CanvasItemCtrl::set_mode(CanvasItemCtrlMode mode)
{
    defer([=, this] {
        if (_mode == mode) return;
        _mode = mode;
        _built.reset();
        request_update();
    });
}

void CanvasItemCtrl::set_size(int size, bool manual)
{
    defer([=, this] {
        _size_set = manual;
        if (_width == size + _extra) return;
        _width  = size + _extra;
        _built.reset();
        request_update(); // Geometry change
    });
}

void CanvasItemCtrl::set_size_via_index(int size_index)
{
    // If size has been set manually in the code, the handles shouldn't be affected.
    if (_size_set) {
        return;
    }
    // Size must always be an odd number to center on pixel.
    if (size_index < 1 || size_index > 15) {
        std::cerr << "CanvasItemCtrl::set_size_via_index: size_index out of range!" << std::endl;
        size_index = 3;
    }

    int size = 0;
    switch (_handle._type) {
    case CANVAS_ITEM_CTRL_TYPE_ADJ_HANDLE:
    case CANVAS_ITEM_CTRL_TYPE_ADJ_SKEW:
        size = size_index * 2 + 7;
        break;

    case CANVAS_ITEM_CTRL_TYPE_ADJ_ROTATE:
    case CANVAS_ITEM_CTRL_TYPE_ADJ_CENTER:
        size = size_index * 2 + 9; // 2 larger than HANDLE/SKEW
        break;

    case CANVAS_ITEM_CTRL_TYPE_ADJ_SALIGN:
    case CANVAS_ITEM_CTRL_TYPE_ADJ_CALIGN:
    case CANVAS_ITEM_CTRL_TYPE_ADJ_MALIGN:
        size = size_index * 4 + 5; // Needs to be larger to allow for rotating.
        break;

    case CANVAS_ITEM_CTRL_TYPE_ROTATE:
    case CANVAS_ITEM_CTRL_TYPE_MARGIN:
    case CANVAS_ITEM_CTRL_TYPE_CENTER:
    case CANVAS_ITEM_CTRL_TYPE_SIZER:
    case CANVAS_ITEM_CTRL_TYPE_SHAPER:
    case CANVAS_ITEM_CTRL_TYPE_MARKER:
    case CANVAS_ITEM_CTRL_TYPE_MESH:
    case CANVAS_ITEM_CTRL_TYPE_LPE:
    case CANVAS_ITEM_CTRL_TYPE_NODE_AUTO:
    case CANVAS_ITEM_CTRL_TYPE_NODE_CUSP:
        size = size_index * 2 + 5;
        break;

    case CANVAS_ITEM_CTRL_TYPE_NODE_SMOOTH:
    case CANVAS_ITEM_CTRL_TYPE_NODE_SYMETRICAL:
        size = size_index * 2 + 3;
        break;

    case CANVAS_ITEM_CTRL_TYPE_INVISIPOINT:
        size = 1;
        break;

    case CANVAS_ITEM_CTRL_TYPE_GUIDE_HANDLE:
        size = size_index | 1; // "or" with 1 will make it odd if even
        break;

    case CANVAS_ITEM_CTRL_TYPE_ANCHOR: // vanishing point for 3D box and anchor for pencil
    case CANVAS_ITEM_CTRL_TYPE_POINT:
    case CANVAS_ITEM_CTRL_TYPE_DEFAULT:
        size = size_index * 2 + 1;
        break;

    default:
        g_warning("set_size_via_index: missing case for handle type: %d", static_cast<int>(_handle._type));
        size = size_index * 2 + 1;
        break;
    }

    set_size(size, false);
}

void CanvasItemCtrl::set_size_default()
{
    int size = Preferences::get()->getIntLimited("/options/grabsize/value", 3, 1, 15);
    set_size_via_index(size);
}

void CanvasItemCtrl::set_size_extra(int extra)
{
    defer([=, this] {
        _width  += extra - _extra;
        _extra = extra;
        _built.reset();
        request_update(); // Geometry change
    });
}

void CanvasItemCtrl::set_type(CanvasItemCtrlType type)
{
    defer([=, this] {
        if (_handle._type == type) return;
        _handle.setType(type);
        set_size_default();
        _built.reset();
        request_update(); // Possible geometry change
    });
}

void CanvasItemCtrl::set_selected(bool selected)
{
    _handle.setSelected(selected);
    _built.reset();
    request_update();
}
void CanvasItemCtrl::set_click(bool click)
{
    _handle.setClick(click);
    _built.reset();
    request_update();
}
void CanvasItemCtrl::set_hover(bool hover)
{
    _handle.setHover(hover);
    _built.reset();
    request_update();
}

/**
 * Reset the state to normal or normal selected
 */
void CanvasItemCtrl::set_normal(bool selected)
{
    _handle.setSelected(selected);
    _handle.setHover(false);
    _handle.setClick(false);
    _built.reset();
    request_update();
}

void CanvasItemCtrl::set_angle(double angle)
{
    defer([=, this] {
        if (_angle == angle) return;
        _angle = angle;
        _built.reset();
        request_update(); // Geometry change
    });
}

void CanvasItemCtrl::set_anchor(SPAnchorType anchor)
{
    defer([=, this] {
        if (_anchor == anchor) return;
        _anchor = anchor;
        request_update(); // Geometry change
    });
}

static void draw_darrow(Cairo::RefPtr<Cairo::Context> const &cr, double size, double offset_x, double offset_y)
{
    // Find points, starting from tip of one arrowhead, working clockwise.
    /*   1        4
        ╱│        │╲
       ╱ └────────┘ ╲
     0╱  2        3  ╲5
      ╲  8        7  ╱
       ╲ ┌────────┐ ╱
        ╲│9      6│╱
    */

    // Length of arrowhead (not including stroke).
    double delta = (size - 1) / 4.0; // Use unscaled width.

    // Tip of arrow (0)
    double tip_x = 0.5;          // At edge, allow room for stroke.
    double tip_y = size / 2.0;   // Center

    // Outer corner (1)
    double out_x = tip_x + delta;
    double out_y = tip_y - delta;

    // Inner corner (2)
    double in_x = out_x;
    double in_y = out_y + (delta / 2.0);

    double x0 = tip_x;
    double y0 = tip_y;
    double x1 = out_x;
    double y1 = out_y;
    double x2 = in_x;
    double y2 = in_y;
    double x3 = size - in_x;
    double y3 = in_y;
    double x4 = size - out_x;
    double y4 = out_y;
    double x5 = size - tip_x;
    double y5 = tip_y;
    double x6 = size - out_x;
    double y6 = size - out_y;
    double x7 = size - in_x;
    double y7 = size - in_y;
    double x8 = in_x;
    double y8 = size - in_y;
    double x9 = out_x;
    double y9 = size - out_y;

    // Draw arrow
    cr->move_to(offset_x + x0, offset_y + y0);
    cr->line_to(offset_x + x1, offset_y + y1);
    cr->line_to(offset_x + x2, offset_y + y2);
    cr->line_to(offset_x + x3, offset_y + y3);
    cr->line_to(offset_x + x4, offset_y + y4);
    cr->line_to(offset_x + x5, offset_y + y5);
    cr->line_to(offset_x + x6, offset_y + y6);
    cr->line_to(offset_x + x7, offset_y + y7);
    cr->line_to(offset_x + x8, offset_y + y8);
    cr->line_to(offset_x + x9, offset_y + y9);
    cr->close_path();
}

static void draw_carrow(Cairo::RefPtr<Cairo::Context> const &cr, double size, double offset_x, double offset_y)
{
    // Length of arrowhead (not including stroke).
    double delta = (size - 3) / 4.0; // Use unscaled width.

    // Tip of arrow
    double tip_x =         1.5;  // Edge, allow room for stroke when rotated.
    double tip_y = delta + 1.5;

    // Outer corner (1)
    double out_x = tip_x + delta;
    double out_y = tip_y - delta;

    // Inner corner (2)
    double in_x = out_x;
    double in_y = out_y + (delta / 2.0);

    double x0 = tip_x;
    double y0 = tip_y;
    double x1 = out_x;
    double y1 = out_y;
    double x2 = in_x;
    double y2 = in_y;
    double x3 = size - in_y;        //double y3 = size - in_x;
    double x4 = size - out_y;
    double y4 = size - out_x;
    double x5 = size - tip_y;
    double y5 = size - tip_x;
    double x6 = x5 - delta;
    double y6 = y4;
    double x7 = x5 - delta / 2.0;
    double y7 = y4;
    double x8 = x1;                 //double y8 = y0 + delta/2.0;
    double x9 = x1;
    double y9 = y0 + delta;

    // Draw arrow
    cr->move_to(offset_x + x0, offset_y + y0);
    cr->line_to(offset_x + x1, offset_y + y1);
    cr->line_to(offset_x + x2, offset_y + y2);
    cr->arc(offset_x + x1, offset_y + y4, x3 - x2, 3.0 * M_PI / 2.0, 0);
    cr->line_to(offset_x + x4, offset_y + y4);
    cr->line_to(offset_x + x5, offset_y + y5);
    cr->line_to(offset_x + x6, offset_y + y6);
    cr->line_to(offset_x + x7, offset_y + y7);
    cr->arc_negative(offset_x + x1, offset_y + y4, x7 - x8, 0, 3.0 * M_PI / 2.0);
    cr->line_to(offset_x + x9, offset_y + y9);
    cr->close_path();
}

static void draw_triangle(Cairo::RefPtr<Cairo::Context> const &cr, double size, double offset_x, double offset_y)
{
    // Construct an arrowhead (triangle)
    double s = size / 2.0;
    double wcos = s * cos(M_PI / 6);
    double hsin = s * sin(M_PI / 6);
    // Construct a smaller arrow head for fill.
    Geom::Point p1f(1, s);
    Geom::Point p2f(s + wcos - 1, s + hsin);
    Geom::Point p3f(s + wcos - 1, s - hsin);
    // Draw arrow
    cr->move_to(offset_x + p1f[0], offset_y + p1f[1]);
    cr->line_to(offset_x + p2f[0], offset_y + p2f[1]);
    cr->line_to(offset_x + p3f[0], offset_y + p3f[1]);
    cr->close_path();
}

static void draw_triangle_angled(Cairo::RefPtr<Cairo::Context> const &cr, double size, double offset_x, double offset_y)
{
    // Construct an arrowhead (triangle) of half size.
    double s = size / 2.0;
    double wcos = s * cos(M_PI / 9);
    double hsin = s * sin(M_PI / 9);
    Geom::Point p1f(s + 1, s);
    Geom::Point p2f(s + wcos - 1, s + hsin - 1);
    Geom::Point p3f(s + wcos - 1, s - (hsin - 1));
    // Draw arrow
    cr->move_to(offset_x + p1f[0], offset_y + p1f[1]);
    cr->line_to(offset_x + p2f[0], offset_y + p2f[1]);
    cr->line_to(offset_x + p3f[0], offset_y + p3f[1]);
    cr->close_path();
}

static void draw_pivot(Cairo::RefPtr<Cairo::Context> const &cr, double size, double offset_x, double offset_y)
{
    double delta4 = (size - 5) / 4.0; // Keep away from edge or will clip when rotating.
    double delta8 = delta4 / 2;

    // Line start
    double center = size / 2.0;

    cr->move_to(offset_x + center - delta8, offset_y + center - 2 * delta4 - delta8);
    cr->rel_line_to(delta4,  0);
    cr->rel_line_to(0,       delta4);

    cr->rel_line_to(delta4,  delta4);

    cr->rel_line_to(delta4,  0);
    cr->rel_line_to(0,       delta4);
    cr->rel_line_to(-delta4,  0);

    cr->rel_line_to(-delta4,  delta4);

    cr->rel_line_to(0,       delta4);
    cr->rel_line_to(-delta4,  0);
    cr->rel_line_to(0,      -delta4);

    cr->rel_line_to(-delta4, -delta4);

    cr->rel_line_to(-delta4,  0);
    cr->rel_line_to(0,      -delta4);
    cr->rel_line_to(delta4,  0);

    cr->rel_line_to(delta4, -delta4);
    cr->close_path();

    cr->begin_new_sub_path();
    cr->arc_negative(offset_x + center, offset_y + center, delta4, 0, -2 * M_PI);
}

static void draw_salign(Cairo::RefPtr<Cairo::Context> const &cr, double size, double offset_x, double offset_y)
{
    // Triangle pointing at line.

    // Basic units.
    double delta4 = (size - 1) / 4.0; // Use unscaled width.
    double delta8 = delta4 / 2;
    if (delta8 < 2) {
        // Keep a minimum gap of at least one pixel (after stroking).
        delta8 = 2;
    }

    // Tip of triangle
    double tip_x = size / 2.0; // Center (also rotation point).
    double tip_y = size / 2.0;

    // Corner triangle position.
    double outer = size / 2.0 - delta4;

    // Outer line position
    double oline = size / 2.0 + (int)delta4;

    // Inner line position
    double iline = size / 2.0 + (int)delta8;

    // Draw triangle
    cr->move_to(offset_x + tip_x,        offset_y + tip_y);
    cr->line_to(offset_x + outer,        offset_y + outer);
    cr->line_to(offset_x + size - outer, offset_y + outer);
    cr->close_path();

    // Draw line
    cr->move_to(offset_x + outer,        offset_y + iline);
    cr->line_to(offset_x + size - outer, offset_y + iline);
    cr->line_to(offset_x + size - outer, offset_y + oline);
    cr->line_to(offset_x + outer,        offset_y + oline);
    cr->close_path();
}

static void draw_calign(Cairo::RefPtr<Cairo::Context> const &cr, double size, double offset_x, double offset_y)
{
    // Basic units.
    double delta4 = (size - 1) / 4.0; // Use unscaled width.
    double delta8 = delta4 / 2;
    if (delta8 < 2) {
        // Keep a minimum gap of at least one pixel (after stroking).
        delta8 = 2;
    }

    // Tip of triangle
    double tip_x = size / 2.0; // Center (also rotation point).
    double tip_y = size / 2.0;

    // Corner triangle position.
    double outer = size / 2.0 - delta8 - delta4;

    // End of line positin
    double eline = size / 2.0 - delta8;

    // Outer line position
    double oline = size / 2.0 + (int)delta4;

    // Inner line position
    double iline = size / 2.0 + (int)delta8;

    // Draw triangle
    cr->move_to(offset_x + tip_x, offset_y + tip_y);
    cr->line_to(offset_x + outer, offset_y + tip_y);
    cr->line_to(offset_x + tip_x, offset_y + outer);
    cr->close_path();

    // Draw line
    cr->move_to(offset_x + iline, offset_y + iline);
    cr->line_to(offset_x + iline, offset_y + eline);
    cr->line_to(offset_x + oline, offset_y + eline);
    cr->line_to(offset_x + oline, offset_y + oline);
    cr->line_to(offset_x + eline, offset_y + oline);
    cr->line_to(offset_x + eline, offset_y + iline);
    cr->close_path();
}

static void draw_malign(Cairo::RefPtr<Cairo::Context> const &cr, double size, double offset_x, double offset_y)
{
    // Basic units.
    double delta4 = (size - 1) / 4.0; // Use unscaled width.
    double delta8 = delta4 / 2;
    if (delta8 < 2) {
        // Keep a minimum gap of at least one pixel (after stroking).
        delta8 = 2;
    }

    // Tip of triangle
    double tip_0 = size / 2.0;
    double tip_1 = size / 2.0 - delta8;

    // Draw triangles
    cr->move_to(offset_x + tip_0,           offset_y + tip_1);
    cr->line_to(offset_x + tip_0 - delta4,  offset_y + tip_1 - delta4);
    cr->line_to(offset_x + tip_0 + delta4,  offset_y + tip_1 - delta4);
    cr->close_path();

    cr->move_to(offset_x + size - tip_1,           offset_y + tip_0);
    cr->line_to(offset_x + size - tip_1 + delta4,  offset_y + tip_0 - delta4);
    cr->line_to(offset_x + size - tip_1 + delta4,  offset_y + tip_0 + delta4);
    cr->close_path();

    cr->move_to(offset_x + size - tip_0,           offset_y + size - tip_1);
    cr->line_to(offset_x + size - tip_0 + delta4,  offset_y + size - tip_1 + delta4);
    cr->line_to(offset_x + size - tip_0 - delta4,  offset_y + size - tip_1 + delta4);
    cr->close_path();

    cr->move_to(offset_x + tip_1,           offset_y + tip_0);
    cr->line_to(offset_x + tip_1 - delta4,  offset_y + tip_0 + delta4);
    cr->line_to(offset_x + tip_1 - delta4,  offset_y + tip_0 - delta4);
    cr->close_path();
}

static void draw_cairo_path(CanvasItemCtrlShape shape, Cairo::RefPtr<Cairo::Context> const &cr, 
                            double size, double offset_x, double offset_y)
{
    switch (shape) {
    case CANVAS_ITEM_CTRL_SHAPE_DARROW:
    case CANVAS_ITEM_CTRL_SHAPE_SARROW:
        draw_darrow(cr, size, offset_x, offset_y);
        break;

    case CANVAS_ITEM_CTRL_SHAPE_TRIANGLE:
        draw_triangle(cr, size, offset_x, offset_y);
        break;

    case CANVAS_ITEM_CTRL_SHAPE_TRIANGLE_ANGLED:
        draw_triangle_angled(cr, size, offset_x, offset_y);
        break;

    case CANVAS_ITEM_CTRL_SHAPE_CARROW:
        draw_carrow(cr, size, offset_x, offset_y);
        break;

    case CANVAS_ITEM_CTRL_SHAPE_PIVOT:
        draw_pivot(cr, size, offset_x, offset_y);
        break;

    case CANVAS_ITEM_CTRL_SHAPE_SALIGN:
        draw_salign(cr, size, offset_x, offset_y);
        break;

    case CANVAS_ITEM_CTRL_SHAPE_CALIGN:
        draw_calign(cr, size, offset_x, offset_y);
        break;

    case CANVAS_ITEM_CTRL_SHAPE_MALIGN:
        draw_malign(cr, size, offset_x, offset_y);
        break;

    default:
        // Shouldn't happen
        break;
    }
}

/**
 * Update and redraw control ctrl.
 */
void CanvasItemCtrl::_update(bool)
{
    // Queue redraw of old area (erase previous content).
    request_redraw();

    // Setting the position to (inf, inf) to hide it is a pervasive hack we need to support.
    if (!_position.isFinite()) {
        _bounds = {};
        return;
    }

    // Width is always odd.
    assert(_width % 2 == 1);

    // Get half width and , rounded down.
    int const w_half = _width / 2;

    // Set _angle, and compute adjustment for anchor.
    int dx = 0;
    int dy = 0;

    _parsed.init([ &, this] {
        parse_handle_styles();
    });

    CanvasItemCtrlShape shape;
    if(!_shape_set && handle_styles.find(_handle) != handle_styles.end()) {
        shape = handle_styles[_handle]->shape();
    } else {
        shape = _shape;
    }

    switch (shape) {
    case CANVAS_ITEM_CTRL_SHAPE_DARROW:
    case CANVAS_ITEM_CTRL_SHAPE_SARROW:
    case CANVAS_ITEM_CTRL_SHAPE_CARROW:
    case CANVAS_ITEM_CTRL_SHAPE_SALIGN:
    case CANVAS_ITEM_CTRL_SHAPE_CALIGN: {
        double angle = int{_anchor} * M_PI_4 + angle_of(affine());
        double const half = _width / 2.0;

        dx = -(half + 2) * cos(angle); // Add a bit to prevent tip from overlapping due to rounding errors.
        dy = -(half + 2) * sin(angle);

        switch (shape) {
        case CANVAS_ITEM_CTRL_SHAPE_CARROW:
            angle += 5 * M_PI_4;
            break;

        case CANVAS_ITEM_CTRL_SHAPE_SARROW:
            angle += M_PI_2;
            break;

        case CANVAS_ITEM_CTRL_SHAPE_SALIGN:
            dx = -(half / 2 + 2) * cos(angle);
            dy = -(half / 2 + 2) * sin(angle);
            angle -= M_PI_2;
            break;

        case CANVAS_ITEM_CTRL_SHAPE_CALIGN:
            angle -= M_PI_4;
            dx = (half / 2 + 2) * (sin(angle) - cos(angle));
            dy = (half / 2 + 2) * (-sin(angle) - cos(angle));
            break;

        default:
            break;
        }

        if (_angle != angle) {
            _angle = angle;
            _built.reset();
        }

        break;
    }

    case CANVAS_ITEM_CTRL_SHAPE_PIVOT:
    case CANVAS_ITEM_CTRL_SHAPE_MALIGN: {
        double const angle = angle_of(affine());
        if (_angle != angle) {
            _angle = angle;
            _built.reset();
        }
        break;
    }

    default:
        switch (_anchor) {
        case SP_ANCHOR_N:
        case SP_ANCHOR_CENTER:
        case SP_ANCHOR_S:
            break;

        case SP_ANCHOR_NW:
        case SP_ANCHOR_W:
        case SP_ANCHOR_SW:
            dx = w_half;
            break;

        case SP_ANCHOR_NE:
        case SP_ANCHOR_E:
        case SP_ANCHOR_SE:
            dx = -w_half;
            break;
        }

        switch (_anchor) {
        case SP_ANCHOR_W:
        case SP_ANCHOR_CENTER:
        case SP_ANCHOR_E:
            break;

        case SP_ANCHOR_NW:
        case SP_ANCHOR_N:
        case SP_ANCHOR_NE:
            dy = w_half;
            break;

        case SP_ANCHOR_SW:
        case SP_ANCHOR_S:
        case SP_ANCHOR_SE:
            dy = -w_half;
            break;
        }
        break;
    }

    auto const pt = Geom::IntPoint(-w_half, -w_half) + Geom::IntPoint(dx, dy) + (_position * affine()).floor();
    _bounds = Geom::IntRect(pt, pt + Geom::IntPoint(_width, _width));

    // Queue redraw of new area
    request_redraw();
}

static inline uint32_t compose_xor(uint32_t bg, uint32_t fg, uint32_t a)
{
    uint32_t c = bg * (255 - a) + (((bg ^ ~fg) + (bg >> 2) - (bg > 127 ? 63 : 0)) & 255) * a;
    return (c + 127) / 255;
}

/**
 * Render ctrl to screen via Cairo.
 */
void CanvasItemCtrl::_render(CanvasItemBuffer &buf) const
{
    _parsed.init([ &, this] {
        parse_handle_styles();
    });

    _built.init([ &, this] {
        build_cache(buf.device_scale);
    });

    Geom::Point point = (_bounds->min() - buf.rect.min());
    int x = point.x(); // Must be pixel aligned.
    int y = point.y();

    buf.cr->save();
    // This code works regardless of source type.

    // 1. Copy the affected part of output to a temporary surface

    // Size in device pixels. Does not set device scale.
    int width  = _width  * buf.device_scale;
    auto work = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, width, width);
    cairo_surface_set_device_scale(work->cobj(), buf.device_scale, buf.device_scale); // No C++ API!       

    auto cr = Cairo::Context::create(work);
    cr->translate(-_bounds->left(), -_bounds->top());
    cr->set_source(buf.cr->get_target(), buf.rect.left(), buf.rect.top());
    cr->paint();
    // static int a = 0;
    // std::string name0 = "ctrl0_" + _name + "_" + std::to_string(a++) + ".png";
    // work->write_to_png(name0);

    // 2. Composite the control on a temporary surface
    work->flush();
    int stride = work->get_stride() / 4; // divided by 4 to covert from bytes to 1 pixel (32 bits)
    auto row_ptr = reinterpret_cast<uint32_t *>(work->get_data());

    // Turn pixel position back into desktop coords for page or desk color
    auto px2dt = Geom::Scale(buf.device_scale).inverse()
               * Geom::Translate(_bounds->min())
               * affine().inverse();
    bool use_bg = !get_canvas()->background_in_stores() || buf.outline_pass;

    uint32_t *handle_ptr = _cache.get();
    for (int i = 0; i < width; ++i) {
        for (int j = 0; j < width; ++j) {
            uint32_t base = row_ptr[j];
            uint32_t handle_px = *handle_ptr++;
            uint32_t handle_op = handle_px & 0xff;
            uint32_t canvas_color = 0x00;
            if (use_bg) {
                // this code allow background become isolated from rendering so we can do things like outline overlay
                canvas_color = get_canvas()->get_effective_background(Geom::Point(j, i) * px2dt);
            }
            if(_mode != CANVAS_ITEM_CTRL_MODE_NORMAL) {
                if (base == 0 && handle_px != 0) {
                    base = canvas_color;
                }
                if (handle_op == 0 && handle_px != 0) {
                    row_ptr[j] = argb32_from_rgba(handle_px | 0x000000ff);
                    continue;
                } else if (handle_op == 0) {
                    row_ptr[j] = base;
                    continue;
                }
            }
            switch(_mode) {
            case CANVAS_ITEM_CTRL_MODE_NORMAL: {
                EXTRACT_ARGB32(base, base_a, base_r, base_g, base_b)
                uint32_t handle_argb = (handle_px>>8) | (handle_px<<24); 
                EXTRACT_ARGB32(handle_argb, handle_a, handle_r, handle_g, handle_b)
                float handle_af = handle_a/255.0f;
                float base_af = base_a/255.0f;
                float result_af = handle_af + base_af * (1-handle_af);
                if(result_af == 0) {
                    row_ptr[j] = 0;
                    continue;
                }
                uint32_t result_r = (handle_r * handle_af + base_r * base_af * (1-handle_af)) / result_af;
                uint32_t result_g = (handle_g * handle_af + base_g * base_af * (1-handle_af)) / result_af;
                uint32_t result_b = (handle_b * handle_af + base_b * base_af * (1-handle_af)) / result_af;
                ASSEMBLE_ARGB32(result, int(result_af*255), result_r, result_g, result_b)
                row_ptr[j] = result;
                break;
            }
            case CANVAS_ITEM_CTRL_MODE_COLOR:
                row_ptr[j] = argb32_from_rgba(handle_px | 0x000000ff);
                break;
            // TODO: Remove the other methods entirely if normal is accepted
            // (They are not used anywhere)
            case CANVAS_ITEM_CTRL_MODE_XOR:
            case CANVAS_ITEM_CTRL_MODE_GRAYSCALED_XOR:
            case CANVAS_ITEM_CTRL_MODE_DESATURATED_XOR: {
                // if color to draw has opacity, we overide base colors flattening base colors
                EXTRACT_ARGB32(base, base_a, base_r, base_g, base_b)
                EXTRACT_ARGB32(canvas_color, canvas_a, canvas_r, canvas_g, canvas_b)
                if (canvas_a != base_a) {
                base_r = (base_a / 255.0) * base_r + (1 - (base_a / 255.0)) * canvas_r;
                base_g = (base_a / 255.0) * base_g + (1 - (base_a / 255.0)) * canvas_g;
                base_b = (base_a / 255.0) * base_b + (1 - (base_a / 255.0)) * canvas_b;
                base_a = 255;
                }
                uint32_t result_r = compose_xor(base_r, (handle_px & 0xff000000) >> 24, handle_op);
                uint32_t result_g = compose_xor(base_g, (handle_px & 0x00ff0000) >> 16, handle_op);
                uint32_t result_b = compose_xor(base_b, (handle_px & 0x0000ff00) >>  8, handle_op);
                switch(_mode) {
                case CANVAS_ITEM_CTRL_MODE_GRAYSCALED_XOR: {
                    uint32_t gray = result_r * 0.299 + result_g * 0.587 + result_b * 0.114;
                    result_r = gray;
                    result_g = gray;
                    result_b = gray;
                    break;
                }
                case CANVAS_ITEM_CTRL_MODE_DESATURATED_XOR: {
                    double f = 0.85; // desaturate by 15%
                    double p = sqrt(result_r * result_r * 0.299 + result_g * result_g *  0.587 + result_b * result_b * 0.114);
                    result_r = p + (result_r - p) * f;
                    result_g = p + (result_g - p) * f;
                    result_b = p + (result_b - p) * f;
                    break;
                }
                }
                ASSEMBLE_ARGB32(result, base_a, result_r, result_g, result_b)
                row_ptr[j] = result;
                break;
            }
            }
        }
        // move the row pointer to the next row
        row_ptr += stride;
    }
    work->mark_dirty();

    // 3. Replace the affected part of output with contents of temporary surface
    buf.cr->set_source(work, x, y);

    buf.cr->rectangle(x, y, _width, _width);
    buf.cr->clip();
    buf.cr->set_operator(Cairo::OPERATOR_SOURCE);
    buf.cr->paint();
    buf.cr->restore();
}

/**
 * Conversion maps for ctrl types (CSS parsing).
 */
const std::unordered_map<std::string, CanvasItemCtrlType> ctrl_type_map = {
    {"*", CANVAS_ITEM_CTRL_TYPE_DEFAULT},
    {".inkscape-adj-handle", CANVAS_ITEM_CTRL_TYPE_ADJ_HANDLE},
    {".inkscape-adj-skew", CANVAS_ITEM_CTRL_TYPE_ADJ_SKEW},
    {".inkscape-adj-rotate", CANVAS_ITEM_CTRL_TYPE_ADJ_ROTATE},
    {".inkscape-adj-center", CANVAS_ITEM_CTRL_TYPE_ADJ_CENTER},
    {".inkscape-adj-salign", CANVAS_ITEM_CTRL_TYPE_ADJ_SALIGN},
    {".inkscape-adj-calign", CANVAS_ITEM_CTRL_TYPE_ADJ_CALIGN},
    {".inkscape-adj-malign", CANVAS_ITEM_CTRL_TYPE_ADJ_MALIGN},
    {".inkscape-anchor", CANVAS_ITEM_CTRL_TYPE_ANCHOR},
    {".inkscape-point", CANVAS_ITEM_CTRL_TYPE_POINT},
    {".inkscape-rotate", CANVAS_ITEM_CTRL_TYPE_ROTATE},
    {".inkscape-margin", CANVAS_ITEM_CTRL_TYPE_MARGIN},
    {".inkscape-center", CANVAS_ITEM_CTRL_TYPE_CENTER},
    {".inkscape-sizer", CANVAS_ITEM_CTRL_TYPE_SIZER},
    {".inkscape-shaper", CANVAS_ITEM_CTRL_TYPE_SHAPER},
    {".inkscape-marker", CANVAS_ITEM_CTRL_TYPE_MARKER},
    {".inkscape-lpe", CANVAS_ITEM_CTRL_TYPE_LPE},
    {".inkscape-node-auto", CANVAS_ITEM_CTRL_TYPE_NODE_AUTO},
    {".inkscape-node-cusp", CANVAS_ITEM_CTRL_TYPE_NODE_CUSP},
    {".inkscape-node-smooth", CANVAS_ITEM_CTRL_TYPE_NODE_SMOOTH},
    {".inkscape-node-symmetrical", CANVAS_ITEM_CTRL_TYPE_NODE_SYMETRICAL},
    {".inkscape-mesh", CANVAS_ITEM_CTRL_TYPE_MESH},
    {".inkscape-invisible", CANVAS_ITEM_CTRL_TYPE_INVISIPOINT}
};

/**
 * Conversion maps for ctrl shapes (CSS parsing).
 */
const std::unordered_map<std::string, CanvasItemCtrlShape> ctrl_shape_map = {
    {"\'square\'", CANVAS_ITEM_CTRL_SHAPE_SQUARE},
    {"\'diamond\'", CANVAS_ITEM_CTRL_SHAPE_DIAMOND},
    {"\'circle\'", CANVAS_ITEM_CTRL_SHAPE_CIRCLE},
    {"\'triangle\'", CANVAS_ITEM_CTRL_SHAPE_TRIANGLE},
    {"\'triangle-angled\'", CANVAS_ITEM_CTRL_SHAPE_TRIANGLE_ANGLED},
    {"\'cross\'", CANVAS_ITEM_CTRL_SHAPE_CROSS},
    {"\'plus\'", CANVAS_ITEM_CTRL_SHAPE_PLUS},
    {"\'plus\'", CANVAS_ITEM_CTRL_SHAPE_PLUS},
    {"\'pivot\'", CANVAS_ITEM_CTRL_SHAPE_PIVOT},
    {"\'arrow\'", CANVAS_ITEM_CTRL_SHAPE_DARROW},
    {"\'skew-arrow\'", CANVAS_ITEM_CTRL_SHAPE_SARROW},
    {"\'curved-arrow\'", CANVAS_ITEM_CTRL_SHAPE_CARROW},
    {"\'side-align\'", CANVAS_ITEM_CTRL_SHAPE_SALIGN},
    {"\'corner-align\'", CANVAS_ITEM_CTRL_SHAPE_CALIGN},
    {"\'middle-align\'", CANVAS_ITEM_CTRL_SHAPE_MALIGN}
};

/**
 * A global vector needed for parsing (between functions).
 */
std::vector<std::pair<HandleStyle *, int>> selected_handles;

/**
 * Parses the CSS selector for handles.
 */
void configure_selector(CRSelector *a_selector, Handle *&selector, int &specificity)
{
    cr_simple_sel_compute_specificity(a_selector->simple_sel);
    specificity =  a_selector->simple_sel->specificity;
    const char *selector_str = reinterpret_cast<const char *>(cr_simple_sel_one_to_string(a_selector->simple_sel));
    std::vector<std::string> tokens = Glib::Regex::split_simple(":", selector_str);
    CanvasItemCtrlType type;
    int token_iterator = 0;
    if (ctrl_type_map.find(tokens[token_iterator]) != ctrl_type_map.end()) {
        type = ctrl_type_map.at(tokens[token_iterator]);
        token_iterator++;
    } else {
        std::cerr << "Unrecognized/unhandled selector:" << selector_str << std::endl;
        selector = NULL;
        return;
    }
    selector = new Handle(type);
    for (; token_iterator < tokens.size(); token_iterator++) {
        if (tokens[token_iterator] == "*") {
            continue;
        } else if (tokens[token_iterator] == "selected") {
            selector->setSelected(true);
        } else if (tokens[token_iterator] == "hover") {
            specificity++;
            selector->setHover(true);
        } else if (tokens[token_iterator] == "click") {
            specificity++;
            selector->setClick(true);
        } else {
            std::cerr << "Unrecognized/unhandled selector:" << selector_str << std::endl;
            selector = NULL;
            return;
        }
    }
}

/**
 * Selects fitting handles from all handles based on selector.
 */
void set_selectors(CRDocHandler *a_handler, CRSelector *a_selector, bool is_users)
{
    while (a_selector) {
        Handle *selector;
        int specificity;
        configure_selector(a_selector, selector, specificity);
        if (selector) {
            for (const auto& [handle, style] : handle_styles) {
                if (Handle::fits(*selector, handle)) {
                    selected_handles.emplace_back(style, specificity + 10000 * is_users);
                }
            }
        }
        a_selector = a_selector->next;
        delete selector;
    }
}

/**
 * Parse user's style definition css.
 */
void set_selectors_user(CRDocHandler *a_handler, CRSelector *a_selector)
{
    set_selectors(a_handler, a_selector, true);
}

/**
 * Parse the default style definition css.
 */
void set_selectors_base(CRDocHandler *a_handler, CRSelector *a_selector)
{
    set_selectors(a_handler, a_selector, false);
}

/**
 * Parse and set the properties for selected handles.
 */
void set_properties(CRDocHandler *a_handler, CRString *a_name, CRTerm *a_value, gboolean a_important)
{
    auto gvalue = cr_term_to_string(a_value);
    auto gproperty = cr_string_peek_raw_str(a_name);
    if (!gvalue || !gproperty) {
        std::cerr << "Empty or improper value or property, skipped." << std::endl;
        return;
    }
    const std::string value = (char *)gvalue;
    const std::string property = gproperty;
    g_free(gvalue);
    if (property == "shape") {
        if (ctrl_shape_map.find(value) != ctrl_shape_map.end()) {
            for (auto& [handle, specificity] : selected_handles) {
                handle->shape.setProperty(ctrl_shape_map.at(value), specificity + 100000 * a_important);
            }
        } else {
            std::cerr << "Unrecognized value for " << property << ": " << value << std::endl;
            return;
        }
    } else if (property == "fill" || property == "stroke" || property == "outline") {
        CRRgb *rgb = cr_rgb_new();
        CRStatus status = cr_rgb_set_from_term(rgb, a_value);

        if (status == CR_OK) {
            ASSEMBLE_ARGB32(color, 255, (uint8_t)rgb->red, (uint8_t)rgb->green, (uint8_t)rgb->blue)
            for (auto& [handle, specificity] : selected_handles) {
                if (property == "fill") {
                    handle->fill.setProperty(color, specificity + 100000 * a_important);
                } else if(property == "stroke") {
                    handle->stroke.setProperty(color, specificity + 100000 * a_important);
                } else {// outline
                    handle->outline.setProperty(color, specificity + 100000 * a_important);
                }
            }
        } else {
            std::cerr << "Unrecognized value for " << property << ": " << value << std::endl;
            return;
        }
    } else if (property == "opacity" || property == "fill-opacity" ||
               property == "stroke-opacity" || property == "outline-opacity") {
        if (!a_value->content.num) {
            std::cerr << "Invalid value for " << property << ": " << value << std::endl;
            return;
        }

        double val;
        if (a_value->content.num->type == NUM_PERCENTAGE) {
            val = (a_value->content.num->val) / 100.0f;
        } else if (a_value->content.num->type == NUM_GENERIC) {
            val = a_value->content.num->val;
        } else {
            std::cerr << "Invalid type for " << property << ": " << value << std::endl;
            return;
        }

        if (val > 1 || val < 0) {
            std::cerr << "Invalid value for " << property << ": " << value << std::endl;
            return;
        }
        for (auto& [handle, specificity] : selected_handles) {
            if (property == "opacity") {
                handle->opacity.setProperty(val, specificity + 100000 * a_important);
            } else if (property == "fill-opacity") {
                handle->fill_opacity.setProperty(val, specificity + 100000 * a_important);
            } else if (property == "stroke-opacity") {
                handle->stroke_opacity.setProperty(val, specificity + 100000 * a_important);
            } else { // outline opacity
                handle->outline_opacity.setProperty(val, specificity + 100000 * a_important);
            }
        }
    } else if (property == "stroke-width" || property == "outline-width") {
        // Assuming px value only, which stays the same regardless of the size of the handles.
        int val;
        if (!a_value->content.num) {
            std::cerr << "Invalid value for " << property << ": " << value << std::endl;
            return;
        }
        if (a_value->content.num->type == NUM_LENGTH_PX) {
            val = int(a_value->content.num->val);
        } else {
            std::cerr << "Invalid type for " << property << ": " << value << std::endl;
            return;
        }

        for (auto& [handle, specificity] : selected_handles) {
            if (property == "stroke-width") {
                handle->stroke_width.setProperty(val, specificity + 100000 * a_important);
            } else {
                handle->outline_width.setProperty(val, specificity + 100000 * a_important);
            }
        }
    } else {
        std::cerr << "Unrecognized property: " << property << std::endl;
    }
}

/**
 * Clean-up for selected handles vector.
 */
void clear_selectors(CRDocHandler *a_handler, CRSelector *a_selector)
{
    selected_handles.clear();
}

/**
 * Parse and set handle styles from css.
 */
void CanvasItemCtrl::parse_handle_styles() const
{
    for (int type_i = CANVAS_ITEM_CTRL_TYPE_DEFAULT; type_i <= CANVAS_ITEM_CTRL_TYPE_INVISIPOINT; type_i++) {
        auto type = static_cast<CanvasItemCtrlType>(type_i);
        for (auto state_bits = 0; state_bits < 8; state_bits++) {
            bool selected = state_bits & (1<<2);
            bool hover = state_bits & (1<<1);
            bool click = state_bits & (1<<0);
            delete handle_styles[Handle(type, selected, hover, click)];
            handle_styles[Handle(type, selected, hover, click)] = new HandleStyle();
        }
    }

    CRDocHandler *sac = cr_doc_handler_new();
    sac->start_selector = set_selectors_base;
    sac->property = set_properties;
    sac->end_selector = clear_selectors;

    auto base_css_path = Inkscape::IO::Resource::get_path_string(Inkscape::IO::Resource::SYSTEM, Inkscape::IO::Resource::UIS, "node-handles.css");
    if (Inkscape::IO::file_test(base_css_path.c_str(), G_FILE_TEST_EXISTS)) {
        CRParser *base_parser = cr_parser_new_from_file(reinterpret_cast<const guchar *>(base_css_path.c_str()), CR_ASCII);
        cr_parser_set_sac_handler(base_parser, sac);
        cr_parser_parse(base_parser);
    }

    auto user_css_path = Inkscape::IO::Resource::get_path_string(Inkscape::IO::Resource::USER, Inkscape::IO::Resource::UIS, "node-handles.css");
    if (Inkscape::IO::file_test(user_css_path.c_str(), G_FILE_TEST_EXISTS)) {
        CRParser *user_parser = cr_parser_new_from_file(reinterpret_cast<const guchar *>(user_css_path.c_str()), CR_ASCII);
        sac->start_selector = set_selectors_user;
        cr_parser_set_sac_handler(user_parser, sac);
        cr_parser_parse(user_parser);
    }
}

/**
 * Build object-specific cache.
 */
void CanvasItemCtrl::build_cache(int device_scale) const
{
    if (_width < 2) {
        return; // Nothing to render
    }

    if (_width % 2 == 0) {
        std::cerr << "CanvasItemCtrl::build_cache: Width not odd integer! "
                    << _name << ":  width: " << _width << std::endl;
    }

    int width  = _width  * device_scale;  // Not unsigned or math errors occur!
    std::tuple<Handle,int,double> handle_prop = std::make_tuple(_handle, width, _angle);

    if (_shape_set || _fill_set || _stroke_set) {
        _cache = std::make_unique<uint32_t[]>(width*width); // TODO: (C++20) make_shared
        if (auto handle_style_find = handle_styles.find(_handle); handle_style_find != handle_styles.end()) {
            auto handle_style = handle_style_find->second;
            auto shape = (_shape_set)?_shape:handle_style->shape();
            auto fill = (_fill_set)?_fill:handle_style->getFill();
            auto stroke = (_stroke_set)?_stroke:handle_style->getStroke();
            auto outline = handle_style->getOutline();
            auto stroke_width = handle_style->stroke_width();
            auto outline_width = handle_style->outline_width();
            draw_shape(_cache, shape, fill, stroke, outline, stroke_width, outline_width,
                        width, _angle, device_scale);
        } else {
            draw_shape(_cache, _shape, _fill, _stroke, 0, 1, 0, width, _angle, device_scale);
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if(auto cached_handle = handle_cache.find(handle_prop); cached_handle != handle_cache.end()) {
            _cache = cached_handle->second;
            return;
        }
    }

    _cache = std::make_unique<uint32_t[]>(width*width); // TODO: (C++20) make_shared
    if (auto handle_style_find = handle_styles.find(_handle); handle_style_find != handle_styles.end()) {
        auto handle_style = handle_style_find->second;
        auto shape = handle_style->shape();
        auto fill = handle_style->getFill();
        auto stroke = handle_style->getStroke();
        auto outline = handle_style->getOutline();
        auto stroke_width = handle_style->stroke_width();
        auto outline_width = handle_style->outline_width();
        draw_shape(_cache, shape, fill, stroke, outline, stroke_width, outline_width,
                    width, _angle, device_scale);
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            handle_cache[handle_prop] = _cache;
        }
    } else {
        // Shouldn't happen - every ctrl either have a style in handle_styles 
        // or their style must be set in the code
        std::cerr << "CanvasItemCtrl::build_cache: Unhandled Ctrl - " << _name << std::endl;
    }
}

/**
 * Draw the handles as described by the arguments.
 */
void CanvasItemCtrl::draw_shape(std::shared_ptr<uint32_t[]> cache,
    CanvasItemCtrlShape shape, uint32_t fill, uint32_t stroke, uint32_t outline,
    int stroke_width, int outline_width,
    int width, double angle, int device_scale)
{
    // TODO: maybe replace the long list of arguments with a handle-style entity.
    int scaled_stroke = device_scale * stroke_width; 
    int scaled_outline = device_scale * outline_width; 
    auto p = cache.get();
    switch (shape) {
    case CANVAS_ITEM_CTRL_SHAPE_SQUARE:
        // Actually any rectanglular shape.
        for (int i = 0; i < width; ++i) {
            for (int j = 0; j < width; ++j) {
                if(i < outline_width || j < outline_width ||
                    width - i <= outline_width || width - j <= outline_width) {
                    *p++ = outline;
                } else if (i < outline_width + stroke_width || j < outline_width + stroke_width ||
                    width - i <= outline_width + stroke_width || width - j <= outline_width + stroke_width) {
                    *p++ = stroke;
                } else {
                    *p++ = fill;
                }
            }
        }
        break;

    case CANVAS_ITEM_CTRL_SHAPE_DIAMOND: {
        int m = (width + 1) / 2;

        for (int i = 0; i < width; ++i) {
            for (int j = 0; j < width; ++j) {
                if (i  +           j  > m - 1 + scaled_stroke + scaled_outline &&
                        (width - 1 - i) +           j  > m - 1 + scaled_stroke + scaled_outline &&
                        (width - 1 - i) + (width - 1 - j) > m - 1 + scaled_stroke + scaled_outline &&
                        i    + (width - 1 - j) > m - 1 + scaled_stroke + scaled_outline) {
                    *p++ = fill;
                } else if (i  +           j  > m - 1 + scaled_outline &&
                        (width - 1 - i) +           j  > m - 1 + scaled_outline &&
                        (width - 1 - i) + (width - 1 - j) > m - 1 + scaled_outline &&
                        i    + (width - 1 - j) > m - 1 + scaled_outline) {
                    *p++ = stroke;
                } else if (i  +           j  > m - 2 &&
                         (width - 1 - i) +           j  > m - 2 &&
                         (width - 1 - i) + (width - 1 - j) > m - 2 &&
                         i    + (width - 1 - j) > m - 2) {
                    *p++ = outline;
                } else {
                    *p++ = 0;
                }
            }
        }
        break;
    }

    case CANVAS_ITEM_CTRL_SHAPE_CIRCLE: {
        double ro  = width / 2.0;
        double ro2 = ro * ro;
        double rs  = ro - scaled_outline;
        double rs2 = rs * rs;
        double rf  = ro - (scaled_stroke + scaled_outline);
        double rf2 = rf * rf;

        for (int i = 0; i < width; ++i) {
            for (int j = 0; j < width; ++j) {

                double rx = i - (width / 2.0) + 0.5;
                double ry = j - (width / 2.0) + 0.5;
                double r2 = rx * rx + ry * ry;

                if (r2 < rf2) {
                    *p++ = fill;
                } else if (r2 < rs2) {
                    *p++ = stroke;
                } else if (r2 < ro2) {
                    *p++ = outline;
                } else {
                    *p++ = 0;
                }
            }
        }
        break;
    }

    case CANVAS_ITEM_CTRL_SHAPE_CROSS: {
        // Actually an 'x'. 
        double rel0 = scaled_stroke / sqrt(2);
        double rel1 = (2 * scaled_outline + scaled_stroke) / sqrt(2);
        double rel2 = (4 * scaled_outline + scaled_stroke) / sqrt(2);

        for (int y = 0; y < width; y++) {
            for (int x = 0; x < width; x++) {
                if ((abs(x-y) <= std::max(width-rel2,0.0) && abs(x+y-width) <= rel0) ||
                    (abs(x-y) <= rel0 && abs(x+y-width) <= std::max(width-rel2,0.0))) {
                    *p++ = stroke;
                } else if ((abs(x-y) <= std::max(width-rel1,0.0) && abs(x+y-width) <= rel1) ||
                           (abs(x-y) <= rel1 && abs(x+y-width) <= std::max(width-rel1,0.0))) {
                    *p++ = outline;
                } else {
                    *p++ = 0;
                }
            }
        }
        break;
    }

    case CANVAS_ITEM_CTRL_SHAPE_PLUS:
        // Actually an '+'.
        for (int y = 0; y < width; y++) {
            for (int x = 0; x < width; x++) {
                if ((std::abs(x - width / 2) < scaled_stroke / 2.0 ||
                     std::abs(y - width / 2)  < scaled_stroke / 2.0) &&
                    (x >= scaled_outline && y >= scaled_outline &&
                     width - x >= scaled_outline + 1 && width - y >= scaled_outline + 1)) {
                    *p++ = stroke;
                } else if (std::abs(x - width / 2) < scaled_stroke / 2.0 + scaled_outline ||
                           std::abs(y - width / 2)  < scaled_stroke / 2.0 + scaled_outline){
                    *p++ = outline;
                } else {
                    *p++ = 0;
                }
            }
        }
        break;

    case CANVAS_ITEM_CTRL_SHAPE_TRIANGLE:        //triangle optionaly rotated
    case CANVAS_ITEM_CTRL_SHAPE_TRIANGLE_ANGLED: // triangle with pointing to center of  knot and rotated this way
    case CANVAS_ITEM_CTRL_SHAPE_DARROW:          // Double arrow
    case CANVAS_ITEM_CTRL_SHAPE_SARROW:          // Same shape as darrow but rendered rotated 90 degrees.
    case CANVAS_ITEM_CTRL_SHAPE_CARROW:          // Double corner arrow
    case CANVAS_ITEM_CTRL_SHAPE_PIVOT:           // Fancy "plus"
    case CANVAS_ITEM_CTRL_SHAPE_SALIGN:          // Side align (triangle pointing toward line)
    case CANVAS_ITEM_CTRL_SHAPE_CALIGN:          // Corner align (triangle pointing into "L")
    case CANVAS_ITEM_CTRL_SHAPE_MALIGN: {        // Middle align (four triangles poining inward)
        double size = width / device_scale; // Use unscaled width.

        auto work = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, device_scale * size, device_scale * size);
        cairo_surface_set_device_scale(work->cobj(), device_scale, device_scale); // No C++ API!
        auto cr = Cairo::Context::create(work);
        // Rotate around center
        cr->translate(size / 2.0,  size / 2.0);
        cr->rotate(angle);
        cr->translate(-size / 2.0, -size / 2.0);

        // Clip the region outside the handle for outline. 
        // (1.5 is an approximation of root(2) and 3 is 1.5 * 2)
        double effective_outline = outline_width + 0.5 * stroke_width;
        cr->rectangle(size, 0,  -size, size);
        draw_cairo_path(shape, cr, size - 3 * effective_outline, 1.5 * effective_outline, 1.5 * effective_outline);
        cr->clip();
    
        // Draw the outline.   
        draw_cairo_path(shape, cr, size - 3 * effective_outline, 1.5 * effective_outline, 1.5 * effective_outline);
        cr->set_source_rgba(SP_RGBA32_R_F(outline),
                            SP_RGBA32_G_F(outline),
                            SP_RGBA32_B_F(outline),
                            SP_RGBA32_A_F(outline));
        cr->set_line_width(2 * effective_outline);
        cr->stroke();
        cr->reset_clip();

        // Fill and stroke.
        draw_cairo_path(shape, cr, size - 3 * effective_outline, 1.5 * effective_outline, 1.5 * effective_outline);
        cr->set_source_rgba(SP_RGBA32_R_F(fill),
                            SP_RGBA32_G_F(fill),
                            SP_RGBA32_B_F(fill),
                            SP_RGBA32_A_F(fill));
        cr->fill_preserve();
        cr->set_source_rgba(SP_RGBA32_R_F(stroke),
                            SP_RGBA32_G_F(stroke),
                            SP_RGBA32_B_F(stroke),
                            SP_RGBA32_A_F(stroke));
        cr->set_line_width(stroke_width);
        cr->stroke();

        // Copy to buffer.
        work->flush();
        int stride = work->get_stride();
        unsigned char *work_ptr = work->get_data();
        auto p = cache.get();
        for (int i = 0; i < device_scale * size; ++i) {
            auto pb = reinterpret_cast<uint32_t *>(work_ptr + i * stride);
            for (int j = 0; j < width; ++j) {
                *p++ = rgba_from_argb32(*pb++);
            }
        }
        break;
    }

    default:
        std::cerr << "CanvasItemCtrl::draw_shape: unhandled shape!" << std::endl;
        break;
    }
}

} // namespace Inkscape

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
