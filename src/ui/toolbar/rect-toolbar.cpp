// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Rect aux toolbar
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

#include "rect-toolbar.h"

#include <glibmm/i18n.h>
#include <gtkmm/separatortoolitem.h>
#include <gtkmm/toolbutton.h>

#include "desktop.h"
#include "document-undo.h"
#include "object/sp-namedview.h"
#include "object/sp-rect.h"
#include "selection.h"
#include "ui/builder-utils.h"
#include "ui/icon-names.h"
#include "ui/tools/rect-tool.h"
#include "ui/widget/canvas.h"
#include "ui/widget/combo-tool-item.h"
#include "ui/widget/spinbutton.h"
#include "ui/widget/unit-tracker.h"
#include "widgets/widget-sizes.h"

using Inkscape::UI::Widget::UnitTracker;
using Inkscape::DocumentUndo;
using Inkscape::Util::Unit;
using Inkscape::Util::Quantity;
using Inkscape::Util::unit_table;

namespace Inkscape::UI::Toolbar {

RectToolbar::RectToolbar(SPDesktop *desktop)
    : Toolbar(desktop)
    , _tracker(new UnitTracker(Inkscape::Util::UNIT_TYPE_LINEAR))
    , _builder(initialize_builder("toolbar-rect.ui"))
{
    _builder->get_widget("rect-toolbar", _toolbar);
    if (!_toolbar) {
        std::cerr << "InkscapeWindow: Failed to load rect toolbar!" << std::endl;
    }

    Gtk::Box *unit_menu_box;

    _builder->get_widget("_mode_item", _mode_item);

    _builder->get_widget_derived("_width_item", _width_item);
    _builder->get_widget_derived("_height_item", _height_item);
    _builder->get_widget_derived("_rx_item", _rx_item);
    _builder->get_widget_derived("_ry_item", _ry_item);

    _builder->get_widget("unit_menu_box", unit_menu_box);
    _builder->get_widget("_not_rounded", _not_rounded);

    auto unit_menu = _tracker->create_tool_item(_("Units"), (""));
    unit_menu_box->add(*unit_menu);

    // rx/ry units menu: create
    //tracker->addUnit( SP_UNIT_PERCENT, 0 );
    // fixme: add % meaning per cent of the width/height
    auto init_units = desktop->getNamedView()->display_units;
    _tracker->setActiveUnit(init_units);

    setup_derived_spin_button(_width_item, "width");
    setup_derived_spin_button(_height_item, "height");
    setup_derived_spin_button(_rx_item, "rx");
    setup_derived_spin_button(_ry_item, "ry");

    // Fetch all the ToolbarMenuButtons at once from the UI file
    // Menu Button #1
    Gtk::Box *popover_box1;
    _builder->get_widget("popover_box1", popover_box1);

    Inkscape::UI::Widget::ToolbarMenuButton *menu_btn1 = nullptr;
    _builder->get_widget_derived("menu_btn1", menu_btn1);

    // Initialize all the ToolbarMenuButtons.
    // Note: Do not initialize the these widgets right after fetching from
    // the UI file.
    auto children = _toolbar->get_children();

    menu_btn1->init(1, "tag1", "some-icon", popover_box1, children);
    _expanded_menu_btns.push(menu_btn1);

    _not_rounded->signal_clicked().connect(sigc::mem_fun(*this, &RectToolbar::defaults));
    _desktop->connectEventContextChanged(sigc::mem_fun(*this, &RectToolbar::watch_ec));

    add(*_toolbar);

    sensitivize();
    show_all();
}

void RectToolbar::setup_derived_spin_button(UI::Widget::SpinButton *btn, const Glib::ustring &name)
{
    auto init_units = _desktop->getNamedView()->display_units;
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    auto adj = btn->get_adjustment();
    const Glib::ustring path = "/tools/shapes/rect/" + name;
    auto val = prefs->getDouble(path, 0);
    val = Quantity::convert(val, "px", init_units);
    adj->set_value(val);

    if (name == "width") {
        adj->signal_value_changed().connect(
            sigc::bind(sigc::mem_fun(*this, &RectToolbar::value_changed), adj, "width", &SPRect::setVisibleWidth));
    } else if (name == "height") {
        adj->signal_value_changed().connect(
            sigc::bind(sigc::mem_fun(*this, &RectToolbar::value_changed), adj, "height", &SPRect::setVisibleHeight));
    } else if (name == "rx") {
        adj->signal_value_changed().connect(
            sigc::bind(sigc::mem_fun(*this, &RectToolbar::value_changed), adj, "rx", &SPRect::setVisibleRx));
    } else if (name == "ry") {
        adj->signal_value_changed().connect(
            sigc::bind(sigc::mem_fun(*this, &RectToolbar::value_changed), adj, "ry", &SPRect::setVisibleRy));
    }

    _tracker->addAdjustment(adj->gobj());

    btn->addUnitTracker(_tracker.get());
    btn->set_defocus_widget(_desktop->getCanvas());
}

RectToolbar::~RectToolbar()
{
    if (_repr) { // remove old listener
        _repr->removeObserver(*this);
        Inkscape::GC::release(_repr);
        _repr = nullptr;
    }
    _changed.disconnect();
}

GtkWidget *
RectToolbar::create(SPDesktop *desktop)
{
    auto toolbar = new RectToolbar(desktop);
    return toolbar->Gtk::Widget::gobj();
}

void
RectToolbar::value_changed(Glib::RefPtr<Gtk::Adjustment>&  adj,
                           gchar const                    *value_name,
                           void (SPRect::*setter)(gdouble))
{
    Unit const *unit = _tracker->getActiveUnit();
    g_return_if_fail(unit != nullptr);

    if (DocumentUndo::getUndoSensitive(_desktop->getDocument())) {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        prefs->setDouble(Glib::ustring("/tools/shapes/rect/") + value_name,
            Quantity::convert(adj->get_value(), unit, "px"));
    }

    // quit if run by the attr_changed listener
    if (_freeze || _tracker->isUpdating()) {
        return;
    }

    // in turn, prevent listener from responding
    _freeze = true;

    bool modmade = false;
    Inkscape::Selection *selection = _desktop->getSelection();
    auto itemlist= selection->items();
    for(auto i=itemlist.begin();i!=itemlist.end();++i){
        if (is<SPRect>(*i)) {
            if (adj->get_value() != 0) {
                (cast<SPRect>(*i)->*setter)(Quantity::convert(adj->get_value(), unit, "px"));
            } else {
                (*i)->removeAttribute(value_name);
            }
            modmade = true;
        }
    }

    sensitivize();

    if (modmade) {
        DocumentUndo::done(_desktop->getDocument(), _("Change rectangle"), INKSCAPE_ICON("draw-rectangle"));
    }

    _freeze = false;
}

void
RectToolbar::sensitivize()
{
    if (_rx_item->get_adjustment()->get_value() == 0 && _ry_item->get_adjustment()->get_value() == 0 &&
        _single) { // only for a single selected rect (for now)
        _not_rounded->set_sensitive(false);
    } else {
        _not_rounded->set_sensitive(true);
    }
}

void
RectToolbar::defaults()
{
    _rx_item->get_adjustment()->set_value(0.0);
    _ry_item->get_adjustment()->set_value(0.0);

    sensitivize();
}

void
RectToolbar::watch_ec(SPDesktop* desktop, Inkscape::UI::Tools::ToolBase* tool)
{
    // use of dynamic_cast<> seems wrong here -- we just need to check the current tool

    if (dynamic_cast<Inkscape::UI::Tools::RectTool *>(tool)) {
        Inkscape::Selection *sel = desktop->getSelection();

        _changed = sel->connectChanged(sigc::mem_fun(*this, &RectToolbar::selection_changed));

        // Synthesize an emission to trigger the update
        selection_changed(sel);
    } else {
        if (_changed) {
            _changed.disconnect();
        
            if (_repr) { // remove old listener
                _repr->removeObserver(*this);
                Inkscape::GC::release(_repr);
                _repr = nullptr;
            }
        }
    }
}

/**
 *  \param selection should not be NULL.
 */
void
RectToolbar::selection_changed(Inkscape::Selection *selection)
{
    int n_selected = 0;
    Inkscape::XML::Node *repr = nullptr;
    SPItem *item = nullptr;

    if (_repr) { // remove old listener
        _item = nullptr;
        _repr->removeObserver(*this);
        Inkscape::GC::release(_repr);
        _repr = nullptr;
    }

    auto itemlist= selection->items();
    for(auto i=itemlist.begin();i!=itemlist.end();++i){
        if (is<SPRect>(*i)) {
            n_selected++;
            item = *i;
            repr = item->getRepr();
        }
    }

    _single = false;

    if (n_selected == 0) {
        _mode_item->set_markup(_("<b>New:</b>"));
        _width_item->set_sensitive(false);
        _height_item->set_sensitive(false);
    } else if (n_selected == 1) {
        _mode_item->set_markup(_("<b>Change:</b>"));
        _single = true;
        _width_item->set_sensitive(true);
        _height_item->set_sensitive(true);

        if (repr) {
            _repr = repr;
            _item = item;
            Inkscape::GC::anchor(_repr);
            _repr->addObserver(*this);
            _repr->synthesizeEvents(*this);
        }
    } else {
        // FIXME: implement averaging of all parameters for multiple selected
        //gtk_label_set_markup(GTK_LABEL(l), _("<b>Average:</b>"));
        _mode_item->set_markup(_("<b>Change:</b>"));
        sensitivize();
    }
}

void RectToolbar::notifyAttributeChanged(Inkscape::XML::Node &repr, GQuark,
                                         Inkscape::Util::ptr_shared,
                                         Inkscape::Util::ptr_shared)
{
    // quit if run by the _changed callbacks
    if (_freeze) {
        return;
    }

    // in turn, prevent callbacks from responding
    _freeze = true;

    auto unit = _tracker->getActiveUnit();
    if (!unit) {
        return;
    }

    auto rx_adj = _rx_item->get_adjustment();
    auto ry_adj = _ry_item->get_adjustment();
    auto width_adj = _width_item->get_adjustment();
    auto height_adj = _height_item->get_adjustment();

    if (auto rect = cast<SPRect>(_item)) {
        rx_adj->set_value(Quantity::convert(rect->getVisibleRx(), "px", unit));
        ry_adj->set_value(Quantity::convert(rect->getVisibleRy(), "px", unit));
        width_adj->set_value(Quantity::convert(rect->getVisibleWidth(), "px", unit));
        height_adj->set_value(Quantity::convert(rect->getVisibleHeight(), "px", unit));
    }

    sensitivize();
    _freeze = false;
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8 :
