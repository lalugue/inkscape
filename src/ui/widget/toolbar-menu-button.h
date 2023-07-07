// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author:
 *   Vaibhav Malik <vaibhavmalik2018@gmail.com>
 *
 * Copyright (C) 2011 Author
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_TOOLBAR_MENU_BUTTON_H
#define INKSCAPE_UI_WIDGET_TOOLBAR_MENU_BUTTON_H

#include <gtkmm.h>
#include <gtkmm/box.h>
#include <gtkmm/builder.h>
#include <gtkmm/menubutton.h>
#include <vector>

namespace Inkscape {
namespace UI {
namespace Widget {

/**
 * TODO: Add description
 * ToolbarMenuButton widget
 */
class ToolbarMenuButton : public Gtk::MenuButton
{
    int _priority;
    std::string _tag;
    std::vector<std::pair<int, Gtk::Widget *>> _children;
    Gtk::Box *_popover_box;

public:
    ToolbarMenuButton();
    ~ToolbarMenuButton() override{};

    ToolbarMenuButton(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder> &refGlade);

    void init(int priority, std::string tag, std::string icon_name, Gtk::Box *popover_box,
              std::vector<Gtk::Widget *> &children);

    int get_priority();
    std::string get_tag();
    std::vector<std::pair<int, Gtk::Widget *>> get_children();
    Gtk::Box *get_popover_box();
    int get_required_width();
};

} // namespace Widget
} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UI_WIDGET_TOOLBAR_MENU_BUTTON_H

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
