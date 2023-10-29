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
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "canvas-item-ctrl.h"

#include <2geom/transforms.h>
#include <cmath>
#include <iostream>

#include "ctrl-handle-rendering.h"
#include "preferences.h" // Default size.
#include "ui/widget/canvas.h"

namespace Inkscape {

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
    , _handle{.type = type}
{
    _name = "CanvasItemCtrl:Type_" + std::to_string(_handle.type);
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

    auto size = size_index;
    set_size(size, false);
}

float CanvasItemCtrl::get_width() const {
    auto const &style = _context->handlesCss()->style_map.at(_handle);
    return _width * style.scale() + style.size_extra();
}

int CanvasItemCtrl::get_pixmap_width(int device_scale) const {
    return static_cast<int>(get_width()) * device_scale | 1;
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
        if (_handle.type == type) return;
        _handle.type = type;
        set_size_default();
        _built.reset();
        request_update(); // Possible geometry change
    });
}

void CanvasItemCtrl::set_selected(bool selected)
{
    defer([=, this] {
        _handle.selected = selected;
        _built.reset();
        request_update();
    });
}

void CanvasItemCtrl::set_click(bool click)
{
    defer([=, this] {
        _handle.click = click;
        _built.reset();
        request_update();
    });
}

void CanvasItemCtrl::set_hover(bool hover)
{
    defer([=, this] {
        _handle.hover = hover;
        _built.reset();
        request_update();
    });
}

/**
 * Reset the state to normal or normal selected
 */
void CanvasItemCtrl::set_normal(bool selected)
{
    defer([=, this] {
        _handle.selected = selected;
        _handle.hover = false;
        _handle.click = false;
        _built.reset();
        request_update();
    });
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

static double angle_of(Geom::Affine const &affine)
{
    return std::atan2(affine[1], affine[0]);
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
    const auto width = static_cast<int>(std::ceil(get_width())) | 1;

    // Get half width, rounded down.
    int const w_half = width / 2;

    // Set _angle, and compute adjustment for anchor.
    int dx = 0;
    int dy = 0;

    CanvasItemCtrlShape shape = _shape;
    if (!_shape_set) {
        if (_context->handlesCss()->style_map.count(_handle) == 0) {
            std::cout << "Missing style for handle " << _handle.type << std::endl;
            return;
        }
        auto const &style = _context->handlesCss()->style_map.at(_handle);
        shape = style.shape();
    }

    switch (shape) {
    case CANVAS_ITEM_CTRL_SHAPE_DARROW:
    case CANVAS_ITEM_CTRL_SHAPE_SARROW:
    case CANVAS_ITEM_CTRL_SHAPE_CARROW:
    case CANVAS_ITEM_CTRL_SHAPE_SALIGN:
    case CANVAS_ITEM_CTRL_SHAPE_CALIGN: {
        double angle = int{_anchor} * M_PI_4 + angle_of(affine());
        double const half = width / 2.0;

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
    _bounds = Geom::IntRect(pt, pt + Geom::IntPoint(width, width));

    // Queue redraw of new area
    request_redraw();
}

/**
 * Render ctrl to screen via Cairo.
 */
void CanvasItemCtrl::_render(CanvasItemBuffer &buf) const
{
    _built.init([&, this] {
        build_cache(buf.device_scale);
    });

    if (!_cache) {
        return;
    }

    auto const [x, y] = _bounds->min().round(); // Must be pixel aligned.

    buf.cr->save();
    cairo_set_source_surface(buf.cr->cobj(), const_cast<cairo_surface_t *>(_cache->cobj()), x - buf.rect.left(), y - buf.rect.top()); // C API is const-incorrect.
    buf.cr->paint();
    buf.cr->restore();
}

void CanvasItemCtrl::_invalidate_ctrl_handles()
{
    assert(!_context->snapshotted()); // precondition
    _built.reset();
    request_update();
}

/**
 * Build object-specific cache.
 */
void CanvasItemCtrl::build_cache(int device_scale) const
{
    auto width = get_width();
    if (width < 2) {
        return; // Nothing to render
    }

    // take size in logical pixels and make it fit physical pixel grid
    auto pixel_fit = [=](float v) { return std::round(v * device_scale) / device_scale; };

    auto const &style = _context->handlesCss()->style_map.at(_handle);
    // growing stroke width with handle size:
    auto stroke_width = pixel_fit(style.stroke_width() * (0.7f + _width / 6.0f));
    // fixed-size outline
    auto outline_width = pixel_fit(style.outline_width());
    // handle size
    auto size = std::floor(width * device_scale) / device_scale;

    _cache = Handles::draw({
        .shape = _shape_set ? _shape : style.shape(),
        .fill = _fill_set ? _fill : style.getFill(),
        .stroke = _stroke_set ? _stroke : style.getStroke(),
        .outline = style.getOutline(),
        .stroke_width = stroke_width,
        .outline_width = outline_width,
        .size = size,
        .width = get_pixmap_width(device_scale),
        .angle = _angle,
        .device_scale = device_scale
    });
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
