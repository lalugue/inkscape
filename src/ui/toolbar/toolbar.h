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

#include <stack>

#include "ui/widget/toolbar-menu-button.h"

namespace Glib { class ustring; }

namespace Gtk {
class Builder;
class Label;
class ToggleButton;
} // namespace Gtk

class SPDesktop;

namespace Inkscape::UI::Toolbar {

/**
 * \brief An abstract definition for a toolbar within Inkscape
 *
 * \detail This is basically the same as a Gtk::Toolbar but contains a
 *         few convenience functions.
 */

class Toolbar : public Gtk::Box
{
protected:
    Toolbar(SPDesktop *desktop);

    SPDesktop *const _desktop;
    Gtk::Box *_toolbar = nullptr;

    void addCollapsibleButton(UI::Widget::ToolbarMenuButton *button);

    void get_preferred_width_vfunc(int &minimum_width, int &natural_width) const override;
    void on_size_allocate(Gtk::Allocation &allocation) override;

private:
    std::stack<UI::Widget::ToolbarMenuButton *> _expanded_menu_btns;
    std::stack<UI::Widget::ToolbarMenuButton *> _collapsed_menu_btns;

    void _resize_handler(Gtk::Allocation &allocation);
    void _move_children(Gtk::Box *src, Gtk::Box *dest, std::vector<std::pair<int, Gtk::Widget *>> children, bool is_expanding = false);
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
