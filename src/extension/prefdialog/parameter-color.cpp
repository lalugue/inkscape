// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2005-2007 Authors:
 *   Ted Gould <ted@gould.cx>
 *   Johan Engelen <johan@shouraizou.nl>
 *   Christopher Brown <audiere@gmail.com>
 *   Jon A. Cruz <jon@joncruz.org>
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "parameter-color.h"

#include <iostream>
#include <sstream>

#include <gtkmm/box.h>

#include "color.h"
#include "preferences.h"

#include "extension/extension.h"

#include "ui/widget/color-notebook.h"

#include "xml/node.h"


namespace Inkscape {
namespace Extension {

ParamColor::ParamColor(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext)
    : Parameter(xml, ext)
{
    // get value
    unsigned int _value = 0x000000ff; // default to black
    if (xml->firstChild()) {
        const char *value = xml->firstChild()->content();
        if (value) {
            _value = strtoul(value, nullptr, 0);
        }
    }

    gchar *pref_name = this->pref_name();
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    _value = prefs->getUInt(extension_pref_root + pref_name, _value);
    g_free(pref_name);

    _color.setValue(_value);

    _color_changed = _color.signal_changed.connect(sigc::mem_fun(this, &ParamColor::_onColorChanged));
    // TODO: SelectedColor does not properly emit signal_changed after dragging, so we also need the following
    _color_released = _color.signal_released.connect(sigc::mem_fun(this, &ParamColor::_onColorChanged));
}

ParamColor::~ParamColor()
{
    _color_changed.disconnect();
    _color_released.disconnect();
}

guint32 ParamColor::set(guint32 in, SPDocument * /*doc*/, Inkscape::XML::Node * /*node*/)
{
    _color.setValue(in);

    return in;
}

Gtk::Widget *ParamColor::get_widget( SPDocument * /*doc*/, Inkscape::XML::Node * /*node*/, sigc::signal<void> *changeSignal )
{
    using Inkscape::UI::Widget::ColorNotebook;

    if (_hidden) return nullptr;

    if (changeSignal) {
        _changeSignal = new sigc::signal<void>(*changeSignal);
    }

    Gtk::HBox *hbox = Gtk::manage(new Gtk::HBox(false, Parameter::GUI_PARAM_WIDGETS_SPACING));
    Gtk::Widget *selector = Gtk::manage(new ColorNotebook(_color));
    hbox->pack_start(*selector, true, true, 0);
    selector->show();
    hbox->show();

    return hbox;
}

void ParamColor::_onColorChanged()
{
    gchar *pref_name = this->pref_name();
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setUInt(extension_pref_root + pref_name, _color.value());
    g_free(pref_name);

    if (_changeSignal)
        _changeSignal->emit();
}

void ParamColor::string(std::string &string) const
{
    char str[16];
    snprintf(str, 16, "%u", _color.value());
    string += str;
}

};  /* namespace Extension */
};  /* namespace Inkscape */
