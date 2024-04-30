// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Color picker button and window.
 */
/* Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Ralf Stephan <ralf@ark.in-berlin.de>
 *
 * Copyright (C) Authors 2000-2005
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_COLOR_PICKER_H
#define SEEN_INKSCAPE_COLOR_PICKER_H

#include "labelled.h"

#include <cstdint>
#include <utility>

#include "colors/color.h"
#include "colors/color-set.h"
#include "ui/widget/color-preview.h"
#include <gtkmm/button.h>
#include <gtkmm/dialog.h>
#include <sigc++/signal.h>

namespace Gtk {
class Builder;
}

namespace Inkscape::UI::Widget {

class ColorNotebook;

class ColorPicker : public Gtk::Button {
public:
    [[nodiscard]] ColorPicker(Glib::ustring title,
                              Glib::ustring const &tip,
                              Colors::Color const &initial,
                              bool undo,
                              bool use_transparency = true);

    ColorPicker(BaseObjectType *cobject, Glib::RefPtr<Gtk::Builder> const &,
                Glib::ustring title, bool use_transparency = true);

    ~ColorPicker() override;

    void setColor(Colors::Color const &);
    void open();
    void closeWindow();

    sigc::connection connectChanged(sigc::slot<void (Colors::Color const &)> slot)
    {
        return _changed_signal.connect(std::move(slot));
    }

    [[nodiscard]] Colors::Color get_current_color() const {
        if (_colors->isEmpty())
            return Colors::Color(0x0);
        return _colors->getAverage();
    }

protected:
    void _onSelectedColorChanged();
    void on_clicked() override;
    virtual void on_changed(Colors::Color const &);

    ColorPreview *_preview = nullptr;

    Glib::ustring _title;
    sigc::signal<void (Colors::Color const &)> _changed_signal;
    bool          _undo     = false;
    bool          _updating = false;

    void setupDialog(Glib::ustring const &title);
    Gtk::Dialog _colorSelectorDialog;

    std::shared_ptr<Colors::ColorSet> _colors;

private:
    void _construct();
    void set_preview(std::uint32_t rgba);
};


class LabelledColorPicker : public Labelled {
public:
    [[nodiscard]] LabelledColorPicker(Glib::ustring const &label,
                                      Glib::ustring const &title,
                                      Glib::ustring const &tip,
                                      Colors::Color const &initial,
                                      bool undo)
        : Labelled(label, tip, new ColorPicker(title, tip, initial, undo)) {
        property_sensitive().signal_changed().connect([this](){
            getWidget()->set_sensitive(is_sensitive());
        });
    }

    void setColor(Colors::Color const &color)
    {
        static_cast<ColorPicker *>(getWidget())->setColor(color);
    }

    void closeWindow()
    {
        static_cast<ColorPicker *>(getWidget())->closeWindow();
    }

    sigc::connection connectChanged(sigc::slot<void (Colors::Color const &)> slot)
    {
        return static_cast<ColorPicker*>(getWidget())->connectChanged(std::move(slot));
    }
};

} // namespace Inkscape::UI::Widget

#endif // SEEN_INKSCAPE_COLOR_PICKER_H

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
