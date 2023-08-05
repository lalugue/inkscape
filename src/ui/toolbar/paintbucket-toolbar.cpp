// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Paint bucket aux toolbar
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
 *
 * Copyright (C) 2004 David Turner
 * Copyright (C) 2003 MenTaLguY
 * Copyright (C) 1999-2011 authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "paintbucket-toolbar.h"

#include <glibmm/i18n.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treemodel.h>

#include "desktop.h"
#include "preferences.h"
#include "ui/icon-names.h"
#include "ui/tools/flood-tool.h"
#include "ui/widget/canvas.h"
#include "ui/widget/combo-tool-item.h"
#include "ui/widget/spinbutton.h"
#include "ui/widget/unit-tracker.h"

using Inkscape::Util::unit_table;

namespace Inkscape::UI::Toolbar {

PaintbucketToolbar::PaintbucketToolbar(SPDesktop *desktop)
    : Toolbar(desktop)
    , _tracker{std::make_unique<UI::Widget::UnitTracker>(Inkscape::Util::UNIT_TYPE_LINEAR)}
    , _builder(initialize_builder("toolbar-paintbucket.ui"))
{
    auto *prefs = Inkscape::Preferences::get();

    _builder->get_widget("paintbucket-toolbar", _toolbar);
    if (!_toolbar) {
        std::cerr << "InkscapeWindow: Failed to load paintbucket toolbar!" << std::endl;
    }

    Gtk::Box *channels_box;
    Gtk::Box *units_menu_box;
    Gtk::Box *autogap_box;

    Gtk::Button *reset_btn;

    _builder->get_widget("channels_box", channels_box);

    _builder->get_widget_derived("_threshold_item", _threshold_item);
    _builder->get_widget_derived("_offset_item", _offset_item);
    _builder->get_widget("units_menu_box", units_menu_box);

    _builder->get_widget("autogap_box", autogap_box);
    _builder->get_widget("reset_btn", reset_btn);

    // Setup the spin buttons.
    setup_derived_spin_button(_threshold_item, "threshold", 5);
    setup_derived_spin_button(_offset_item, "offset", 0);

    // Channel
    {
        UI::Widget::ComboToolItemColumns columns;

        Glib::RefPtr<Gtk::ListStore> store = Gtk::ListStore::create(columns);

        for (auto item: Inkscape::UI::Tools::FloodTool::channel_list) {
            Gtk::TreeModel::Row row = *(store->append());
            row[columns.col_label    ] = _(item);
            row[columns.col_sensitive] = true;
        }

        _channels_item = Gtk::manage(UI::Widget::ComboToolItem::create(_("Fill by"), Glib::ustring(), "Not Used", store));
        _channels_item->use_group_label(true);

        int channels = prefs->getInt("/tools/paintbucket/channels", 0);
        _channels_item->set_active(channels);

        _channels_item->signal_changed().connect(sigc::mem_fun(*this, &PaintbucketToolbar::channels_changed));
        channels_box->add(*_channels_item);

        // Create the units menu.
        Glib::ustring stored_unit = prefs->getString("/tools/paintbucket/offsetunits");
        if (!stored_unit.empty()) {
            Unit const *u = unit_table.getUnit(stored_unit);
            _tracker->setActiveUnit(u);
        }
    }

    // Auto Gap
    {
        UI::Widget::ComboToolItemColumns columns;

        Glib::RefPtr<Gtk::ListStore> store = Gtk::ListStore::create(columns);

        for (auto item: Inkscape::UI::Tools::FloodTool::gap_list) {
            Gtk::TreeModel::Row row = *(store->append());
            row[columns.col_label    ] = g_dpgettext2(nullptr, "Flood autogap", item);
            row[columns.col_sensitive] = true;
        }

        _autogap_item = Gtk::manage(UI::Widget::ComboToolItem::create(_("Close gaps"), Glib::ustring(), "Not Used", store));
        _autogap_item->use_group_label(true);

        int autogap = prefs->getInt("/tools/paintbucket/autogap", 0);
        _autogap_item->set_active(autogap);

        _autogap_item->signal_changed().connect(sigc::mem_fun(*this, &PaintbucketToolbar::autogap_changed));
        autogap_box->add(*_autogap_item);

        auto units_menu = _tracker->create_tool_item(_("Units"), (""));
        units_menu_box->add(*units_menu);
    }

    // Fetch all the ToolbarMenuButtons at once from the UI file
    // Menu Button #1
    Gtk::Box *popover_box1;
    _builder->get_widget("popover_box1", popover_box1);

    Inkscape::UI::Widget::ToolbarMenuButton *menu_btn1 = nullptr;
    _builder->get_widget_derived("menu_btn1", menu_btn1);

    // Initialize all the ToolbarMenuButtons only after all the children of the
    // toolbar have been fetched. Otherwise, the children to be moved in the
    // popover will get mapped to a different position and it will probably
    // cause segfault.
    auto children = _toolbar->get_children();

    menu_btn1->init(1, "tag1", "some-icon", popover_box1, children);
    _expanded_menu_btns.push(menu_btn1);

    add(*_toolbar);

    // Signals.
    reset_btn->signal_clicked().connect(sigc::mem_fun(*this, &PaintbucketToolbar::defaults));

    show_all();
}

void PaintbucketToolbar::setup_derived_spin_button(UI::Widget::SpinButton *btn, Glib::ustring const &name,
                                                   double default_value)
{
    auto *prefs = Inkscape::Preferences::get();
    const Glib::ustring path = "/tools/painbucket/" + name;
    auto val = prefs->getDouble(path, default_value);

    auto adj = btn->get_adjustment();
    adj->set_value(val);

    if (name == "threshold") {
        adj->signal_value_changed().connect(sigc::mem_fun(*this, &PaintbucketToolbar::threshold_changed));
    } else if (name == "offset") {
        adj->signal_value_changed().connect(sigc::mem_fun(*this, &PaintbucketToolbar::offset_changed));
        _tracker->addAdjustment(adj->gobj());
        btn->addUnitTracker(_tracker.get());
    }

    btn->set_defocus_widget(_desktop->getCanvas());
}

GtkWidget *PaintbucketToolbar::create(SPDesktop *desktop)
{
    auto toolbar = new PaintbucketToolbar(desktop);
    return toolbar->Gtk::Widget::gobj();
}

void PaintbucketToolbar::channels_changed(int channels)
{
    Inkscape::UI::Tools::FloodTool::set_channels(channels);
}

void PaintbucketToolbar::threshold_changed()
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setInt("/tools/paintbucket/threshold", (gint)_threshold_item->get_adjustment()->get_value());
}

void PaintbucketToolbar::offset_changed()
{
    Unit const *unit = _tracker->getActiveUnit();
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    // Don't adjust the offset value because we're saving the
    // unit and it'll be correctly handled on load.
    prefs->setDouble("/tools/paintbucket/offset", (gdouble)_offset_item->get_adjustment()->get_value());

    g_return_if_fail(unit != nullptr);
    prefs->setString("/tools/paintbucket/offsetunits", unit->abbr);
}

void PaintbucketToolbar::autogap_changed(int autogap)
{
    auto prefs = Inkscape::Preferences::get();
    prefs->setInt("/tools/paintbucket/autogap", autogap);
}

void PaintbucketToolbar::defaults()
{
    // FIXME: make defaults settable via Inkscape Options
    _threshold_item->get_adjustment()->set_value(15);
    _offset_item->get_adjustment()->set_value(0.0);

    _channels_item->set_active(Inkscape::UI::Tools::FLOOD_CHANNELS_RGB);
    _autogap_item->set_active(0);
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
