// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Jon A. Cruz
 *   Johan B. C. Engelen
 *
 * Copyright (C) 2006-2008 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "ui/widget/imagetoggler.h"

#include <gdkmm/general.h>
#include <gtkmm/iconinfo.h>

#include "ui/icon-loader.h"
#include "ui/icon-names.h"

namespace Inkscape::UI::Widget {

ImageToggler::ImageToggler(char const *on, char const *off) :
    Glib::ObjectBase(typeid(ImageToggler)),
    Gtk::CellRenderer(),
    _pixOnName(on),
    _pixOffName(off),
    _property_active(*this, "active", false),
    _property_activatable(*this, "activatable", true),
    _property_gossamer(*this, "gossamer", false),
    _property_active_icon(*this, "active_icon", ""),
    _property_pixbuf_on(*this, "pixbuf_on", Glib::RefPtr<Gdk::Pixbuf>(nullptr)),
    _property_pixbuf_off(*this, "pixbuf_off", Glib::RefPtr<Gdk::Pixbuf>(nullptr))
{
    property_mode() = Gtk::CELL_RENDERER_MODE_ACTIVATABLE;
    Gtk::IconSize::lookup(Gtk::ICON_SIZE_MENU, _size, _size);
}

void ImageToggler::get_preferred_height_vfunc(Gtk::Widget &widget, int &min_h, int &nat_h) const
{
    min_h = _size + 6;
    nat_h = _size + 8;
}

void ImageToggler::get_preferred_width_vfunc(Gtk::Widget &widget, int &min_w, int &nat_w) const
{
    min_w = _size + 12;
    nat_w = _size + 16;
}

void ImageToggler::set_force_visible(bool const force_visible)
{
    _force_visible = force_visible;
}

void ImageToggler::render_vfunc(const Cairo::RefPtr<Cairo::Context> &cr,
                                Gtk::Widget &widget,
                                const Gdk::Rectangle &background_area,
                                const Gdk::Rectangle &cell_area,
                                Gtk::CellRendererState flags)
{
    int scale = 0;

    // Lazy/late pixbuf rendering to get access to scale factor from widget.
    if(!_property_pixbuf_on.get_value()) {
        scale = widget.get_scale_factor();
        _property_pixbuf_on = sp_get_icon_pixbuf(_pixOnName, _size * scale);
        _property_pixbuf_off = sp_get_icon_pixbuf(_pixOffName, _size * scale);
    }

    std::string icon_name = _property_active_icon.get_value();
    // if the icon isn't cached, render it to a pixbuf
    if (!icon_name.empty() && !_icon_cache[icon_name]) { 
        if (scale == 0) scale = widget.get_scale_factor();
        _icon_cache[icon_name] = sp_get_icon_pixbuf(icon_name, _size * scale);
    }

    // Hide when not being used.
    double alpha = 1.0;
    bool visible = _property_activatable.get_value()
                || _property_active.get_value()
                || _force_visible;
    if (!visible) {
        // XXX There is conflict about this value, some users want 0.2, others want 0.0
        alpha = 0.0;
    }
    if (_property_gossamer.get_value()) {
        alpha += 0.2;
    }
    if (alpha <= 0.0) {
        return;
    }

    Glib::RefPtr<Gdk::Pixbuf> pixbuf;
    if (_property_active.get_value()) {
        pixbuf = icon_name.empty() ? _property_pixbuf_on.get_value() : _icon_cache[icon_name];
    } else {
        pixbuf = _property_pixbuf_off.get_value();
    }

    // Center the icon in the cell area
    int x = cell_area.get_x() + int((cell_area.get_width() - _size) * 0.5);
    int y = cell_area.get_y() + int((cell_area.get_height() - _size) * 0.5);

    Gdk::Cairo::set_source_pixbuf(cr, pixbuf, x, y);
    cr->set_operator(Cairo::OPERATOR_ATOP);
    cr->rectangle(x, y, _size, _size);
    if (alpha < 1.0) {
        cr->clip();
        cr->paint_with_alpha(alpha);
    } else {
        cr->fill();
    }
}

bool
ImageToggler::activate_vfunc(GdkEvent * /*event*/,
                             Gtk::Widget &/*widget*/,
                             const Glib::ustring &path,
                             const Gdk::Rectangle &/*background_area*/,
                             const Gdk::Rectangle &/*cell_area*/,
                             Gtk::CellRendererState /*flags*/)
{
    _signal_toggled.emit(path);
    return false;
}

} // namespace Inkscape::UI::Widget

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
