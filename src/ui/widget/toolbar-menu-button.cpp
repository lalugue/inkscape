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
}

ToolbarMenuButton::ToolbarMenuButton()
    : Gtk::MenuButton()
{
    // std::cout << "ToolbarMenuButton::Constructor()\n";
}

void ToolbarMenuButton::init(int priority, std::string tag, std::string icon_name, Gtk::Box *popover_box,
                             std::vector<Gtk::Widget *> &children)
{
    // std::cout << "ToolbarMenuButton::init()\n";
    _priority = priority;
    _tag = _tag;
    _popover_box = popover_box;

    // std::cout << "Priority: " << _priority << std::endl;
    // std::cout << "Tag: " << _tag << std::endl;
    // std::cout << "Icon Name: " << icon_name << std::endl;

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

int ToolbarMenuButton::get_required_width()
{
    // std::cout << "ToolbarMenuButton::get_required_width()\n";

    int req_w = 0;
    int min_w = 0;
    int nat_w = 0;

    _popover_box->get_preferred_width(min_w, nat_w);
    req_w = std::max(min_w, nat_w);
    this->get_preferred_width(min_w, nat_w);

    return req_w - std::min(min_w, nat_w);
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
