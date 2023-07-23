// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Tweak aux toolbar
 */
/* Authors:
 *   MenTaLguY <mental@rydia.net>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Frank Felfe <innerspace@iname.com>
 *   John Cliff <simarilius@yahoo.com>
 *   David Turner <novalis@gnu.org>
 *   Josh Andler <scislac@scislac.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Maximilian Albert <maximilian.albert@gmail.com>
 *   Tavmjong Bah <tavmjong@free.fr>
 *   Abhishek Sharma
 *   Kris De Gussem <Kris.DeGussem@gmail.com>
 *   Vaibhav Malik <vaibhavmalik2018@gmail.com>
 *
 * Copyright (C) 2004 David Turner
 * Copyright (C) 2003 MenTaLguY
 * Copyright (C) 1999-2011 authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "tweak-toolbar.h"

#include <glibmm/i18n.h>

#include "desktop.h"
#include "document-undo.h"
#include "ui/icon-names.h"
#include "ui/tools/tweak-tool.h"
#include "ui/widget/canvas.h"
#include "ui/widget/spinbutton.h"

namespace Inkscape {
namespace UI {
namespace Toolbar {

TweakToolbar::TweakToolbar(SPDesktop *desktop)
    : Toolbar(desktop)
    , _builder(initialize_builder("toolbar-tweak.ui"))
{
    auto *prefs = Inkscape::Preferences::get();

    _builder->get_widget("tweak-toolbar", _toolbar);
    if (!_toolbar) {
        std::cerr << "InkscapeWindow: Failed to load tweak toolbar!" << std::endl;
    }

    Gtk::Box *mode_buttons_box;

    _builder->get_widget("mode_buttons_box", mode_buttons_box);

    _builder->get_widget_derived("_width_item", _width_item);
    _builder->get_widget_derived("_force_item", _force_item);

    _builder->get_widget("_pressure_btn", _pressure_btn);

    _builder->get_widget("_fidelity_box", _fidelity_box);
    _builder->get_widget_derived("_fidelity_item", _fidelity_item);

    _builder->get_widget("_channels_box", _channels_box);
    _builder->get_widget("_channels_label", _channels_label);
    _builder->get_widget("_doh_btn", _doh_btn);
    _builder->get_widget("_dos_btn", _dos_btn);
    _builder->get_widget("_dol_btn", _dol_btn);
    _builder->get_widget("_doo_btn", _doo_btn);

    setup_derived_spin_button(_width_item, "width", 15);
    setup_derived_spin_button(_force_item, "force", 20);
    setup_derived_spin_button(_fidelity_item, "fidelity", 50);

    // Configure mode buttons
    int btn_index = 0;

    for (auto child : mode_buttons_box->get_children()) {
        auto btn = dynamic_cast<Gtk::RadioButton *>(child);
        _mode_buttons.push_back(btn);

        btn->signal_clicked().connect(sigc::bind(sigc::mem_fun(*this, &TweakToolbar::mode_changed), btn_index++));
    }

    // Pressure button.
    _pressure_btn->signal_toggled().connect(sigc::mem_fun(*this, &TweakToolbar::pressure_state_changed));
    _pressure_btn->set_active(prefs->getBool("/tools/tweak/usepressure", true));

    // Set initial mode.
    guint mode = prefs->getInt("/tools/tweak/mode", 0);
    _mode_buttons[mode]->set_active();

    // Configure channel buttons.
    // Translators: H, S, L, and O stands for:
    // Hue, Saturation, Lighting and Opacity respectively.
    _doh_btn->signal_toggled().connect(sigc::mem_fun(*this, &TweakToolbar::toggle_doh));
    _doh_btn->set_active(prefs->getBool("/tools/tweak/doh", true));
    _dos_btn->signal_toggled().connect(sigc::mem_fun(*this, &TweakToolbar::toggle_dos));
    _dos_btn->set_active(prefs->getBool("/tools/tweak/dos", true));
    _dol_btn->signal_toggled().connect(sigc::mem_fun(*this, &TweakToolbar::toggle_dol));
    _dol_btn->set_active(prefs->getBool("/tools/tweak/dol", true));
    _doo_btn->signal_toggled().connect(sigc::mem_fun(*this, &TweakToolbar::toggle_doo));
    _doo_btn->set_active(prefs->getBool("/tools/tweak/doo", true));

    // Fetch all the ToolbarMenuButtons at once from the UI file
    // Menu Button #1
    Gtk::Box *popover_box1;
    _builder->get_widget("popover_box1", popover_box1);

    Inkscape::UI::Widget::ToolbarMenuButton *menu_btn1 = nullptr;
    _builder->get_widget_derived("menu_btn1", menu_btn1);

    // Menu Button #2
    Gtk::Box *popover_box2;
    _builder->get_widget("popover_box2", popover_box2);

    Inkscape::UI::Widget::ToolbarMenuButton *menu_btn2 = nullptr;
    _builder->get_widget_derived("menu_btn2", menu_btn2);

    // Initialize all the ToolbarMenuButtons.
    // Note: Do not initialize the these widgets right after fetching from
    // the UI file.
    auto children = _toolbar->get_children();

    menu_btn1->init(1, "tag1", "some-icon", popover_box1, children);
    _expanded_menu_btns.push(menu_btn1);
    menu_btn2->init(2, "tag2", "some-icon", popover_box2, children);
    _expanded_menu_btns.push(menu_btn2);

    add(*_toolbar);

    show_all();

    // Elements must be hidden after show_all() is called
    if (mode == Inkscape::UI::Tools::TWEAK_MODE_COLORPAINT || mode == Inkscape::UI::Tools::TWEAK_MODE_COLORJITTER) {
        _fidelity_box->set_visible(false);
    } else {
        _channels_box->set_visible(false);
    }
}

void TweakToolbar::setup_derived_spin_button(UI::Widget::SpinButton *btn, const Glib::ustring &name,
                                             double default_value)
{
    auto *prefs = Inkscape::Preferences::get();
    const Glib::ustring path = "/tools/tweak/" + name;
    auto val = prefs->getDouble(path, default_value);

    auto adj = btn->get_adjustment();
    adj->set_value(val);

    if (name == "width") {
        adj->signal_value_changed().connect(sigc::mem_fun(*this, &TweakToolbar::width_value_changed));
    } else if (name == "force") {
        adj->signal_value_changed().connect(sigc::mem_fun(*this, &TweakToolbar::force_value_changed));
    } else if (name == "fidelity") {
        adj->signal_value_changed().connect(sigc::mem_fun(*this, &TweakToolbar::fidelity_value_changed));
    }

    btn->set_defocus_widget(_desktop->getCanvas());
}

void TweakToolbar::set_mode(int mode)
{
    _mode_buttons[mode]->set_active();
}

GtkWidget *TweakToolbar::create(SPDesktop *desktop)
{
    auto toolbar = new TweakToolbar(desktop);
    return toolbar->Gtk::Widget::gobj();
}

void TweakToolbar::width_value_changed()
{
    auto prefs = Inkscape::Preferences::get();
    prefs->setDouble("/tools/tweak/width", _width_item->get_adjustment()->get_value() * 0.01);
}

void TweakToolbar::force_value_changed()
{
    auto prefs = Inkscape::Preferences::get();
    prefs->setDouble("/tools/tweak/force", _force_item->get_adjustment()->get_value() * 0.01);
}

void TweakToolbar::mode_changed(int mode)
{
    auto prefs = Inkscape::Preferences::get();
    prefs->setInt("/tools/tweak/mode", mode);

    bool flag = ((mode == Inkscape::UI::Tools::TWEAK_MODE_COLORPAINT) ||
                 (mode == Inkscape::UI::Tools::TWEAK_MODE_COLORJITTER));

    _channels_box->set_visible(flag);

    if (_fidelity_box) {
        _fidelity_box->set_visible(!flag);
    }
}

void TweakToolbar::fidelity_value_changed()
{
    auto prefs = Inkscape::Preferences::get();
    prefs->setDouble("/tools/tweak/fidelity", _fidelity_item->get_adjustment()->get_value() * 0.01);
}

void TweakToolbar::pressure_state_changed()
{
    auto prefs = Inkscape::Preferences::get();
    prefs->setBool("/tools/tweak/usepressure", _pressure_btn->get_active());
}

void TweakToolbar::toggle_doh()
{
    auto prefs = Inkscape::Preferences::get();
    prefs->setBool("/tools/tweak/doh", _doh_btn->get_active());
}

void TweakToolbar::toggle_dos()
{
    auto prefs = Inkscape::Preferences::get();
    prefs->setBool("/tools/tweak/dos", _dos_btn->get_active());
}

void TweakToolbar::toggle_dol()
{
    auto prefs = Inkscape::Preferences::get();
    prefs->setBool("/tools/tweak/dol", _dol_btn->get_active());
}

void TweakToolbar::toggle_doo()
{
    auto prefs = Inkscape::Preferences::get();
    prefs->setBool("/tools/tweak/doo", _doo_btn->get_active());
}
}
}
}

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
