// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Build a set of color page for a given color space
 *//*
 * Copyright (C) 2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "ui/widget/color-page.h"

#include <gtkmm/adjustment.h>
#include <gtkmm/box.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>

#include "colors/color.h"
#include "colors/color-set.h"
#include "colors/spaces/base.h"
#include "colors/spaces/components.h"
#include "ui/builder-utils.h"

namespace Inkscape::UI::Widget {

static unsigned int MAX_COMPONENTS = 6;

ColorPage::ColorPage(std::shared_ptr<Space::AnySpace> space, std::shared_ptr<ColorSet> colors)
    : Gtk::Box()
    , _builder(create_builder("color-page.glade"))
    , _space(std::move(space))
    , _selected_colors(std::move(colors))
    , _specific_colors(std::make_shared<Colors::ColorSet>(_space, true))
{
    append(get_widget<Gtk::Grid>(_builder, "color-page"));

    // Keep the selected colorset in-sync with the space specific colorset.
    _specific_changed_connection = _specific_colors->signal_changed.connect([this]() {
        _selected_changed_connection.block();
        for (auto &[id, color] : *_specific_colors) {
            _selected_colors->set(id, color);
        }
        _selected_changed_connection.unblock();
    });

    // Keep the child in-sync with the selected colorset.
    _selected_changed_connection = _selected_colors->signal_changed.connect([this]() {
        _specific_changed_connection.block();
        for (auto &[id, color] : *_selected_colors) {
            _specific_colors->set(id, color);
        }
        _specific_changed_connection.unblock();
    });

    // Control signals when widget isn't mapped (not visible to the user)
    signal_map().connect([this]() {
        _specific_colors->setAll(*_selected_colors);
        _specific_changed_connection.unblock();
        _selected_changed_connection.unblock();
    });

    signal_unmap().connect([this]() {
        _specific_colors->clear();
        _specific_changed_connection.block();
        _selected_changed_connection.block();
    });

    for (auto &component : _specific_colors->getComponents()) {
        std::string index = std::to_string(component.index + 1);
        _channels.emplace_back(
            get_widget<Gtk::Label>(_builder, ("label" + index).c_str()),
            get_derived_widget<ColorSlider>(_builder, ("slider" + index).c_str(), _specific_colors, component),
            get_derived_widget<SpinButton>(_builder, ("spin" + index).c_str())
        );
    }

    // Hide unncessary channel widgets
    for (auto j = _specific_colors->getComponents().size(); j < MAX_COMPONENTS; j++) {
        std::string index = std::to_string(j+1);
        hide_widget(_builder, "label" + index);
        hide_widget(_builder, "slider" + index);
        hide_widget(_builder, "spin" + index);
    }
}

ColorPageChannel::ColorPageChannel(
    Gtk::Label &label,
    ColorSlider &slider,
    SpinButton &spin)
    : _label(label)
    , _slider(slider)
    , _spin(spin)
    , _adj(spin.get_adjustment())
{
    auto &component = _slider._component;
    _label.set_text_with_mnemonic(component.name);
    _label.set_tooltip_text(component.tip);

    _slider.set_tooltip_text(component.tip);
    _slider.set_hexpand(true);

    _spin.set_tooltip_text(component.tip);

    _adj->set_lower(0.0);
    _adj->set_upper(component.scale);
    _adj->set_page_increment(0.0);
    _adj->set_page_size(0.0);

    _adj_changed = _adj->signal_value_changed().connect([this]() {
        _slider_changed.block();
        _slider.setScaled(_adj->get_value());
        _slider_changed.unblock();
    });
    _slider_changed = _slider.signal_value_changed.connect([this]() {
        _adj_changed.block();
        _adj->set_value(_slider.getScaled());
        _adj_changed.unblock();
    });
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
// vim:filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8: textwidth=99:
