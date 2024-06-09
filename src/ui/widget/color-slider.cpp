// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * A slider with colored background - implementation.
 *//*
 * Authors:
 *   see git history
 *
 * Copyright (C) 2018-2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "color-slider.h"

#include <cairomm/pattern.h>
#include <gdkmm/general.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/gestureclick.h>
#include <sigc++/functors/mem_fun.h>
#include <utility>

#include "colors/color.h"
#include "colors/color-set.h"
#include "colors/spaces/base.h"
#include "colors/spaces/components.h"
#include "preferences.h"
#include "ui/controller.h"
#include "ui/util.h"

static const gint ARROW_SIZE = 8;
constexpr uint32_t ERR_DARK = 0xff00ff00;    // Green
constexpr uint32_t ERR_LIGHT = 0xffff00ff;   // Magenta
constexpr uint32_t CHECK_DARK = 0xff5f5f5f;  // Dark gray
constexpr uint32_t CHECK_LIGHT = 0xffa0a0a0; // Light gray

namespace Inkscape::UI::Widget {

ColorSlider::ColorSlider(
    BaseObjectType *cobject,
    Glib::RefPtr<Gtk::Builder> const &builder,
    std::shared_ptr<Colors::ColorSet> colors,
    Colors::Space::Component component)
    : Gtk::DrawingArea(cobject)
    , _colors(std::move(colors))
    , _component(std::move(component))
{
    set_name("ColorSlider");

    set_draw_func(sigc::mem_fun(*this, &ColorSlider::draw_func));

    Controller::add_click(*this,
                          sigc::mem_fun(*this, &ColorSlider::on_click_pressed ),
                          sigc::mem_fun(*this, &ColorSlider::on_click_released),
                          Controller::Button::left);
    Controller::add_motion<nullptr, &ColorSlider::on_motion, nullptr>
                          (*this, *this);
    _changed_connection = _colors->signal_changed.connect([this]() {
        queue_draw();
    });
}

static double get_value_at(Gtk::Widget const &self, double const x, double const y)
{
    return CLAMP(x / self.get_width(), 0.0, 1.0);
}

Gtk::EventSequenceState ColorSlider::on_click_pressed(Gtk::GestureClick const &click,
                                                      int /*n_press*/, double const x, double const y)
{
    _colors->grab();
    update_component(x, y, click.get_current_event_state());
    return Gtk::EventSequenceState::NONE;
}

Gtk::EventSequenceState ColorSlider::on_click_released(Gtk::GestureClick const & /*click*/,
                                                       int /*n_press*/, double /*x*/, double /*y*/)
{
    _colors->release();
    return Gtk::EventSequenceState::NONE;
}

void ColorSlider::on_motion(GtkEventControllerMotion const * const motion,
                            double const x, double const y)
{
    if (_colors->isGrabbed()) {
        auto const state = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(motion));
        update_component(x, y, (Gdk::ModifierType)state);
    }
}

void ColorSlider::update_component(double x, double y, Gdk::ModifierType const state)
{
    auto const constrained = Controller::has_flag(state, Gdk::ModifierType::CONTROL_MASK);

    // XXX We don't know how to deal with constraints yet.
    if (_colors->setAll(_component, get_value_at(*this, x, y))) {
        signal_value_changed.emit();
    }
}

/**
 * Generate a checkerboard pattern with the given colors.
 *
 * @arg dark - The RGBA dark color
 * @arg light - The RGBA light color
 * @arg scale - The scale factor of the cairo surface
 * @arg buffer - The memory to populate with this pattern
 *
 * @returns A Gdk::Pixbuf of the buffer memory.
 */
Glib::RefPtr<Gdk::Pixbuf> _make_checkerboard(uint32_t dark, uint32_t light, unsigned scale, std::vector<uint32_t> &buffer)
{
    // A pattern of 2x2 blocks is enough for REPEAT mode to do the rest, this way we never need to recalculate the checkerboard
    static auto block = 0x08 * scale;
    static auto pattern = block * 2;

    buffer = std::vector<uint32_t>(pattern * pattern);
    for (auto y = 0; y < pattern; y++) {
        for (auto x = 0; x < pattern; x++) {
            buffer[(y * pattern) + x] = ((x / block) & 1) != ((y / block) & 1) ? dark : light;
        }
    }
    return Gdk::Pixbuf::create_from_data((guint8*)buffer.data(), Gdk::Colorspace::RGB, true, 8, pattern, pattern, pattern * 4);
}

