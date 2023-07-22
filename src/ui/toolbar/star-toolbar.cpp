// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Star aux toolbar
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

#include "star-toolbar.h"

#include <glibmm/i18n.h>

#include "desktop.h"
#include "document-undo.h"
#include "object/sp-star.h"
#include "selection.h"
#include "ui/builder-utils.h"
#include "ui/icon-names.h"
#include "ui/tools/star-tool.h"
#include "ui/widget/canvas.h"
#include "ui/widget/spinbutton.h"

using Inkscape::DocumentUndo;

namespace Inkscape {
namespace UI {
namespace Toolbar {

StarToolbar::StarToolbar(SPDesktop *desktop)
    : Toolbar(desktop)
    , _builder(initialize_builder("toolbar-star.ui"))
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    bool is_flat_sided = prefs->getBool("/tools/shapes/star/isflatsided", false);

    _builder->get_widget("star-toolbar", _toolbar);
    if (!_toolbar) {
        std::cerr << "InkscapeWindow: Failed to load star toolbar!" << std::endl;
    }

    Gtk::ToggleButton *flat_polygon_button;
    Gtk::ToggleButton *flat_star_button;

    _builder->get_widget("_mode_item", _mode_item);
    _builder->get_widget("flat_polygon_button", flat_polygon_button);
    _builder->get_widget("flat_star_button", flat_star_button);

    _builder->get_widget("_spoke_box", _spoke_box);

    _builder->get_widget_derived("_magnitude_item", _magnitude_item);
    _builder->get_widget_derived("_spoke_item", _spoke_item);
    _builder->get_widget_derived("_roundedness_item", _roundedness_item);
    _builder->get_widget_derived("_randomization_item", _randomization_item);

    _builder->get_widget("_reset_item", _reset_item);

    setup_derived_spin_button(_magnitude_item, "magnitude", is_flat_sided ? 3 : 2);
    setup_derived_spin_button(_spoke_item, "proportion", 0.5);
    setup_derived_spin_button(_roundedness_item, "rounded", 0.0);
    setup_derived_spin_button(_randomization_item, "randomized", 0.0);

    /* Flatsided checkbox */
    _flat_item_buttons.push_back(flat_polygon_button);
    _flat_item_buttons.push_back(flat_star_button);
    _flat_item_buttons[is_flat_sided ? 0 : 1]->set_active();

    int btn_index = 0;

    for (auto btn : _flat_item_buttons) {
        btn->signal_clicked().connect(sigc::bind(sigc::mem_fun(*this, &StarToolbar::side_mode_changed), btn_index++));
    }

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

    desktop->connectEventContextChanged(sigc::mem_fun(*this, &StarToolbar::watch_tool));

    add(*_toolbar);

    show_all();
    _spoke_item->set_visible(!is_flat_sided);
}

void StarToolbar::setup_derived_spin_button(UI::Widget::SpinButton *btn, const Glib::ustring &name, float initial_value)
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    const Glib::ustring path = "/tools/shapes/star/" + name;
    auto val = prefs->getDouble(path, initial_value);

    auto adj = btn->get_adjustment();
    adj->set_value(val);

    if (name == "magnitude") {
        adj->signal_value_changed().connect(sigc::mem_fun(*this, &StarToolbar::magnitude_value_changed));
    } else if (name == "proportion") {
        adj->signal_value_changed().connect(sigc::mem_fun(*this, &StarToolbar::proportion_value_changed));
    } else if (name == "rounded") {
        adj->signal_value_changed().connect(sigc::mem_fun(*this, &StarToolbar::rounded_value_changed));
    } else if (name == "randomized") {
        adj->signal_value_changed().connect(sigc::mem_fun(*this, &StarToolbar::randomized_value_changed));
    }

