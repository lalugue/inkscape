// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Spiral aux toolbar
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

#include "spiral-toolbar.h"

#include <glibmm/i18n.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/separatortoolitem.h>

#include "desktop.h"
#include "document-undo.h"
#include "object/sp-spiral.h"
#include "selection.h"
#include "ui/builder-utils.h"
#include "ui/icon-names.h"
#include "ui/widget/canvas.h"
#include "ui/widget/spinbutton.h"

using Inkscape::DocumentUndo;

namespace Inkscape {
namespace UI {
namespace Toolbar {

SpiralToolbar::SpiralToolbar(SPDesktop *desktop)
    : Toolbar(desktop)
    , _builder(initialize_builder("toolbar-spiral.ui"))
{
    _builder->get_widget("spiral-toolbar", _toolbar);
    if (!_toolbar) {
        std::cerr << "InkscapeWindow: Failed to load spiral toolbar!" << std::endl;
    }

    Gtk::Button *reset_item;

    _builder->get_widget("_mode_item", _mode_item);

    _builder->get_widget_derived("_revolution_item", _revolution_item);
    _builder->get_widget_derived("_expansion_item", _expansion_item);
    _builder->get_widget_derived("_t0_item", _t0_item);

    _builder->get_widget("reset_item", reset_item);

    setup_derived_spin_button(_revolution_item, "revolution", 3.0);
    setup_derived_spin_button(_expansion_item, "expansion", 1.0);
    setup_derived_spin_button(_t0_item, "t0", 0.0);

    add(*_toolbar);

    reset_item->signal_clicked().connect(sigc::mem_fun(*this, &SpiralToolbar::defaults));

    _connection.reset(new sigc::connection(
        desktop->getSelection()->connectChanged(sigc::mem_fun(*this, &SpiralToolbar::selection_changed))));

    show_all();
}

void SpiralToolbar::setup_derived_spin_button(UI::Widget::SpinButton *btn, const Glib::ustring &name,
                                              double default_value)
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    auto adj = btn->get_adjustment();

    const Glib::ustring path = "/tools/shapes/spiral/" + name;
    auto val = prefs->getDouble(path, default_value);
    adj->set_value(val);

    adj->signal_value_changed().connect(sigc::bind(sigc::mem_fun(*this, &SpiralToolbar::value_changed), adj, name));

    btn->set_defocus_widget(_desktop->getCanvas());
}

SpiralToolbar::~SpiralToolbar()
{
    if(_repr) {
        _repr->removeObserver(*this);
        GC::release(_repr);
        _repr = nullptr;
    }

    if(_connection) {
        _connection->disconnect();
    }
}

GtkWidget *
SpiralToolbar::create(SPDesktop *desktop)
{
    auto toolbar = new SpiralToolbar(desktop);
    return toolbar->Gtk::Widget::gobj();
}

void
SpiralToolbar::value_changed(Glib::RefPtr<Gtk::Adjustment> &adj,
                             Glib::ustring const           &value_name)
{
    if (DocumentUndo::getUndoSensitive(_desktop->getDocument())) {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        prefs->setDouble("/tools/shapes/spiral/" + value_name,
            adj->get_value());
    }

    // quit if run by the attr_changed listener
    if (_freeze) {
        return;
    }

    // in turn, prevent listener from responding
    _freeze = true;

    gchar* namespaced_name = g_strconcat("sodipodi:", value_name.data(), nullptr);

    bool modmade = false;
    auto itemlist= _desktop->getSelection()->items();
    for(auto i=itemlist.begin();i!=itemlist.end(); ++i){
        SPItem *item = *i;
        if (is<SPSpiral>(item)) {
            Inkscape::XML::Node *repr = item->getRepr();
            repr->setAttributeSvgDouble(namespaced_name, adj->get_value() );
            item->updateRepr();
            modmade = true;
        }
    }

    g_free(namespaced_name);

    if (modmade) {
        DocumentUndo::done(_desktop->getDocument(), _("Change spiral"), INKSCAPE_ICON("draw-spiral"));
    }

    _freeze = false;
}

void
SpiralToolbar::defaults()
{
    // fixme: make settable
    gdouble rev = 3;
    gdouble exp = 1.0;
    gdouble t0 = 0.0;

    _revolution_item->get_adjustment()->set_value(rev);
    _expansion_item->get_adjustment()->set_value(exp);
    _t0_item->get_adjustment()->set_value(t0);

    if(_desktop->getCanvas()) _desktop->getCanvas()->grab_focus();
}

void
SpiralToolbar::selection_changed(Inkscape::Selection *selection)
{
    int n_selected = 0;
    Inkscape::XML::Node *repr = nullptr;

    if ( _repr ) {
        _repr->removeObserver(*this);
        GC::release(_repr);
        _repr = nullptr;
    }

    auto itemlist= selection->items();
    for(auto i=itemlist.begin();i!=itemlist.end(); ++i){
        SPItem *item = *i;
        if (is<SPSpiral>(item)) {
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
        // FIXME: implement averaging of all parameters for multiple selected
        //gtk_label_set_markup(GTK_LABEL(l), _("<b>Average:</b>"));
        _mode_item->set_markup(_("<b>Change:</b>"));
    }
}

void SpiralToolbar::notifyAttributeChanged(Inkscape::XML::Node &repr, GQuark, Inkscape::Util::ptr_shared, Inkscape::Util::ptr_shared)
{

    // quit if run by the _changed callbacks
    if (_freeze) {
        return;
    }

    // in turn, prevent callbacks from responding
    _freeze = true;

    double revolution = repr.getAttributeDouble("sodipodi:revolution", 3.0);
    _revolution_item->get_adjustment()->set_value(revolution);

    double expansion = repr.getAttributeDouble("sodipodi:expansion", 1.0);
    _expansion_item->get_adjustment()->set_value(expansion);

    double t0 = repr.getAttributeDouble("sodipodi:t0", 0.0);
    _t0_item->get_adjustment()->set_value(t0);

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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
