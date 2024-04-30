// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Ralf Stephan <ralf@ark.in-berlin.de>
 *   Abhishek Sharma
 *
 * Copyright (C) Authors 2000-2005
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "color-picker.h"

#include <utility>

#include "desktop.h"
#include "document-undo.h"
#include "inkscape.h"
#include "ui/dialog-events.h"
#include "ui/widget/color-notebook.h"

static bool _in_use = false;

namespace Inkscape::UI::Widget {

ColorPicker::ColorPicker(Glib::ustring title,
                         Glib::ustring const &tip,
                         Colors::Color const &initial,
                         bool const undo,
                         bool use_transparency)
    : _preview(Gtk::make_managed<ColorPreview>(initial.toRGBA()))
    , _title(std::move(title))
    , _undo(undo)
    , _colors(std::make_shared<Colors::ColorSet>(nullptr, use_transparency))
{
    // set tooltip if given, otherwise leave original tooltip in place (from external button)
    if (!tip.empty()) {
        set_tooltip_text(tip);
    }
    _colors->set(initial);
    _construct();
}

ColorPicker::ColorPicker(BaseObjectType *cobject, Glib::RefPtr<Gtk::Builder> const &,
                         Glib::ustring title, bool use_transparency)
    : Gtk::Button(cobject)
    , _preview(Gtk::make_managed<ColorPreview>(0x0))
    , _title(std::move(title))
    , _colors(std::make_shared<Colors::ColorSet>(nullptr, use_transparency))
{
    _construct();
}

void ColorPicker::_construct()
{
    set_child(*_preview);

    _colors->signal_changed.connect(sigc::mem_fun(*this, &ColorPicker::_onSelectedColorChanged));
    _colors->signal_released.connect(sigc::mem_fun(*this, &ColorPicker::_onSelectedColorChanged));

    auto color_selector = Gtk::make_managed<ColorNotebook>(_colors);
    color_selector->set_label(_title);
    color_selector->set_margin(4);

    sp_transientize(_colorSelectorDialog);
    _colorSelectorDialog.set_child(*color_selector);
    _colorSelectorDialog.set_title(_title);
    _colorSelectorDialog.set_hide_on_close();

    property_sensitive().signal_changed().connect([this](){
        _preview->setEnabled(is_sensitive());
    });
}

ColorPicker::~ColorPicker() = default;

void ColorPicker::setColor(Colors::Color const &color)
{
    if (_in_use) return;

    _updating = true;
    set_preview(color.toRGBA());
    _colors->set(color);
    _updating = false;
}

void ColorPicker::closeWindow()
{
    _colorSelectorDialog.set_visible(false);
}

void ColorPicker::open() {
    on_clicked();
}

void ColorPicker::on_clicked()
{
    _colorSelectorDialog.present();
}

void ColorPicker::on_changed(Colors::Color const &color)
{
}

void ColorPicker::_onSelectedColorChanged()
{
    if (_updating || _in_use)
        return;

    if (_undo && SP_ACTIVE_DESKTOP) {
        DocumentUndo::done(SP_ACTIVE_DESKTOP->getDocument(), /* TODO: annotate */ "color-picker.cpp:129", "");
    }

    _in_use = true;
    if (auto color = _colors->get()) {
        set_preview(color->toRGBA());
        on_changed(*color);
        _changed_signal.emit(*color);
    }
    _in_use = false;

}

void ColorPicker::set_preview(std::uint32_t const rgba)
{
    bool has_alpha = _colors->getAlphaConstraint().value_or(true);
    _preview->setRgba32(has_alpha ? rgba : rgba | 0xff);
}

} // namespace Inkscape::UI::Widget

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
