// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * A replacement for GTK3ʼs Gtk::MenuItem, as removed in GTK4.
 */
/*
 * Authors:
 *   Daniel Boles <dboles.src+inkscape@gmail.com>
 *
 * Copyright (C) 2023 Daniel Boles
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "ui/widget/popover-menu-item.h"

#include <pangomm/layout.h>
#include <gtkmm/box.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/stylecontext.h>
#include <gtkmm/tooltip.h>

#include "ui/menuize.h"
#include "ui/util.h"
#include "ui/widget/popover-menu.h"

namespace Inkscape::UI::Widget {

static void ellipsize(Gtk::Label &label, int const max_width_chars)
{
    if (max_width_chars <= 0) return;

    label.set_max_width_chars(max_width_chars);
    label.set_ellipsize(Pango::ELLIPSIZE_MIDDLE);
    label.set_has_tooltip(true);
    label.signal_query_tooltip().connect([&](int, int, bool,
                                             Glib::RefPtr<Gtk::Tooltip> const &tooltip)
    {
        if (!label.get_layout()->is_ellipsized()) return false;

        tooltip->set_text(label.get_text());
        return true;
    });
}

PopoverMenuItem::PopoverMenuItem(Glib::ustring const &text,
                                 bool const mnemonic,
                                 int const max_width_chars,
                                 Glib::ustring const &icon_name,
                                 Gtk::IconSize const icon_size,
                                 bool const popdown_on_activate)
    : Glib::ObjectBase{"PopoverMenuItem"}
    , CssNameClassInit{"menuitem"}
    , Gtk::Button{}
{
    g_assert(max_width_chars >= 0);

    get_style_context()->add_class("menuitem");
    set_relief(Gtk::RELIEF_NONE);

    Gtk::Label *label = nullptr;
    Gtk::Image *image = nullptr;

    if (!text.empty()) {
        label = Gtk::make_managed<Gtk::Label>(text, Gtk::ALIGN_START, Gtk::ALIGN_CENTER, mnemonic);
        ellipsize(*label, max_width_chars);
    }

    if (!icon_name.empty()) {
        image = Gtk::make_managed<Gtk::Image>(icon_name, icon_size);
    }

    if (label && image) {
        auto &hbox = *Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_HORIZONTAL, 8);
        hbox.add(*image);
        hbox.add(*label);
        add(hbox);
    } else if (label) {
        add(*label);
    } else if (image) {
        add(*image);
    }

    if (popdown_on_activate) {
        signal_activate().connect([this]
        {
            if (auto const menu = get_menu()) {
                menu->popdown();
            }
        });
    }

    UI::menuize(*this);
}

PopoverMenuItem::PopoverMenuItem(Glib::ustring const &text,
                                 bool const mnemonic,
                                 Glib::ustring const &icon_name,
                                 Gtk::IconSize const icon_size,
                                 bool const popdown_on_activate)
: PopoverMenuItem{text, mnemonic, 0, icon_name, icon_size, popdown_on_activate}
{}

Glib::SignalProxy<void> PopoverMenuItem::signal_activate()
{
    return signal_clicked();
}

PopoverMenu *PopoverMenuItem::get_menu()
{
    PopoverMenu *result = nullptr;
    for_each_parent(*this, [&](Gtk::Widget &parent)
    {
        if (auto const menu = dynamic_cast<PopoverMenu *>(&parent)) {
            result = menu;
            return ForEachResult::_break;
        }
        return ForEachResult::_continue;
    });
    return result;
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
