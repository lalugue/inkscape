// SPDX-License-Identifier: GPL-2.0-or-later
#include "ctrl-handle-rendering.h"

#include <mutex>
#include <unordered_map>
#include <boost/container_hash/hash.hpp>
#include <cairomm/context.h>

#include "color.h"
#include "display/cairo-utils.h"

namespace Inkscape {
namespace {

void draw_darrow(Cairo::RefPtr<Cairo::Context> const &cr, double size, double offset_x, double offset_y)
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

void draw_carrow(Cairo::RefPtr<Cairo::Context> const &cr, double size, double offset_x, double offset_y)
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

void draw_triangle(Cairo::RefPtr<Cairo::Context> const &cr, double size, double offset_x, double offset_y)
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

void draw_triangle_angled(Cairo::RefPtr<Cairo::Context> const &cr, double size, double offset_x, double offset_y)
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

void draw_pivot(Cairo::RefPtr<Cairo::Context> const &cr, double size, double offset_x, double offset_y)
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

void draw_salign(Cairo::RefPtr<Cairo::Context> const &cr, double size, double offset_x, double offset_y)
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

void draw_calign(Cairo::RefPtr<Cairo::Context> const &cr, double size, double offset_x, double offset_y)
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

void draw_malign(Cairo::RefPtr<Cairo::Context> const &cr, double size, double offset_x, double offset_y)
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

void draw_cairo_path(CanvasItemCtrlShape shape, Cairo::RefPtr<Cairo::Context> const &cr,
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

std::unordered_map<HandleTuple, std::shared_ptr<uint32_t const []>> handle_cache;
std::mutex cache_mutex;

} // namespace

void draw_shape(uint32_t *cache,
                CanvasItemCtrlShape shape, uint32_t fill, uint32_t stroke, uint32_t outline,
                int stroke_width, int outline_width,
                int width, double angle, int device_scale)
{
    // TODO: Maybe replace the long list of arguments with a handle-style entity.
    int const scaled_stroke = device_scale * stroke_width;
    int const scaled_outline = device_scale * outline_width;

    auto p = cache;

    switch (shape) {
        case CANVAS_ITEM_CTRL_SHAPE_SQUARE:
            // Actually any rectangular shape.
            for (int i = 0; i < width; ++i) {
                for (int j = 0; j < width; ++j) {
                    if (        i <  scaled_outline ||         j <  scaled_outline ||
                        width - i <= scaled_outline || width - j <= scaled_outline)
                    {
                        *p++ = outline;
                    } else if (        i <  scaled_outline + scaled_stroke ||         j <  scaled_outline + scaled_stroke ||
                               width - i <= scaled_outline + scaled_stroke || width - j <= scaled_outline + scaled_stroke)
                    {
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
                    if (             i  +              j  > m - 1 + scaled_stroke + scaled_outline &&
                        (width - 1 - i) +              j  > m - 1 + scaled_stroke + scaled_outline &&
                        (width - 1 - i) + (width - 1 - j) > m - 1 + scaled_stroke + scaled_outline &&
                                     i  + (width - 1 - j) > m - 1 + scaled_stroke + scaled_outline)
                    {
                        *p++ = fill;
                    } else if (             i  +              j  > m - 1 + scaled_outline &&
                               (width - 1 - i) +              j  > m - 1 + scaled_outline &&
                               (width - 1 - i) + (width - 1 - j) > m - 1 + scaled_outline &&
                                            i  + (width - 1 - j) > m - 1 + scaled_outline)
                    {
                        *p++ = stroke;
                    } else if (             i  +              j  > m - 2 &&
                               (width - 1 - i) +              j  > m - 2 &&
                               (width - 1 - i) + (width - 1 - j) > m - 2 &&
                                            i  + (width - 1 - j) > m - 2)
                    {
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
                    if ((abs(x - y) <= std::max(width - rel2, 0.0) && abs(x + y - width) <= rel0) ||
                        (abs(x - y) <= rel0 && abs(x + y - width) <= std::max(width - rel2, 0.0)))
                    {
                        *p++ = stroke;
                    } else if ((abs(x - y) <= std::max(width - rel1, 0.0) && abs(x + y - width) <= rel1) ||
                               (abs(x - y) <= rel1 && abs(x + y - width) <= std::max(width - rel1, 0.0)))
                    {
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
                         std::abs(y - width / 2) < scaled_stroke / 2.0) &&
                        (x >= scaled_outline && y >= scaled_outline &&
                         width - x >= scaled_outline + 1 && width - y >= scaled_outline + 1))
                    {
                        *p++ = stroke;
                    } else if (std::abs(x - width / 2) < scaled_stroke / 2.0 + scaled_outline ||
                               std::abs(y - width / 2) < scaled_stroke / 2.0 + scaled_outline)
                    {
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

std::shared_ptr<uint32_t const []> lookup_cache(HandleTuple const &prop)
{
    auto lock = std::lock_guard{cache_mutex};
    auto const it = handle_cache.find(prop);
    if (it == handle_cache.end()) {
        return {};
    }
    return it->second;
}

void insert_cache(HandleTuple const &prop, std::shared_ptr<uint32_t const []> cache)
{
    auto lock = std::lock_guard{cache_mutex};
    handle_cache[prop] = std::move(cache);
}

} // namespace Inkscape

namespace std {
size_t hash<Inkscape::HandleTuple>::operator()(Inkscape::HandleTuple const &tuple) const
{
    auto const tmp = std::make_tuple(hash<Inkscape::Handle>{}(std::get<0>(tuple)),
                                     std::get<1>(tuple),
                                     std::get<2>(tuple));
    return boost::hash<decltype(tmp)>{}(tmp);
}
} // namespace std

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
