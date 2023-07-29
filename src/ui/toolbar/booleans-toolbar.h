// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * A toolbar for the Builder tool.
 *
 * Authors:
 *   Martin Owens
 *
 * Copyright (C) 2022 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_TOOLBAR_BOOLEANS_TOOLBAR_H
#define INKSCAPE_UI_TOOLBAR_BOOLEANS_TOOLBAR_H

#include <glibmm/refptr.h>
#include <gtkmm/toolbar.h>

namespace Gtk {
class Adjustment;
class Builder;
class ToolButton;
class Widget;
} // namespace Gtk

#include "toolbar.h"

class SPDesktop;

namespace Inkscape::UI::Toolbar {

class BooleansToolbar : public Toolbar
{
public:
    static GtkWidget *create(SPDesktop *desktop);

    BooleansToolbar(SPDesktop *desktop);
    ~BooleansToolbar() final;

    void on_parent_changed(Gtk::Widget *) final;
    void mode_changed(int mode);

private:
    Glib::RefPtr<Gtk::Builder> _builder;
    Glib::RefPtr<Gtk::Adjustment> _adj_opacity;

    Gtk::ToggleButton *_btn_shape_add;
    Gtk::ToggleButton *_btn_shape_delete;

    Gtk::Button *_btn_confirm;
    Gtk::Button *_btn_cancel;
};

} // namespace Inkscape::UI::Toolbar

#endif // INKSCAPE_UI_TOOLBAR_BOOLEANS_TOOLBAR_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim:filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99:
