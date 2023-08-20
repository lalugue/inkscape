// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "toolbar.h"

#include "desktop.h"
#include "io/resource.h"

namespace Inkscape::UI::Toolbar {

/**
 * \brief A default constructor that just assigns the desktop
 */
Toolbar::Toolbar(SPDesktop *desktop)
    : _desktop(desktop)
{
    signal_size_allocate().connect(sigc::mem_fun(*this, &Toolbar::resize_handler));
}

Glib::RefPtr<Gtk::Builder> Toolbar::initialize_builder(Glib::ustring const &file_name)
{
    Glib::ustring toolbar_builder_file = get_filename(Inkscape::IO::Resource::UIS, file_name.c_str());
    auto builder = Gtk::Builder::create();
    try {
        builder->add_from_file(toolbar_builder_file);
    } catch (Glib::Error const &ex) {
        std::cerr << "Failed to initialize the builder for: " << file_name << ex.what().raw() << std::endl;
    }

    return builder;
}

void Toolbar::resize_handler(Gtk::Allocation &allocation)
{
    _resize_handler(allocation, false);
}

void Toolbar::_resize_handler(Gtk::Allocation &allocation, bool freeze_idle)
{
    // Return if called in freeze state.
    if (_freeze_resize) {
        return;
    }

    if (_toolbar == nullptr) {
        return;
    }

    _freeze_resize = true;

    int min_w = 0;
    int nat_w = 0;

    get_preferred_width(min_w, nat_w);

    // Check if the toolbar needs to be resized.
    // Added an offset of 7 pixels to allow toolbars to shrink
    // on MacOS.
    if (allocation.get_width() <= (std::max(min_w, nat_w) + 7)) {
        // Now, check if there are any expanded ToolbarMenuButtons.
        // If there are none, then the toolbar size can not be reduced further.
        if (_expanded_menu_btns.empty()) {
            _freeze_resize = false;
            return;
        }

        // If there still are some widgets which can be collapsed
        // Move the top most item from _expanded_menu_buttons.
        auto menu_btn = _expanded_menu_btns.top();
        move_children(_toolbar, menu_btn->get_popover_box(), menu_btn->get_children());
        menu_btn->set_visible(true);

        _expanded_menu_btns.pop();
        _collapsed_menu_btns.push(menu_btn);

        _freeze_resize = false;

        // Check if the toolbar has become small enough otherwise recursively
        // call this handler again.
        get_preferred_width(min_w, nat_w);

        // Added an offset of 7 pixels to allow toolbars to shrink
        // on MacOS.
        if (allocation.get_width() <= (std::max(min_w, nat_w) + 7)) {
            _resize_handler(allocation, false);
        } else if (!freeze_idle) {
            // TODO: Add the right idle calls.
        }
    } else {
        // Once the allocated width of the toolbar is greater than its
        // minimum width, try to re-insert the a group of elements back
        // into the toolbar.
        // First check if "_moved_children" is empty.
        if (_collapsed_menu_btns.empty()) {
            // No ToolbarMenuButton is there to be expanded.
            _freeze_resize = false;
            return;
        }

        // Two cases are possible:
        // 1. Toolbar width has decreased -> Do nothing
        // TODO: Find a way to detect this case.
        if (false) {
            return;
        }

        // 2. Toolbar width has increased -> Take action
        // Find the required width of the toolbar required to expand the top
        // most child of _collapsed_menu_btn stack.
        auto menu_btn = _collapsed_menu_btns.top();
        _toolbar->get_preferred_width(min_w, nat_w);
        int req_width = std::max(min_w, nat_w) + menu_btn->get_required_width();

        // Added an offset of 7 pixels to allow toolbars to shrink
        // on MacOS.
        if (allocation.get_width() > (req_width + 7)) {
            // Move a group of widgets back into the toolbar.
            move_children(menu_btn->get_popover_box(), _toolbar, menu_btn->get_children(), true);
            menu_btn->set_visible(false);

            _collapsed_menu_btns.pop();
            _expanded_menu_btns.push(menu_btn);
        } else if (!freeze_idle) {
            // TODO: Add the right idle calls.
        }
    }

    // This call will take care of widgets placement and overlapping issues.
    Gtk::Box::on_size_allocate(allocation);

    _freeze_resize = false;
}

void Toolbar::move_children(Gtk::Box *src, Gtk::Box *dest, std::vector<std::pair<int, Gtk::Widget *>> children,
                            bool is_expanding)
{
    for (auto child_pos_pair : children) {
        auto pos = child_pos_pair.first;
        auto child = child_pos_pair.second;

        src->remove(*child);
        dest->add(*child);

        // is_expanding will be true when the children are being put back into
        // the toolbar. In that case, insert the children at their previous
        // positions.
        if (is_expanding) {
            dest->reorder_child(*child, pos);
        }
    }
}

GtkWidget *Toolbar::create(SPDesktop *desktop)
{
    auto toolbar = Gtk::manage(new Toolbar(desktop));
    return toolbar->Gtk::Widget::gobj();
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
