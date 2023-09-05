// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Toolbar for Commands.
 */
/*
 * Authors:
 *   Tavmjong Bah
 *
 * Copyright (C) 2023 Tavmjong Bah
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_TOOLBAR_COMMAND_H
#define SEEN_TOOLBAR_COMMAND_H

#include <gtkmm/box.h>

namespace Inkscape::UI::Toolbar {

class CommandToolbar final : public Gtk::Box {
public:
    CommandToolbar();
};

} // namespace Inkscape::UI::Toolbar

#endif /* SEEN_TOOLBAR_COMMAND_H */

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
