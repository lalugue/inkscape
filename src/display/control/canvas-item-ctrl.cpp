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

    int size = 0;
    switch (_handle.type) {
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
    case CANVAS_ITEM_CTRL_TYPE_NODE_SYMMETRICAL:
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
        g_warning("set_size_via_index: missing case for handle type: %d", static_cast<int>(_handle.type));
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
    assert(_width % 2 == 1);

    // Get half width, rounded down.
    int const w_half = _width / 2;

    // Set _angle, and compute adjustment for anchor.
    int dx = 0;
    int dy = 0;

    CanvasItemCtrlShape shape = _shape;
    if (!_shape_set) {
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
    if (_width < 2) {
        return; // Nothing to render
    }

    if (_width % 2 == 0) {
        std::cerr << "CanvasItemCtrl::build_cache: Width not odd integer! "
                  << _name << ":  width: " << _width << std::endl;
    }
    
    auto const &style = _context->handlesCss()->style_map.at(_handle);

    _cache = Handles::draw({
        .shape = _shape_set ? _shape : style.shape(),
        .fill = _fill_set ? _fill : style.getFill(),
        .stroke = _stroke_set ? _stroke : style.getStroke(),
        .outline = style.getOutline(),
        .stroke_width = style.stroke_width(),
        .outline_width = style.outline_width(),
        .width = _width * device_scale,
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
