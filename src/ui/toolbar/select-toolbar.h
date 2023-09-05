// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * Selector aux toolbar
 */
/*
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <bulia@dr.com>
 *
 * Copyright (C) 2003 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_SELECT_TOOLBAR_H
#define SEEN_SELECT_TOOLBAR_H

#include <memory>
#include <string>
#include <vector>
#include <glibmm/refptr.h>

#include "toolbar.h"
#include "helper/auto-connection.h"

namespace Gtk {
class Adjustment;
class ToggleToolButton;
class ToolItem;
} // namespace Gtk

class SPDesktop;

namespace Inkscape {

class Selection;

namespace UI {

namespace Widget {
class UnitTracker;
} // namespace Widget

namespace Toolbar {

class SelectToolbar final : public Toolbar {
    using parent_type = Toolbar;

private:
    std::unique_ptr<UI::Widget::UnitTracker> _tracker;

    Glib::RefPtr<Gtk::Adjustment>  _adj_x;
    Glib::RefPtr<Gtk::Adjustment>  _adj_y;
    Glib::RefPtr<Gtk::Adjustment>  _adj_w;
    Glib::RefPtr<Gtk::Adjustment>  _adj_h;
    Gtk::ToggleToolButton         *_lock_btn;
    Gtk::ToggleToolButton         *_select_touch_btn;
    Gtk::ToggleToolButton         *_transform_stroke_btn;
    Gtk::ToggleToolButton         *_transform_corners_btn;
    Gtk::ToggleToolButton         *_transform_gradient_btn;
    Gtk::ToggleToolButton         *_transform_pattern_btn;

    std::vector<Gtk::ToolItem *> _context_items;
    std::vector<auto_connection> _connections;

    bool _update;
    std::string _action_key;
    std::string const _action_prefix;

    char const *get_action_key(double mh, double sh, double mv, double sv);
    void any_value_changed(Glib::RefPtr<Gtk::Adjustment>& adj);
    void layout_widget_update(Inkscape::Selection *sel);
    void on_inkscape_selection_modified(Inkscape::Selection *selection, unsigned flags);
    void on_inkscape_selection_changed (Inkscape::Selection *selection);
    void toggle_lock();
    void toggle_touch();
    void toggle_stroke();
    void toggle_corners();
    void toggle_gradient();
    void toggle_pattern();

protected:
    SelectToolbar(SPDesktop *desktop);

    void on_unrealize() final;

public:
    static GtkWidget * create(SPDesktop *desktop);
};

} // namespace Widget
} // namespace UI
} // namespace Inkscape

#endif /* !SEEN_SELECT_TOOLBAR_H */

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
