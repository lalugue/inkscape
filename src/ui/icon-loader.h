// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Icon Loader
 *//*
 * Authors:
 * see git history
 * Jabiertxo Arraiza <jabier.arraiza@marker.es>
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INK_ICON_LOADER_H
#define SEEN_INK_ICON_LOADER_H

#include <glibmm/refptr.h>
#include <gdkmm/pixbuf.h>
#include <gtkmm/enums.h>

namespace Glib {
class ustring;
} // namespace Glib

namespace Gdk {
class PixBuf;
class RGBA;
} // namespace Gdk

namespace Gtk {
class Image;
} // namespace Gtk

Gtk::Image *sp_get_icon_image(Glib::ustring const &icon_name, int size);
Gtk::Image *sp_get_icon_image(Glib::ustring const &icon_name, Gtk::BuiltinIconSize icon_size);
Gtk::Image *sp_get_icon_image(Glib::ustring const &icon_name, Gtk::IconSize icon_size);
GtkWidget  *sp_get_icon_image(Glib::ustring const &icon_name, GtkIconSize icon_size);
Glib::RefPtr<Gdk::Pixbuf> sp_get_icon_pixbuf(Glib::ustring icon_name, int size);
Glib::RefPtr<Gdk::Pixbuf> sp_get_icon_pixbuf(Glib::ustring const &icon_name, Gtk::IconSize icon_size, int scale=1);
Glib::RefPtr<Gdk::Pixbuf> sp_get_icon_pixbuf(Glib::ustring const &icon_name, Gtk::BuiltinIconSize icon_size, int scale=1);
Glib::RefPtr<Gdk::Pixbuf> sp_get_icon_pixbuf(Glib::ustring const &icon_name, GtkIconSize icon_size, int scale=1);
Glib::RefPtr<Gdk::Pixbuf> sp_get_shape_icon (Glib::ustring const &shape_type, Gdk::RGBA const &color, int size, int scale=1);

#endif // SEEN_INK_ICON_LOADER_H

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
