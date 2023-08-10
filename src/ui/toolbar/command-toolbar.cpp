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
    _builder->get_widget("commands-toolbar", _toolbar);
    if (!_toolbar) {
        std::cerr << "InkscapeWindow: Failed to load commands toolbar!" << std::endl;
    }

    // Fetch all the ToolbarMenuButtons at once from the UI file
    // Menu Button #1
    Gtk::Box *popover_box1;
    _builder->get_widget("popover_box1", popover_box1);

    Inkscape::UI::Widget::ToolbarMenuButton *menu_btn1 = nullptr;
    _builder->get_widget_derived("menu_btn1", menu_btn1);

    // Menu Button #2
    Gtk::Box *popover_box2;
    _builder->get_widget("popover_box2", popover_box2);

    Inkscape::UI::Widget::ToolbarMenuButton *menu_btn2 = nullptr;
    _builder->get_widget_derived("menu_btn2", menu_btn2);

    // Menu Button #3
    Gtk::Box *popover_box3;
    _builder->get_widget("popover_box3", popover_box3);

    Inkscape::UI::Widget::ToolbarMenuButton *menu_btn3 = nullptr;
    _builder->get_widget_derived("menu_btn3", menu_btn3);

    // Menu Button #4
    Gtk::Box *popover_box4;
    _builder->get_widget("popover_box4", popover_box4);

    Inkscape::UI::Widget::ToolbarMenuButton *menu_btn4 = nullptr;
    _builder->get_widget_derived("menu_btn4", menu_btn4);

    // Menu Button #5
    Gtk::Box *popover_box5;
    _builder->get_widget("popover_box5", popover_box5);

    Inkscape::UI::Widget::ToolbarMenuButton *menu_btn5 = nullptr;
    _builder->get_widget_derived("menu_btn5", menu_btn5);

    // Menu Button #6
    Gtk::Box *popover_box6;
    _builder->get_widget("popover_box6", popover_box6);

    Inkscape::UI::Widget::ToolbarMenuButton *menu_btn6 = nullptr;
    _builder->get_widget_derived("menu_btn6", menu_btn6);

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
