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

#include <memory>
#include <2geom/point.h>

#include "canvas-item.h"
#include "canvas-item-enums.h"
#include "ctrl-handle-styling.h"

#include "enums.h" // SP_ANCHOR_X
#include "display/initlock.h"

namespace Inkscape {

class CanvasItemCtrl : public CanvasItem
{
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

protected:
    ~CanvasItemCtrl() override = default;

    void _update(bool propagate) override;
    void _render(CanvasItemBuffer &buf) const override;
    void _invalidate_ctrl_handles() override;

    void build_cache(int device_scale) const;

    // Geometry
    Geom::Point _position;

    // Display
    InitLock _built;
    mutable std::shared_ptr<Cairo::ImageSurface const> _cache;

    // Properties
    Handles::TypeState _handle;
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
