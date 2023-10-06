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

#ifndef SEEN_UI_DIALOG_IMAGETOGGLER_H
#define SEEN_UI_DIALOG_IMAGETOGGLER_H

#include <map>
#include <string>
#include <gtkmm/cellrenderer.h>
#include <glibmm/property.h>
#include <glibmm/propertyproxy.h>
#include <sigc++/signal.h>

namespace Inkscape::UI::Widget {

class ImageToggler : public Gtk::CellRenderer {
public:
    ImageToggler(char const *on, char const *off);

    sigc::signal<void (const Glib::ustring&)> signal_toggled() { return _signal_toggled;}

    Glib::PropertyProxy<bool> property_active() { return _property_active.get_proxy(); }
    Glib::PropertyProxy<bool> property_activatable() { return _property_activatable.get_proxy(); }
    Glib::PropertyProxy<bool> property_gossamer() { return _property_gossamer.get_proxy(); }
    Glib::PropertyProxy<std::string> property_active_icon() { return _property_active_icon.get_proxy(); }

    /// Sets whether to force visible icons in ALL cells of the column, EVEN IF their activatable &
    /// property_active() properties are false. The ObjectsPanel uses this to show all blend icons.
    void set_force_visible(bool force_visible);

private:
    void render_vfunc(const Cairo::RefPtr<Cairo::Context> &cr,
                      Gtk::Widget &widget,
                      const Gdk::Rectangle &background_area,
                      const Gdk::Rectangle &cell_area,
                      Gtk::CellRendererState flags) override;

    void get_preferred_width_vfunc(Gtk::Widget &widget,
                                   int &min_w,
                                   int &nat_w) const override;
    
    void get_preferred_height_vfunc(Gtk::Widget &widget,
                                    int &min_h,
                                    int &nat_h) const override;

    bool activate_vfunc(GdkEvent *event,
                        Gtk::Widget &widget,
                        const Glib::ustring &path,
                        const Gdk::Rectangle &background_area,
                        const Gdk::Rectangle &cell_area,
                        Gtk::CellRendererState flags) override;

    int _size;
    Glib::ustring _pixOnName;
    Glib::ustring _pixOffName;
    bool _force_visible = false;
    Glib::Property<bool> _property_active;
    Glib::Property<bool> _property_activatable;
    Glib::Property<bool> _property_gossamer;
    Glib::Property<Glib::RefPtr<Gdk::Pixbuf>> _property_pixbuf_on;
    Glib::Property<Glib::RefPtr<Gdk::Pixbuf>> _property_pixbuf_off;
    Glib::Property<std::string> _property_active_icon;
    std::map<const std::string, Glib::RefPtr<Gdk::Pixbuf>> _icon_cache;

    sigc::signal<void (const Glib::ustring&)> _signal_toggled;
};

} // namespace Inkscape::UI::Widget

#endif // SEEN_UI_DIALOG_IMAGETOGGLER_H

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