void ColorSlider::draw_func(Cairo::RefPtr<Cairo::Context> const &cr,
                            int const width, int const height)
{
    auto const scale = get_scale_factor();
    bool const is_alpha = _component.id == "a";

    // changing scale to draw pixmap at display resolution
    cr->save();
    cr->scale(1.0 / scale, 1.0 / scale);

    // Color set is empty, this is not allowed, show warning colors
    if (_colors->isEmpty()) {
        static std::vector<uint32_t> err_buffer;
        static Glib::RefPtr<Gdk::Pixbuf> error = _make_checkerboard(ERR_DARK, ERR_LIGHT, scale, err_buffer);

        Gdk::Cairo::set_source_pixbuf(cr, error, 0, 0);
        cr->get_source()->set_extend(Cairo::Pattern::Extend::REPEAT);
        cr->paint();

        // Don't try and paint any color (there isn't any)
        cr->restore();
        return;
    }

    // The alpha background is a checkerboard pattern of light and dark pixels
    if (is_alpha) {
        static std::vector<uint32_t> bg_buffer;
        static Glib::RefPtr<Gdk::Pixbuf> background = _make_checkerboard(CHECK_DARK, CHECK_LIGHT, scale, bg_buffer);

        // Paint the alpha background
        Gdk::Cairo::set_source_pixbuf(cr, background, 0, 0);
        cr->get_source()->set_extend(Cairo::Pattern::Extend::REPEAT);
        cr->paint();
    }

    if (!_gradient) {
        // Draw row of colored pixels here
        auto paint_color = _colors->getAverage();

        if (!is_alpha) {
            // Remove alpha channel from paint
            paint_color.enableOpacity(false);
        }

        // When the widget is wider, we want a new color gradient buffer
        if (_gr_buffer.size() < static_cast<size_t>(width)) {
            _gr_buffer = std::vector<uint32_t>(width);
            _gradient = Gdk::Pixbuf::create_from_data((guint8*)&_gr_buffer.front(), Gdk::Colorspace::RGB, true, 8, width, 1, width * 4);
        }

        for (int x = 0; x < width; x++) {
            paint_color.set(_component.index, static_cast<double>(x) / width);
            _gr_buffer[x] = paint_color.toABGR();
        }
    }

    Gdk::Cairo::set_source_pixbuf(cr, _gradient, 0, 0);
    cr->get_source()->set_extend(Cairo::Pattern::Extend::REPEAT);
    cr->paint();
    cr->restore();

    /* Draw arrow */
    double value = _colors->getAverage(_component);
    gint x = (int)(value * (width / scale) - ARROW_SIZE / 2);
    gint y1 = 0;
    gint y2 = height / scale - 1;
    cr->set_line_width(2.0);

    // Define top arrow
    cr->move_to(x - 0.5, y1 + 0.5);
    cr->line_to(x + ARROW_SIZE - 0.5, y1 + 0.5);
    cr->line_to(x + (ARROW_SIZE - 1) / 2.0, y1 + ARROW_SIZE / 2.0 + 0.5);
    cr->close_path();

    // Define bottom arrow
    cr->move_to(x - 0.5, y2 + 0.5);
    cr->line_to(x + ARROW_SIZE - 0.5, y2 + 0.5);
    cr->line_to(x + (ARROW_SIZE - 1) / 2.0, y2 - ARROW_SIZE / 2.0 + 0.5);
    cr->close_path();

    // Render both arrows
    cr->set_source_rgb(0.0, 0.0, 0.0);
    cr->stroke_preserve();
    cr->set_source_rgb(1.0, 1.0, 1.0);
    cr->fill();
}

double ColorSlider::getScaled() const
{
    if (_colors->isEmpty())
        return 0.0;
    return _colors->getAverage(_component) * _component.scale;
}

void ColorSlider::setScaled(double value)
{
    // setAll replaces every color with the same value, setAverage moves them all by the same amount.
    _colors->setAll(_component, value / _component.scale);
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
