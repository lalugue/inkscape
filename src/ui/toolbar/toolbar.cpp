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

#include <gtkmm/label.h>
#include <gtkmm/separatortoolitem.h>
#include <gtkmm/togglebutton.h>

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

Gtk::ToolItem *
Toolbar::add_label(const Glib::ustring &label_text)
{
    auto const ti = Gtk::make_managed<Gtk::ToolItem>();

    // For now, we always enable mnemonic
    auto const label = Gtk::make_managed<Gtk::Label>(label_text, true);

    ti->add(*label);
    pack_end(*ti, false, false, 2);

    return ti;
}

/**
 * \brief Add a toggle toolbutton to the toolbar
 *
 * \param[in] label_text   The text to display in the toolbar
 * \param[in] tooltip_text The tooltip text for the toolitem
 *
 * \returns The toggle button
 */
Gtk::ToggleButton *Toolbar::add_toggle_button(const Glib::ustring &label_text, const Glib::ustring &tooltip_text)
{
    auto const btn = Gtk::make_managed<Gtk::ToggleButton>(label_text);
    btn->set_tooltip_text(tooltip_text);
    pack_end(*btn, false, false, 2);
    return btn;
}

/**
 * \brief Add a separator line to the toolbar
 *
 * \details This is just a convenience wrapper for the
 *          standard GtkMM functionality
 */
void
Toolbar::add_separator()
{
    pack_end(*Gtk::manage(new Gtk::SeparatorToolItem()), false, false, 2);
}

Glib::RefPtr<Gtk::Builder> Toolbar::initialize_builder(const char *file_name)
{
    Glib::ustring select_toolbar_builder_file = get_filename(Inkscape::IO::Resource::UIS, file_name);
    auto builder = Gtk::Builder::create();
    try {
        builder->add_from_file(select_toolbar_builder_file);
    } catch (const Glib::Error &ex) {
        std::cerr << "Failed to initialize the builder for: " << file_name << ex.what().raw() << std::endl;
    }

    return builder;
}

void Toolbar::resize_handler(Gtk::Allocation &allocation)
{
    if (_toolbar == nullptr) {
        return;
    }

    int min_w = 0;
    int nat_w = 0;

    get_preferred_width(min_w, nat_w);

    // Check if the toolbar needs to be resized.
    if (allocation.get_width() <= std::max(min_w, nat_w)) {
        // Now, check if there are any expanded ToolbarMenuButtons.
        // If there are none, then the toolbar size can not be reduced further.
        if (_expanded_menu_btns.empty()) {
            return;
        }

        // If there still are some widgets which can be collapsed
        // Move the top most item from _expanded_menu_buttons.
        auto menu_btn = _expanded_menu_btns.top();
        move_children(_toolbar, menu_btn->get_popover_box(), menu_btn->get_children());
        menu_btn->set_visible(true);

        _expanded_menu_btns.pop();
        _collapsed_menu_btns.push(menu_btn);

        // Check if the toolbar has become small enough otherwise recursively
        // call this handler again.
        get_preferred_width(min_w, nat_w);

        if (allocation.get_width() <= std::max(min_w, nat_w)) {
            resize_handler(allocation);
        }
    } else {
        // Once the allocated width of the toolbar is greater than its
        // minimum width, try to re-insert the a group of elements back
        // into the toolbar.
        // First check if "_moved_children" is empty.
        if (_collapsed_menu_btns.empty()) {
            // No ToolbarMenuButton is there to be expanded.
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

        if (allocation.get_width() > req_width) {
            // Move a group of widgets back into the toolbar.
            move_children(menu_btn->get_popover_box(), _toolbar, menu_btn->get_children(), true);
            menu_btn->set_visible(false);

            _collapsed_menu_btns.pop();
            _expanded_menu_btns.push(menu_btn);
        }
    }
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

GtkWidget *
Toolbar::create(SPDesktop *desktop)
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
