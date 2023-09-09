// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author:
 *   Vaibhav Malik <vaibhavmalik2018@gmail.com>
 *
 * Copyright (C) 2011 Author
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "toolbar-menu-button.h"

#include <iostream>

namespace Inkscape {
namespace UI {
namespace Widget {

ToolbarMenuButton::ToolbarMenuButton(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder> &refGlade)
    : Gtk::MenuButton(cobject)
{
    // std::cout << "ToolbarMenuButton::BuilderConstructor()\n";
    // Workaround to hide the widget by default and when there are no
    // children in the popover.
    set_visible(false);
    signal_show().connect(
        [=]() {
            if (_popover_box->get_children().empty()) {
                set_visible(false);
            }
        },
        false);
}

ToolbarMenuButton::ToolbarMenuButton()
    : Gtk::MenuButton()
{
    // std::cout << "ToolbarMenuButton::Constructor()\n";
}

void ToolbarMenuButton::init(int priority, std::string tag, Gtk::Box *popover_box, std::vector<Gtk::Widget *> &children)
{
    // std::cout << "ToolbarMenuButton::init()\n";
    _priority = priority;
    _tag = tag;
    _popover_box = popover_box;

    // std::cout << "Priority: " << _priority << std::endl;
    // std::cout << "Tag: " << _tag << std::endl;

    // Automatically fetch all the children having "tag" as their style class.
    // This approach will allow even non-programmers to group the children into
    // popovers. Store the position of the child in the toolbar as well. It'll
    // be used to reinsert the child in the right place when the toolbar is
    // large enough.
    int pos = 0;

    for (auto child : children) {
        auto style_context = child->get_style_context();
        bool is_child = style_context->has_class(tag);

        if (is_child) {
            _children.emplace_back(std::make_pair(pos, child));
        }

        pos++;
    }
}

int ToolbarMenuButton::get_priority()
{
    // std::cout << "ToolbarMenuButton::get_priority()\n";

    return _priority;
}

std::string ToolbarMenuButton::get_tag()
{
    // std::cout << "ToolbarMenuButton::get_tag()\n";

    return _tag;
}

std::vector<std::pair<int, Gtk::Widget *>> ToolbarMenuButton::get_children()
{
    return _children;
}

Gtk::Box *ToolbarMenuButton::get_popover_box()
{
    return _popover_box;
}

static int minw(Gtk::Widget const *widget)
{
    int min = 0;
    int nat = 0;
    widget->get_preferred_width(min, nat);
    return min;
};

int ToolbarMenuButton::get_required_width() const
{
    return minw(_popover_box) - minw(this);
}

} // namespace Widget
} // namespace UI
} // namespace Inkscape

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