    btn->set_sensitive(true);
    btn->set_defocus_widget(_desktop->getCanvas());
}

StarToolbar::~StarToolbar()
{
    if (_repr) { // remove old listener
        _repr->removeObserver(*this);
        Inkscape::GC::release(_repr);
        _repr = nullptr;
    }
}

GtkWidget *StarToolbar::create(SPDesktop *desktop)
{
    auto toolbar = new StarToolbar(desktop);
    return toolbar->Gtk::Widget::gobj();
}

void StarToolbar::side_mode_changed(int mode)
{
    bool const flat = mode == 0;

    if (DocumentUndo::getUndoSensitive(_desktop->getDocument())) {
        auto prefs = Preferences::get();
        prefs->setBool("/tools/shapes/star/isflatsided", flat);
    }

    // quit if run by the attr_changed listener
    if (_freeze) {
        return;
    }

    // in turn, prevent listener from responding
    _freeze = true;

    if (_spoke_box) {
        _spoke_box->set_visible(!flat);
    }

    for (auto item : _desktop->getSelection()->items()) {
        if (is<SPStar>(item)) {
            auto repr = item->getRepr();
            if (flat) {
                int sides = _magnitude_item->get_adjustment()->get_value();
                if (sides < 3) {
                    repr->setAttributeInt("sodipodi:sides", 3);
                }
            }
            repr->setAttribute("inkscape:flatsided", flat ? "true" : "false");

            item->updateRepr();
        }
    }

    _magnitude_item->get_adjustment()->set_lower(flat ? 3 : 2);
    if (flat && _magnitude_item->get_adjustment()->get_value() < 3) {
        _magnitude_item->get_adjustment()->set_value(3);
    }

    if (!_batchundo) {
        DocumentUndo::done(_desktop->getDocument(), flat ? _("Make polygon") : _("Make star"), INKSCAPE_ICON("draw-polygon-star"));
    }

    _freeze = false;
}

void StarToolbar::magnitude_value_changed()
{
    if (DocumentUndo::getUndoSensitive(_desktop->getDocument())) {
        // do not remember prefs if this call is initiated by an undo change, because undoing object
        // creation sets bogus values to its attributes before it is deleted
        auto prefs = Preferences::get();
        prefs->setInt("/tools/shapes/star/magnitude", _magnitude_item->get_adjustment()->get_value());
    }

    // quit if run by the attr_changed listener
    if (_freeze) {
        return;
    }

    // in turn, prevent listener from responding
    _freeze = true;

    for (auto item : _desktop->getSelection()->items()) {
        if (is<SPStar>(item)) {
            auto repr = item->getRepr();
            repr->setAttributeInt("sodipodi:sides", _magnitude_item->get_adjustment()->get_value());
            double arg1 = repr->getAttributeDouble("sodipodi:arg1", 0.5);
            repr->setAttributeSvgDouble("sodipodi:arg2", arg1 + M_PI / _magnitude_item->get_adjustment()->get_value());
            item->updateRepr();
        }
    }
    if (!_batchundo) {
        DocumentUndo::maybeDone(_desktop->getDocument(), "star:numcorners", _("Star: Change number of corners"), INKSCAPE_ICON("draw-polygon-star"));
    }

    _freeze = false;
}

void StarToolbar::proportion_value_changed()
{
    if (DocumentUndo::getUndoSensitive(_desktop->getDocument())) {
        if (!std::isnan(_spoke_item->get_adjustment()->get_value())) {
            auto prefs = Preferences::get();
            prefs->setDouble("/tools/shapes/star/proportion", _spoke_item->get_adjustment()->get_value());
        }
    }

    // quit if run by the attr_changed listener
    if (_freeze) {
        return;
    }

    // in turn, prevent listener from responding
    _freeze = true;

    for (auto item : _desktop->getSelection()->items()) {
        if (is<SPStar>(item)) {
            auto repr = item->getRepr();

            double r1 = repr->getAttributeDouble("sodipodi:r1", 1.0);
            double r2 = repr->getAttributeDouble("sodipodi:r2", 1.0);

            if (r2 < r1) {
                repr->setAttributeSvgDouble("sodipodi:r2", r1 * _spoke_item->get_adjustment()->get_value());
            } else {
                repr->setAttributeSvgDouble("sodipodi:r1", r2 * _spoke_item->get_adjustment()->get_value());
            }

            item->updateRepr();
        }
    }

    if (!_batchundo) {
        DocumentUndo::maybeDone(_desktop->getDocument(), "star:spokeratio", _("Star: Change spoke ratio"), INKSCAPE_ICON("draw-polygon-star"));
    }

    _freeze = false;
}

void StarToolbar::rounded_value_changed()
{
    if (DocumentUndo::getUndoSensitive(_desktop->getDocument())) {
        auto prefs = Preferences::get();
        prefs->setDouble("/tools/shapes/star/rounded", _roundedness_item->get_adjustment()->get_value());
    }

    // quit if run by the attr_changed listener
    if (_freeze) {
        return;
    }

    // in turn, prevent listener from responding
    _freeze = true;

    for (auto item : _desktop->getSelection()->items()) {
        if (is<SPStar>(item)) {
            auto repr = item->getRepr();
            repr->setAttributeSvgDouble("inkscape:rounded", _roundedness_item->get_adjustment()->get_value());
            item->updateRepr();
        }
    }
    if (!_batchundo) {
        DocumentUndo::maybeDone(_desktop->getDocument(), "star:rounding", _("Star: Change rounding"), INKSCAPE_ICON("draw-polygon-star"));
    }

    _freeze = false;
}

void StarToolbar::randomized_value_changed()
{
    if (DocumentUndo::getUndoSensitive(_desktop->getDocument())) {
        auto prefs = Inkscape::Preferences::get();
        prefs->setDouble("/tools/shapes/star/randomized", _randomization_item->get_adjustment()->get_value());
    }

    // quit if run by the attr_changed listener
    if (_freeze) {
        return;
    }

    // in turn, prevent listener from responding
    _freeze = true;

    for (auto item : _desktop->getSelection()->items()) {
        if (is<SPStar>(item)) {
            auto repr = item->getRepr();
            repr->setAttributeSvgDouble("inkscape:randomized", _randomization_item->get_adjustment()->get_value());
            item->updateRepr();
        }
    }
    if (!_batchundo) {
        DocumentUndo::maybeDone(_desktop->getDocument(), "star:randomisation", _("Star: Change randomization"), INKSCAPE_ICON("draw-polygon-star"));
    }

    _freeze = false;
}

void StarToolbar::defaults()
{
    _batchundo = true;

    // fixme: make settable in prefs!
    int mag = 5;
    double prop = 0.5;
    bool flat = false;
    double randomized = 0;
    double rounded = 0;

    _flat_item_buttons[flat ? 0 : 1]->set_active();

    _spoke_item->set_visible(!flat);

    if (_magnitude_item->get_adjustment()->get_value() == mag) {
        // Ensure handler runs even if value not changed, to reset inner handle.
        magnitude_value_changed();
    } else {
        _magnitude_item->get_adjustment()->set_value(mag);
    }
    _spoke_item->get_adjustment()->set_value(prop);
    _roundedness_item->get_adjustment()->set_value(rounded);
    _randomization_item->get_adjustment()->set_value(randomized);

    DocumentUndo::done(_desktop->getDocument(), _("Star: Reset to defaults"), INKSCAPE_ICON("draw-polygon-star"));
    _batchundo = false;
}

void StarToolbar::watch_tool(SPDesktop *desktop, UI::Tools::ToolBase *tool)
{
    _changed.disconnect();
    if (dynamic_cast<UI::Tools::StarTool const*>(tool)) {
        _changed = desktop->getSelection()->connectChanged(sigc::mem_fun(*this, &StarToolbar::selection_changed));
        selection_changed(desktop->getSelection());
    }
}

/**
 *  \param selection Should not be NULL.
 */
void StarToolbar::selection_changed(Selection *selection)
{
    int n_selected = 0;
    XML::Node *repr = nullptr;

    if (_repr) { // remove old listener
        _repr->removeObserver(*this);
        Inkscape::GC::release(_repr);
        _repr = nullptr;
    }

    for (auto item : selection->items()) {
        if (is<SPStar>(item)) {
            n_selected++;
            repr = item->getRepr();
        }
    }

    if (n_selected == 0) {
        _mode_item->set_markup(_("<b>New:</b>"));
    } else if (n_selected == 1) {
        _mode_item->set_markup(_("<b>Change:</b>"));

        if (repr) {
            _repr = repr;
            Inkscape::GC::anchor(_repr);
            _repr->addObserver(*this);
            _repr->synthesizeEvents(*this);
        }
    } else {
        // FIXME: implement averaging of all parameters for multiple selected stars
        //gtk_label_set_markup(GTK_LABEL(l), _("<b>Average:</b>"));
        //gtk_label_set_markup(GTK_LABEL(l), _("<b>Change:</b>"));
    }
}

void StarToolbar::notifyAttributeChanged(Inkscape::XML::Node &repr, GQuark name_,
                                         Inkscape::Util::ptr_shared,
                                         Inkscape::Util::ptr_shared)
{
    // quit if run by the _changed callbacks
    if (_freeze) {
        return;
    }

    auto const name = g_quark_to_string(name_);

    // in turn, prevent callbacks from responding
    _freeze = true;

    auto prefs = Preferences::get();
    bool isFlatSided = prefs->getBool("/tools/shapes/star/isflatsided", false);

    if (!strcmp(name, "inkscape:randomized")) {
        double randomized = repr.getAttributeDouble("inkscape:randomized", 0.0);
        _randomization_item->get_adjustment()->set_value(randomized);
    } else if (!strcmp(name, "inkscape:rounded")) {
        double rounded = repr.getAttributeDouble("inkscape:rounded", 0.0);
        _roundedness_item->get_adjustment()->set_value(rounded);
    } else if (!strcmp(name, "inkscape:flatsided")) {
        char const *flatsides = repr.attribute("inkscape:flatsided");
        if (flatsides && !strcmp(flatsides,"false")) {
            _flat_item_buttons[1]->set_active();
            _spoke_item->set_visible(true);
            _magnitude_item->get_adjustment()->set_lower(2);
        } else {
            _flat_item_buttons[0]->set_active();
            _spoke_item->set_visible(false);
            _magnitude_item->get_adjustment()->set_lower(3);
        }
    } else if (!strcmp(name, "sodipodi:r1") || !strcmp(name, "sodipodi:r2") && !isFlatSided) {
        double r1 = repr.getAttributeDouble("sodipodi:r1", 1.0);
        double r2 = repr.getAttributeDouble("sodipodi:r2", 1.0);

        if (r2 < r1) {
            _spoke_item->get_adjustment()->set_value(r2 / r1);
        } else {
            _spoke_item->get_adjustment()->set_value(r1 / r2);
        }
    } else if (!strcmp(name, "sodipodi:sides")) {
        int sides = repr.getAttributeInt("sodipodi:sides", 0);
        _magnitude_item->get_adjustment()->set_value(sides);
    }

    _freeze = false;
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8 :
