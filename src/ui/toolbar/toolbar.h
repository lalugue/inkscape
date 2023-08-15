// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_TOOLBAR_H
#define SEEN_TOOLBAR_H

#include <gtkmm/builder.h>
#include <stack>

#include "ui/widget/toolbar-menu-button.h"

namespace Glib {
class ustring;
} // namespace Glib

namespace Gtk {
class Label;
class ToggleButton;
} // namespace Gtk;

class SPDesktop;

namespace Inkscape::UI::Toolbar {

/**
 * \brief An abstract definition for a toolbar within Inkscape
 *
 * \detail This is basically the same as a Gtk::Toolbar but contains a
 *         few convenience functions. All toolbars must define a "create"
 *         function that adds all the required tool-items and returns the
 *         toolbar as a GtkWidget
 */

class Toolbar : public Gtk::Box
{
public:
    Gtk::Box *_toolbar;
    std::stack<Inkscape::UI::Widget::ToolbarMenuButton *> _expanded_menu_btns;
    std::stack<Inkscape::UI::Widget::ToolbarMenuButton *> _collapsed_menu_btns;

protected:
    SPDesktop *_desktop;
    bool _freeze_resize = false;

    /**
     * \brief A default constructor that just assigns the desktop
     */
    Toolbar(SPDesktop *desktop);

    Glib::RefPtr<Gtk::Builder> initialize_builder(Glib::ustring const &file_name);
    void resize_handler(Gtk::Allocation &allocation);
    void move_children(Gtk::Box *src, Gtk::Box *dest, std::vector<std::pair<int, Gtk::Widget *>> children,
                       bool is_expanding = false);
};

} // namespace Inkscape::UI::Toolbar

#endif // SEEN_TOOLBAR_H

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
