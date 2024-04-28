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
#include <utility>
#include <vector>
#include <memory>

#include <gtkmm/box.h>

#include "ui/widget/bin.h"

class SPDesktop;

namespace Gtk {
class MenuButton;
}

namespace Inkscape::UI::Toolbar {

/**
 * \brief Base class for all toolbars.
 */
class Toolbar : public UI::Widget::Bin
{
protected:
    Toolbar(SPDesktop *desktop);

    SPDesktop *const _desktop;
    Gtk::Box *_toolbar = nullptr;

    void measure_vfunc(Gtk::Orientation orientation, int for_size, int &min, int &nat, int &min_baseline, int &nat_baseline) const override;
    void on_size_allocate(int width, int height, int baseline) override;
    void insert_menu_btn(const int priority, int group_size,
                         std::stack<std::pair<Gtk::Widget *, Gtk::Widget *>> toolbar_children);

private:
    struct ToolbarMenuButton
    {
        // Constructor to initialize data members
        ToolbarMenuButton(int priority, int group_size, Gtk::MenuButton *menu_btn,
                          std::stack<std::pair<Gtk::Widget *, Gtk::Widget *>> toolbar_children)
            : priority(priority)
            , group_size(group_size)
            , menu_btn(menu_btn)
            , toolbar_children(std::move(toolbar_children))
        {}

        // Data members
        int priority;
        int group_size;
        Gtk::MenuButton *menu_btn;
        std::stack<std::pair<Gtk::Widget *, Gtk::Widget *>> popover_children;
        std::stack<std::pair<Gtk::Widget *, Gtk::Widget *>> toolbar_children;
    };

    std::vector<std::unique_ptr<ToolbarMenuButton>> _menu_btns;
    std::stack<int> _size_needed;
    int _active_mb_index = -1;
    bool _resizing = false;

    void _resize_handler(int width, int height);
    void _update_menu_btn_image(Gtk::Widget *child);
    void _move_children(Gtk::Box *src, Gtk::Box *dest, std::stack<std::pair<Gtk::Widget *, Gtk::Widget *>> &tb_children,
                        std::stack<std::pair<Gtk::Widget *, Gtk::Widget *>> &popover_children, int group_size,
                        bool is_expanding = false);

public:
    void init_menu_btns();
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
