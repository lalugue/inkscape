// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Measure aux toolbar
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

#include "measure-toolbar.h"

#include <glibmm/i18n.h>

#include <gtkmm/separatortoolitem.h>

#include "desktop.h"
#include "document-undo.h"
#include "message-stack.h"
#include "object/sp-namedview.h"

#include "ui/icon-names.h"
#include "ui/tools/measure-tool.h"
#include "ui/widget/canvas.h"
#include "ui/widget/combo-tool-item.h"
// #include "ui/widget/label-tool-item.h"
#include "ui/widget/spinbutton.h"
#include "ui/widget/unit-tracker.h"

using Inkscape::UI::Widget::UnitTracker;
using Inkscape::Util::Unit;
using Inkscape::DocumentUndo;
using Inkscape::UI::Tools::MeasureTool;

static MeasureTool *get_measure_tool(SPDesktop *desktop)
{
    if (desktop) {
        return dynamic_cast<MeasureTool *>(desktop->getTool());
    }
    return nullptr;
}



namespace Inkscape {
namespace UI {
namespace Toolbar {

MeasureToolbar::MeasureToolbar(SPDesktop *desktop)
    : Toolbar(desktop)
    , _tracker(new UnitTracker(Inkscape::Util::UNIT_TYPE_LINEAR))
    , _builder(initialize_builder("toolbar-measure.ui"))
{
    auto prefs = Inkscape::Preferences::get();
    auto unit = desktop->getNamedView()->getDisplayUnit();
    _tracker->setActiveUnitByAbbr(prefs->getString("/tools/measure/unit", unit->abbr).c_str());

    _builder->get_widget("measure-toolbar", _toolbar);
    if (!_toolbar) {
        std::cerr << "InkscapeWindow: Failed to load measure toolbar!" << std::endl;
    }

    Gtk::Box *unit_menu_box;

    // _builder->get_widget("_mode_item", _mode_item);

    _builder->get_widget_derived("_font_size_item", _font_size_item);
    _builder->get_widget_derived("_precision_item", _precision_item);
    _builder->get_widget_derived("_scale_item", _scale_item);

    _builder->get_widget("unit_menu_box", unit_menu_box);

    _builder->get_widget("_only_selected_item", _only_selected_item);
    _builder->get_widget("_ignore_1st_and_last_item", _ignore_1st_and_last_item);
    _builder->get_widget("_inbetween_item", _inbetween_item);
    _builder->get_widget("_show_hidden_item", _show_hidden_item);
    _builder->get_widget("_all_layers_item", _all_layers_item);

    _builder->get_widget("_reverse_item", _reverse_item);
    _builder->get_widget("_to_phantom_item", _to_phantom_item);
    _builder->get_widget("_to_guides_item", _to_guides_item);
    _builder->get_widget("_to_item_item", _to_item_item);
    _builder->get_widget("_mark_dimension_item", _mark_dimension_item);

    _builder->get_widget_derived("_offset_item", _offset_item);

    auto unit_menu = _tracker->create_tool_item(_("Units"), (""));
    unit_menu->signal_changed().connect(sigc::mem_fun(*this, &MeasureToolbar::unit_changed));
    unit_menu_box->add(*unit_menu);

    setup_derived_spin_button(_font_size_item, "fontsize", 10.0);
    setup_derived_spin_button(_precision_item, "precision", 2);
    setup_derived_spin_button(_scale_item, "scale", 100.0);
    setup_derived_spin_button(_offset_item, "offset", 5.0);

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

    // Initialize all the ToolbarMenuButtons only after all the children of the
    // toolbar have been fetched. Otherwise, the children to be moved in the
    // popover will get mapped to a different position and it will probably
    // cause segfault.
    auto children = _toolbar->get_children();

    menu_btn1->init(1, "tag1", "some-icon", popover_box1, children);
    menu_btn2->init(2, "tag2", "some-icon", popover_box2, children);
    _expanded_menu_btns.push(menu_btn1);
    _expanded_menu_btns.push(menu_btn2);

    // Signals.
    _only_selected_item->set_active(prefs->getBool("/tools/measure/only_selected", false));
    _only_selected_item->signal_toggled().connect(sigc::mem_fun(*this, &MeasureToolbar::toggle_only_selected));

    _ignore_1st_and_last_item->set_active(prefs->getBool("/tools/measure/ignore_1st_and_last", true));
    _ignore_1st_and_last_item->signal_toggled().connect(
        sigc::mem_fun(*this, &MeasureToolbar::toggle_ignore_1st_and_last));

    _inbetween_item->set_active(prefs->getBool("/tools/measure/show_in_between", true));
    _inbetween_item->signal_toggled().connect(sigc::mem_fun(*this, &MeasureToolbar::toggle_show_in_between));

    _show_hidden_item->set_active(prefs->getBool("/tools/measure/show_hidden", true));
    _show_hidden_item->signal_toggled().connect(sigc::mem_fun(*this, &MeasureToolbar::toggle_show_hidden));

    _all_layers_item->set_active(prefs->getBool("/tools/measure/all_layers", true));
    _all_layers_item->signal_toggled().connect(sigc::mem_fun(*this, &MeasureToolbar::toggle_all_layers));

    _reverse_item->signal_clicked().connect(sigc::mem_fun(*this, &MeasureToolbar::reverse_knots));

    _to_phantom_item->signal_clicked().connect(sigc::mem_fun(*this, &MeasureToolbar::to_phantom));

    _to_guides_item->signal_clicked().connect(sigc::mem_fun(*this, &MeasureToolbar::to_guides));

    _to_item_item->signal_clicked().connect(sigc::mem_fun(*this, &MeasureToolbar::to_item));

    _mark_dimension_item->signal_clicked().connect(sigc::mem_fun(*this, &MeasureToolbar::to_mark_dimension));

    add(*_toolbar);

    show_all();
}

void MeasureToolbar::setup_derived_spin_button(UI::Widget::SpinButton *btn, const Glib::ustring &name,
                                               double default_value)
{
    auto prefs = Inkscape::Preferences::get();
    auto adj = btn->get_adjustment();
    const Glib::ustring path = "/tools/measure/" + name;
    auto val = prefs->getDouble(path, default_value);
    adj->set_value(val);

    if (name == "fontsize") {
        adj->signal_value_changed().connect(sigc::mem_fun(*this, &MeasureToolbar::fontsize_value_changed));
    } else if (name == "precision") {
        adj->signal_value_changed().connect(sigc::mem_fun(*this, &MeasureToolbar::precision_value_changed));
    } else if (name == "scale") {
        adj->signal_value_changed().connect(sigc::mem_fun(*this, &MeasureToolbar::scale_value_changed));
    } else if (name == "offset") {
        adj->signal_value_changed().connect(sigc::mem_fun(*this, &MeasureToolbar::offset_value_changed));
    }

    btn->set_defocus_widget(_desktop->getCanvas());
}

GtkWidget *MeasureToolbar::create(SPDesktop *desktop)
{
    auto toolbar = new MeasureToolbar(desktop);
    return toolbar->Gtk::Widget::gobj();
} // MeasureToolbar::prep()

void MeasureToolbar::fontsize_value_changed()
{
    if (DocumentUndo::getUndoSensitive(_desktop->getDocument())) {
        auto prefs = Inkscape::Preferences::get();
        prefs->setDouble(Glib::ustring("/tools/measure/fontsize"), _font_size_item->get_adjustment()->get_value());
        MeasureTool *mt = get_measure_tool(_desktop);
        if (mt) {
            mt->showCanvasItems();
        }
    }
}

void MeasureToolbar::unit_changed(int /* notUsed */)
{
    Glib::ustring const unit = _tracker->getActiveUnit()->abbr;
    auto prefs = Inkscape::Preferences::get();
    prefs->setString("/tools/measure/unit", unit);
    MeasureTool *mt = get_measure_tool(_desktop);
    if (mt) {
        mt->showCanvasItems();
    }
}

void MeasureToolbar::precision_value_changed()
{
    if (DocumentUndo::getUndoSensitive(_desktop->getDocument())) {
        auto prefs = Inkscape::Preferences::get();
        prefs->setInt(Glib::ustring("/tools/measure/precision"), _precision_item->get_adjustment()->get_value());
        MeasureTool *mt = get_measure_tool(_desktop);
        if (mt) {
            mt->showCanvasItems();
        }
    }
}

void MeasureToolbar::scale_value_changed()
{
    if (DocumentUndo::getUndoSensitive(_desktop->getDocument())) {
        auto prefs = Inkscape::Preferences::get();
        prefs->setDouble(Glib::ustring("/tools/measure/scale"), _scale_item->get_adjustment()->get_value());
        MeasureTool *mt = get_measure_tool(_desktop);
        if (mt) {
            mt->showCanvasItems();
        }
    }
}

void MeasureToolbar::offset_value_changed()
{
    if (DocumentUndo::getUndoSensitive(_desktop->getDocument())) {
        auto prefs = Inkscape::Preferences::get();
        prefs->setDouble(Glib::ustring("/tools/measure/offset"), _offset_item->get_adjustment()->get_value());
        MeasureTool *mt = get_measure_tool(_desktop);
        if (mt) {
            mt->showCanvasItems();
        }
    }
}

void MeasureToolbar::toggle_only_selected()
{
    auto prefs = Inkscape::Preferences::get();
    bool active = _only_selected_item->get_active();
    prefs->setBool("/tools/measure/only_selected", active);
    if ( active ) {
        _desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Measures only selected."));
    } else {
        _desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Measure all."));
    }
    MeasureTool *mt = get_measure_tool(_desktop);
    if (mt) {
        mt->showCanvasItems();
    }
}

void MeasureToolbar::toggle_ignore_1st_and_last()
{
    auto prefs = Inkscape::Preferences::get();
    bool active = _ignore_1st_and_last_item->get_active();
    prefs->setBool("/tools/measure/ignore_1st_and_last", active);
    if ( active ) {
        _desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Start and end measures inactive."));
    } else {
        _desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Start and end measures active."));
    }
    MeasureTool *mt = get_measure_tool(_desktop);
    if (mt) {
        mt->showCanvasItems();
    }
}

void MeasureToolbar::toggle_show_in_between()
{
    auto prefs = Inkscape::Preferences::get();
    bool active = _inbetween_item->get_active();
    prefs->setBool("/tools/measure/show_in_between", active);
    if ( active ) {
        _desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Compute all elements."));
    } else {
        _desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Compute max length."));
    }
    MeasureTool *mt = get_measure_tool(_desktop);
    if (mt) {
        mt->showCanvasItems();
    }
}

void MeasureToolbar::toggle_show_hidden()
{
    auto prefs = Inkscape::Preferences::get();
    bool active = _show_hidden_item->get_active();
    prefs->setBool("/tools/measure/show_hidden", active);
    if ( active ) {
        _desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Show all crossings."));
    } else {
        _desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Show visible crossings."));
    }
    MeasureTool *mt = get_measure_tool(_desktop);
    if (mt) {
        mt->showCanvasItems();
    }
}

void MeasureToolbar::toggle_all_layers()
{
    auto prefs = Inkscape::Preferences::get();
    bool active = _all_layers_item->get_active();
    prefs->setBool("/tools/measure/all_layers", active);
    if ( active ) {
        _desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Use all layers in the measure."));
    } else {
        _desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Use current layer in the measure."));
    }
    MeasureTool *mt = get_measure_tool(_desktop);
    if (mt) {
        mt->showCanvasItems();
    }
}

void MeasureToolbar::reverse_knots()
{
    MeasureTool *mt = get_measure_tool(_desktop);
    if (mt) {
        mt->reverseKnots();
    }
}

void MeasureToolbar::to_phantom()
{
    MeasureTool *mt = get_measure_tool(_desktop);
    if (mt) {
        mt->toPhantom();
    }
}

void MeasureToolbar::to_guides()
{
    MeasureTool *mt = get_measure_tool(_desktop);
    if (mt) {
        mt->toGuides();
    }
}

void MeasureToolbar::to_item()
{
    MeasureTool *mt = get_measure_tool(_desktop);
    if (mt) {
        mt->toItem();
    }
}

void MeasureToolbar::to_mark_dimension()
{
    MeasureTool *mt = get_measure_tool(_desktop);
    if (mt) {
        mt->toMarkDimension();
    }
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
