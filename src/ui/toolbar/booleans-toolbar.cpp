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

BooleansToolbar::BooleansToolbar(SPDesktop *desktop)
    : Toolbar(desktop)
    , _builder(initialize_builder("toolbar-booleans.ui"))
    , _adj_opacity(get_object<Gtk::Adjustment>(_builder, "opacity-adj"))
{
    _builder->get_widget("booleans-toolbar", _toolbar);
    if (!_toolbar) {
        std::cerr << "InkscapeWindow: Failed to load booleans toolbar!" << std::endl;
    }

    _builder->get_widget("_shape_add", _btn_shape_add);
    _builder->get_widget("_shape_delete", _btn_shape_delete);
    _builder->get_widget("_confirm", _btn_confirm);
    _builder->get_widget("_cancel", _btn_cancel);

    add(*_toolbar);

    _mode_buttons.push_back(_btn_shape_add);
    _mode_buttons.push_back(_btn_shape_delete);

    int btn_index = 0;

    for (auto btn : _mode_buttons) {
        btn->signal_clicked().connect(sigc::bind(sigc::mem_fun(*this, &BooleansToolbar::mode_changed), btn_index++));
    }

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

void BooleansToolbar::mode_changed(int mode)
{
    // Set the other button inactive.
    int index = (mode + 1) % 2;

    if (_mode_buttons[index]->get_active()) {
        _mode_buttons[index]->set_active(false);
    }
}

GtkWidget *
BooleansToolbar::create(SPDesktop *desktop)
{
    auto toolbar = new BooleansToolbar(desktop);
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
