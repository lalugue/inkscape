// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Toolbar for Commands.
 */
/* Authors:
 *   Tavmjong Bah
 *   Vaibhav Malik <vaibhavmalik2018@gmail.com>
 *
 * Copyright (C) 2023 Tavmjong Bah
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "command-toolbar.h"

#include <gtkmm/toolbar.h>

#include "preferences.h"
#include "ui/builder-utils.h"
#include "ui/pack.h"

namespace Inkscape::UI::Toolbar {

CommandToolbar::CommandToolbar(SPDesktop *desktop)
    : Toolbar(desktop)
    , _builder(initialize_builder("toolbar-commands.ui"))
{
    _toolbar = &get_widget<Gtk::Box>(_builder, "commands-toolbar");

    // Fetch all the ToolbarMenuButtons at once from the UI file
    // Menu Button #1
    auto popover_box1 = &get_widget<Gtk::Box>(_builder, "popover_box1");
    auto menu_btn1 = &get_derived_widget<UI::Widget::ToolbarMenuButton>(_builder, "menu_btn1");

    // Menu Button #2
    auto popover_box2 = &get_widget<Gtk::Box>(_builder, "popover_box2");
    auto menu_btn2 = &get_derived_widget<UI::Widget::ToolbarMenuButton>(_builder, "menu_btn2");

    // Menu Button #3
    auto popover_box3 = &get_widget<Gtk::Box>(_builder, "popover_box3");
    auto menu_btn3 = &get_derived_widget<UI::Widget::ToolbarMenuButton>(_builder, "menu_btn3");

    // Menu Button #4
    auto popover_box4 = &get_widget<Gtk::Box>(_builder, "popover_box4");
    auto menu_btn4 = &get_derived_widget<UI::Widget::ToolbarMenuButton>(_builder, "menu_btn4");

    // Menu Button #5
    auto popover_box5 = &get_widget<Gtk::Box>(_builder, "popover_box5");
    auto menu_btn5 = &get_derived_widget<UI::Widget::ToolbarMenuButton>(_builder, "menu_btn5");

    // Menu Button #6
    auto popover_box6 = &get_widget<Gtk::Box>(_builder, "popover_box6");
    auto menu_btn6 = &get_derived_widget<UI::Widget::ToolbarMenuButton>(_builder, "menu_btn6");

    // Initialize all the ToolbarMenuButtons only after all the children of the
    // toolbar have been fetched. Otherwise, the children to be moved in the
    // popover will get mapped to a different position and it will probably
    // cause segfault.
    auto children = _toolbar->get_children();

    menu_btn1->init(1, "tag1", "some-icon", popover_box1, children);
    _expanded_menu_btns.push(menu_btn1);

    menu_btn2->init(2, "tag2", "some-icon", popover_box2, children);
    _expanded_menu_btns.push(menu_btn2);

    menu_btn3->init(3, "tag3", "some-icon", popover_box3, children);
    _expanded_menu_btns.push(menu_btn3);

    menu_btn4->init(4, "tag4", "some-icon", popover_box4, children);
    _expanded_menu_btns.push(menu_btn4);

    menu_btn5->init(5, "tag5", "some-icon", popover_box5, children);
    _expanded_menu_btns.push(menu_btn5);

    menu_btn6->init(6, "tag6", "some-icon", popover_box6, children);
    _expanded_menu_btns.push(menu_btn6);

    add(*_toolbar);

    show_all();
}

} // namespace Inkscape::UI::Toolbar

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
