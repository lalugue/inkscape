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

#include "ui/builder-utils.h"
#include "ui/util.h"

namespace Inkscape::UI::Toolbar {

CommandToolbar::CommandToolbar(SPDesktop *desktop)
    : Toolbar(desktop)
    , _builder(create_builder("toolbar-commands.ui"))
{
    _toolbar = &get_widget<Gtk::Box>(_builder, "commands-toolbar");

    set_child(*_toolbar);
    init_menu_btns();
}

CommandToolbar::~CommandToolbar() = default;

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
