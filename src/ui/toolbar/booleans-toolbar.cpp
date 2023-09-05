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

#include "ui/toolbar/booleans-toolbar.h"

#include <gtkmm/adjustment.h>
#include <gtkmm/builder.h>
#include <gtkmm/toolbutton.h>

#include "desktop.h"
#include "ui/builder-utils.h"
#include "ui/tools/booleans-tool.h"

namespace Inkscape::UI::Toolbar {

BooleansToolbar::BooleansToolbar(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder> &builder, SPDesktop *desktop)
    : Gtk::Toolbar(cobject)
    , _builder(builder)
    , _adj_opacity(get_object<Gtk::Adjustment>(_builder, "opacity-adj"))
    , _btn_confirm(get_widget<Gtk::ToolButton>(builder, "confirm"))
    , _btn_cancel(get_widget<Gtk::ToolButton>(builder, "cancel"))
{
    _btn_confirm.signal_clicked().connect([=]{
        auto const tool = dynamic_cast<Tools::InteractiveBooleansTool *>(desktop->getTool());
        tool->shape_commit();
    });
    _btn_cancel.signal_clicked().connect([=]{
        auto const tool = dynamic_cast<Tools::InteractiveBooleansTool *>(desktop->getTool());
        tool->shape_cancel();
    });

    auto prefs = Inkscape::Preferences::get();
    _adj_opacity->set_value(prefs->getDouble("/tools/booleans/opacity", 0.5) * 100);
    _adj_opacity->signal_value_changed().connect([=](){
        auto const tool = dynamic_cast<Tools::InteractiveBooleansTool *>(desktop->getTool());
        double value = (double)_adj_opacity->get_value() / 100;
        prefs->setDouble("/tools/booleans/opacity", value);
        tool->set_opacity(value);
    });
}

BooleansToolbar::~BooleansToolbar() = default;

void BooleansToolbar::on_parent_changed(Gtk::Widget *) {
    _builder.reset();
}

GtkWidget *
BooleansToolbar::create(SPDesktop *desktop)
{
    BooleansToolbar *toolbar;
    auto builder = Inkscape::UI::create_builder("toolbar-booleans.ui");
    builder->get_widget_derived("booleans-toolbar", toolbar, desktop);
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
// vim:filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99:
