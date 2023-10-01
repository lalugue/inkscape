// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_DISPLAY_CONTROL_CTRL_HANDLE_RENDERING_H
#define INKSCAPE_DISPLAY_CONTROL_CTRL_HANDLE_RENDERING_H
/**
 * Control handle rendering/caching.
 */
/*
 * Authors:
 *   Sanidhya Singh
 *
 * Copyright (C) 2023 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <memory>

#include "ctrl-handle-styling.h"

namespace Inkscape {

using HandleTuple = std::tuple<Handle, int, double>;

/**
 * Draw the handles as described by the arguments.
 */
void draw_shape(uint32_t *cache,
                CanvasItemCtrlShape shape, uint32_t fill, uint32_t stroke, uint32_t outline,
                int stroke_width, int outline_width,
                int width, double angle, int device_scale);

std::shared_ptr<uint32_t const []> lookup_cache(HandleTuple const &prop);
void insert_cache(HandleTuple const &prop, std::shared_ptr<uint32_t const []> cache);

} // namespace Inkscape

template <> struct std::hash<Inkscape::HandleTuple>
{
    size_t operator()(Inkscape::HandleTuple const &tuple) const;
};

#endif // INKSCAPE_DISPLAY_CONTROL_CTRL_HANDLE_RENDERING_H

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
