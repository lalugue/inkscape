// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Color selector using sliders for each components, for multiple color modes
 *//*
 * Authors:
 *   see git history
 *   bulia byak <buliabyak@users.sf.net>
 *   Massinissa Derriche <massinissa.derriche@gmail.com>
 *
 * Copyright (C) 2018, 2021 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "ui/widget/color-scales.h"

#include <functional>
#include <stdexcept>

#include <glibmm/i18n.h>
#include <glibmm/ustring.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/expander.h>
#include <gtkmm/grid.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/spinbutton.h>

#include "colors/spaces/components.h"
#include "colors/spaces/enum.h"
#include "colors/spaces/oklch.h"
#include "preferences.h"
#include "ui/dialog-events.h"
#include "ui/icon-loader.h"
#include "ui/pack.h"
#include "ui/widget/color-slider.h"
#include "ui/widget/ink-color-wheel.h"
#include "ui/widget/oklab-color-wheel.h"

constexpr static int CSC_CHANNEL_H     = 0;
constexpr static int CSC_CHANNEL_S     = 1;
constexpr static int CSC_CHANNEL_V     = 2;
constexpr static int CSC_CHANNELS_ALL  = 10;

constexpr static int XPAD = 2;
constexpr static int YPAD = 2;

namespace Inkscape::UI::Widget {

static guchar const *sp_color_scales_hue_map();
static guchar const *sp_color_scales_hsluv_map(guchar *map,
        std::function<void(float*, float)> callback);

static const char* color_mode_icons[] = {
    nullptr,
    "color-selector-rgb",
    "color-selector-hsx",
    "color-selector-cmyk",
    "color-selector-hsx",
    "color-selector-hsluv",
    "color-selector-okhsl",
    "color-selector-cms",
    nullptr
};

const char* color_mode_name[] = {
    N_("None"), N_("RGB"), N_("HSL"), N_("CMYK"), N_("HSV"), N_("HSLuv"), N_("OKHSL"), nullptr
};

const char* get_color_mode_icon(SPColorScalesMode mode) {
    auto index = static_cast<size_t>(mode);
    assert(index > 0 && index < (sizeof(color_mode_icons) / sizeof(color_mode_icons[0])));
    return color_mode_icons[index];
}

const char* get_color_mode_label(SPColorScalesMode mode) {
    auto index = static_cast<size_t>(mode);
    assert(index > 0 && index < (sizeof(color_mode_name) / sizeof(color_mode_name[0])));
    return color_mode_name[index];
}

std::unique_ptr<Inkscape::UI::ColorSelectorFactory> get_factory(SPColorScalesMode mode) {
    switch (mode) {
        case SPColorScalesMode::RGB:   return std::make_unique<ColorScalesFactory<SPColorScalesMode::RGB>>();
        case SPColorScalesMode::HSL:   return std::make_unique<ColorScalesFactory<SPColorScalesMode::HSL>>();
        case SPColorScalesMode::HSV:   return std::make_unique<ColorScalesFactory<SPColorScalesMode::HSV>>();
        case SPColorScalesMode::CMYK:  return std::make_unique<ColorScalesFactory<SPColorScalesMode::CMYK>>();
        case SPColorScalesMode::HSLUV: return std::make_unique<ColorScalesFactory<SPColorScalesMode::HSLUV>>();
        case SPColorScalesMode::OKLAB: return std::make_unique<ColorScalesFactory<SPColorScalesMode::OKLAB>>();
        //case SPColorScalesMode::CMS:   return std::make_unique<ColorICCSelectorFactory>();
        default:
            throw std::invalid_argument("There's no factory for the requested color mode");
    }
}

std::vector<ColorPickerDescription> get_color_pickers() {
    std::vector<ColorPickerDescription> pickers;

    for (auto mode : {
            SPColorScalesMode::HSL,
            SPColorScalesMode::HSV,
            SPColorScalesMode::RGB,
            SPColorScalesMode::CMYK,
            SPColorScalesMode::OKLAB,
            SPColorScalesMode::HSLUV,
            //SPColorScalesMode::CMS
        }) {
        auto label = get_color_mode_label(mode);

        pickers.emplace_back(ColorPickerDescription {
            mode,
            get_color_mode_icon(mode),
            label,
            Glib::ustring::format("/colorselector/", label, "/visible"),
            get_factory(mode)
        });
    }

    return pickers;
}


template <SPColorScalesMode MODE>
gchar const *ColorScales<MODE>::SUBMODE_NAMES[] = { N_("None"), N_("RGB"), N_("HSL"),
    N_("CMYK"), N_("HSV"), N_("HSLuv"), N_("OKHSL") };

// Preference name for the saved state of toggle-able color wheel
template <>
gchar const * const ColorScales<SPColorScalesMode::HSL>::_pref_wheel_visibility =
    "/wheel_vis_hsl";

template <>
gchar const * const ColorScales<SPColorScalesMode::HSV>::_pref_wheel_visibility =
    "/wheel_vis_hsv";

template <>
gchar const * const ColorScales<SPColorScalesMode::HSLUV>::_pref_wheel_visibility =
    "/wheel_vis_hsluv";

template <>
gchar const * const ColorScales<SPColorScalesMode::OKLAB>::_pref_wheel_visibility =
    "/wheel_vis_okhsl";

template <SPColorScalesMode MODE>
ColorScales<MODE>::ColorScales(SelectedColor color, bool no_alpha)
    : Gtk::Box()
    , _color(std::move(color))
    , _range_limit(255.0)
    , _updating(false)
    , _dragging(false)
    , _wheel(nullptr)
{
    assert(_color);

    for (gint i = 0; i < 5; i++) {
        _l[i] = nullptr;
        _s[i] = nullptr;
        _b[i] = nullptr;
    }

    _initUI(no_alpha);

    _color_changed = _color->signal_changed.connect([this](){ _onColorChanged(); });
}

template <SPColorScalesMode MODE>
void ColorScales<MODE>::_initUI(bool no_alpha)
{
    set_orientation(Gtk::Orientation::VERTICAL);

    Gtk::Expander *wheel_frame = nullptr;

    if constexpr (
            MODE == SPColorScalesMode::HSL   ||
            MODE == SPColorScalesMode::HSV   ||
            MODE == SPColorScalesMode::HSLUV ||
            MODE == SPColorScalesMode::OKLAB)
    {
        /* Create wheel */
        if constexpr (MODE == SPColorScalesMode::HSLUV) {
            _wheel = Gtk::make_managed<Inkscape::UI::Widget::ColorWheelHSLuv>();
        } else if constexpr (MODE == SPColorScalesMode::OKLAB) {
            _wheel = Gtk::make_managed<OKWheel>();
        } else {
            _wheel = Gtk::make_managed<Inkscape::UI::Widget::ColorWheelHSL>();
        }

        _wheel->set_visible(true);
        _wheel->set_halign(Gtk::Align::FILL);
        _wheel->set_valign(Gtk::Align::FILL);
        _wheel->set_hexpand(true);
        _wheel->set_vexpand(true);
        _wheel->set_name("ColorWheel");
        _wheel->set_size_request(-1, 130); // minimal size

        /* Signal */
        _wheel->connect_color_changed([this](){ _wheelChanged(); });

        /* Expander */
        // Label icon
        auto const expander_icon = Gtk::manage(
                sp_get_icon_image("color-wheel", Gtk::IconSize::NORMAL)
        );
        expander_icon->set_visible(true);
        expander_icon->set_margin_start(2 * XPAD);
        expander_icon->set_margin_end(3 * XPAD);
        // Label
        auto const expander_label = Gtk::make_managed<Gtk::Label>(_("Color Wheel"));
        expander_label->set_visible(true);
        // Content
        auto const expander_box = Gtk::make_managed<Gtk::Box>();
        expander_box->set_visible(true);
        UI::pack_start(*expander_box, *expander_icon);
        UI::pack_start(*expander_box, *expander_label);
        expander_box->set_halign(Gtk::Align::START);
        expander_box->set_valign(Gtk::Align::START);
        expander_box->set_orientation(Gtk::Orientation::HORIZONTAL);
        // Expander
        wheel_frame = Gtk::make_managed<Gtk::Expander>();
        wheel_frame->set_visible(true);
        wheel_frame->set_margin_start(2 * XPAD);
        wheel_frame->set_margin_end(XPAD);
        wheel_frame->set_margin_top(2 * YPAD);
        wheel_frame->set_margin_bottom(2 * YPAD);
        wheel_frame->set_halign(Gtk::Align::FILL);
        wheel_frame->set_valign(Gtk::Align::FILL);
        wheel_frame->set_hexpand(true);
        wheel_frame->set_vexpand(false);
        wheel_frame->set_label_widget(*expander_box);

        // Signal
        wheel_frame->property_expanded().signal_changed().connect([this, wheel_frame]() {
            bool visible = wheel_frame->get_expanded();
            wheel_frame->set_vexpand(visible);

            // Save wheel visibility
            Inkscape::Preferences::get()->setBool(_prefs + _pref_wheel_visibility, visible);
        });

        wheel_frame->set_child(*_wheel);
        append(*wheel_frame);
    }

    /* Create sliders */
    auto const grid = Gtk::make_managed<Gtk::Grid>();
    append(*grid);

    for (int i = 0; i < 5; i++) {
        /* Label */
        _l[i] = Gtk::make_managed<Gtk::Label>("", true);

        _l[i]->set_halign(Gtk::Align::START);
        _l[i]->set_visible(true);

        _l[i]->set_margin_start(2 * XPAD);
        _l[i]->set_margin_end(XPAD);
        _l[i]->set_margin_top(YPAD);
        _l[i]->set_margin_bottom(YPAD);
        grid->attach(*_l[i], 0, i, 1, 1);

        /* Adjustment */
        _a.push_back(Gtk::Adjustment::create(0.0, 0.0, _range_limit, 1.0, 10.0, 10.0));
        /* Slider */
        _s[i] = Gtk::make_managed<Inkscape::UI::Widget::ColorSlider>(_a[i]);
        _s[i]->set_visible(true);

        _s[i]->set_margin_start(XPAD);
        _s[i]->set_margin_end(XPAD);
        _s[i]->set_margin_top(YPAD);
        _s[i]->set_margin_bottom(YPAD);
        _s[i]->set_hexpand(true);
        grid->attach(*_s[i], 1, i, 1, 1);

        /* Spinbutton */
        _b[i] = Gtk::make_managed<Gtk::SpinButton>(_a[i], 1.0);
        sp_dialog_defocus_on_enter(*_b[i]);
        _l[i]->set_mnemonic_widget(*_b[i]);
        _b[i]->set_visible(true);

        _b[i]->set_margin_start(XPAD);
        _b[i]->set_margin_end(XPAD);
        _b[i]->set_margin_top(YPAD);
        _b[i]->set_margin_bottom(YPAD);
        _b[i]->set_halign(Gtk::Align::END);
        _b[i]->set_valign(Gtk::Align::CENTER);
        grid->attach(*_b[i], 2, i, 1, 1);

        /* Signals */
        _a[i]->signal_value_changed().connect([this, i](){ _adjustmentChanged(i); });
        _s[i]->signal_grabbed.connect([this](){ _sliderAnyGrabbed(); });
        _s[i]->signal_released.connect([this](){ _sliderAnyReleased(); });
        _s[i]->signal_value_changed.connect([this](){ _sliderAnyChanged(); });
    }

    setupMode(no_alpha);

    if constexpr (
            MODE == SPColorScalesMode::HSL   ||
            MODE == SPColorScalesMode::HSV   ||
            MODE == SPColorScalesMode::HSLUV ||
            MODE == SPColorScalesMode::OKLAB)
    {
        // Restore the visibility of the wheel
        bool visible = Inkscape::Preferences::get()->getBool(_prefs + _pref_wheel_visibility,
            false);
        wheel_frame->set_expanded(visible);
        wheel_frame->set_vexpand(visible);
    }

    if (!_color->isEmpty()) {
        _updateDisplay();
    }
}

template <SPColorScalesMode MODE>
void ColorScales<MODE>::_recalcColor()
{
    if (_color->isEmpty())
        g_warning("Color setter can't do anything, nothing to set to.");
    _color->setAll(_getColor());
}

template <SPColorScalesMode MODE>
void ColorScales<MODE>::_updateDisplay(bool update_wheel)
{
    if (_color->isEmpty()) {
        g_warning("Empty ColorSet for ColorScale...");
        return;
    }

    auto color = _color->getAverage();

    if constexpr (MODE == SPColorScalesMode::RGB) {
        color.convert(Colors::Space::Type::RGB);
    } else if constexpr (MODE == SPColorScalesMode::HSL) {
        color.convert(Colors::Space::Type::HSL);
    } else if constexpr (MODE == SPColorScalesMode::HSV) {
        color.convert(Colors::Space::Type::HSV);
    } else if constexpr (MODE == SPColorScalesMode::CMYK) {
        color.convert(Colors::Space::Type::CMYK);
    } else if constexpr (MODE == SPColorScalesMode::HSLUV) {
        color.convert(Colors::Space::Type::HSLUV);
    } else if constexpr (MODE == SPColorScalesMode::OKLAB) {
        color.convert(Colors::Space::Type::OKHSL);
    } else {
        g_warning("file %s: line %d: Illegal color selector mode NONE", __FILE__, __LINE__);
    }

    _updating = true;
    for (size_t i = 0; i < 5; i++) {
        if (i < color.size()) {
            setScaled(_a[i], color[i]);
        } else {
            setScaled(_a[i], 0.0);
        }
    }
    _updateSliders(CSC_CHANNELS_ALL);
    _updating = false;

    if (_wheel && update_wheel) {
        // N.B. We setColor() with emit = false, to avoid a warning from PaintSelector.
        _wheel->setColor(color, true, false);
    }
}

/* Helpers for setting color value */
template <SPColorScalesMode MODE>
double ColorScales<MODE>::getScaled(Glib::RefPtr<Gtk::Adjustment> const &a)
{
    return a->get_value() / a->get_upper();
}

template <SPColorScalesMode MODE>
void ColorScales<MODE>::setScaled(Glib::RefPtr<Gtk::Adjustment> &a, double v, bool constrained)
{
    auto upper = a->get_upper();
    double val = v * upper;
    if (constrained) {
        // TODO: do we want preferences for these?
        if (upper == 255) {
            val = round(val/16) * 16;
        } else {
            val = round(val/10) * 10;
        }
    }
    a->set_value(val);
}

template <SPColorScalesMode MODE>
void ColorScales<MODE>::_setRangeLimit(gdouble upper)
{
    _range_limit = upper;
    for (auto & i : _a) {
        i->set_upper(upper);
    }
}

template <SPColorScalesMode MODE>
void ColorScales<MODE>::_onColorChanged()
{
    if (!get_visible()) { return; }

    _updateDisplay();
}

template <SPColorScalesMode MODE>
void ColorScales<MODE>::on_show()
{
    Gtk::Box::on_show();

    _updateDisplay();
}

template <SPColorScalesMode MODE>
Colors::Color ColorScales<MODE>::_getColor()
{
    Colors::Color color(0x0);

    if constexpr (MODE == SPColorScalesMode::RGB) {
        color.convert(Colors::Space::Type::RGB);
    } else if constexpr (MODE == SPColorScalesMode::HSL) {
        color.convert(Colors::Space::Type::HSL);
    } else if constexpr (MODE == SPColorScalesMode::HSV) {
        color.convert(Colors::Space::Type::HSV);
    } else if constexpr (MODE == SPColorScalesMode::CMYK) {
        color.convert(Colors::Space::Type::CMYK);
    } else if constexpr (MODE == SPColorScalesMode::HSLUV) {
        color.convert(Colors::Space::Type::HSLUV);
    } else if constexpr (MODE == SPColorScalesMode::OKLAB) {
        color.convert(Colors::Space::Type::OKHSL);
    } else {
        g_warning("file %s: line %d: Illegal color selector mode", __FILE__, __LINE__);
    }
    for (size_t i = 0; i < color.size(); i++) {
        color.set(i, getScaled(_a[i]));
    }
    return color;
}

template <SPColorScalesMode MODE>
guint32 ColorScales<MODE>::_getRgba32()
{
    return _getColor().toRGBA();
}

template <SPColorScalesMode MODE>
void ColorScales<MODE>::setupMode(bool no_alpha)
{
    if constexpr (MODE == SPColorScalesMode::NONE) {
        return;
    }

    Colors::Color c = _getColor();

    if constexpr (MODE == SPColorScalesMode::RGB) {
        _setRangeLimit(255.0);
        _a[3]->set_upper(100.0);
        _l[0]->set_markup_with_mnemonic(_("_R:"));
        _s[0]->set_tooltip_text(_("Red"));
        _b[0]->set_tooltip_text(_("Red"));
        _l[1]->set_markup_with_mnemonic(_("_G:"));
        _s[1]->set_tooltip_text(_("Green"));
        _b[1]->set_tooltip_text(_("Green"));
        _l[2]->set_markup_with_mnemonic(_("_B:"));
        _s[2]->set_tooltip_text(_("Blue"));
        _b[2]->set_tooltip_text(_("Blue"));
        _l[3]->set_markup_with_mnemonic(_("_A:"));
        _s[3]->set_tooltip_text(_("Alpha (opacity)"));
        _b[3]->set_tooltip_text(_("Alpha (opacity)"));
        _s[0]->setMap(nullptr);
        _l[4]->set_visible(false);
        _s[4]->set_visible(false);
        _b[4]->set_visible(false);
        _updating = true;
        setScaled(_a[0], c[0]);
        setScaled(_a[1], c[1]);
        setScaled(_a[2], c[2]);
        setScaled(_a[3], c[3]);
        _updateSliders(CSC_CHANNELS_ALL);
        _updating = false;
    } else if constexpr (MODE == SPColorScalesMode::HSL) {
        _setRangeLimit(100.0);

        _l[0]->set_markup_with_mnemonic(_("_H:"));
        _s[0]->set_tooltip_text(_("Hue"));
        _b[0]->set_tooltip_text(_("Hue"));
        _a[0]->set_upper(360.0);

        _l[1]->set_markup_with_mnemonic(_("_S:"));
        _s[1]->set_tooltip_text(_("Saturation"));
        _b[1]->set_tooltip_text(_("Saturation"));

        _l[2]->set_markup_with_mnemonic(_("_L:"));
        _s[2]->set_tooltip_text(_("Lightness"));
        _b[2]->set_tooltip_text(_("Lightness"));

        _l[3]->set_markup_with_mnemonic(_("_A:"));
        _s[3]->set_tooltip_text(_("Alpha (opacity)"));
        _b[3]->set_tooltip_text(_("Alpha (opacity)"));
        _s[0]->setMap(sp_color_scales_hue_map());
        _l[4]->set_visible(false);
        _s[4]->set_visible(false);
        _b[4]->set_visible(false);
        _updating = true;

        setScaled(_a[0], c[0]);
        setScaled(_a[1], c[1]);
        setScaled(_a[2], c[2]);
        setScaled(_a[3], c[3]);

        _updateSliders(CSC_CHANNELS_ALL);
        _updating = false;
    } else if constexpr (MODE == SPColorScalesMode::HSV) {
        _setRangeLimit(100.0);

        _l[0]->set_markup_with_mnemonic(_("_H:"));
        _s[0]->set_tooltip_text(_("Hue"));
        _b[0]->set_tooltip_text(_("Hue"));
        _a[0]->set_upper(360.0);

        _l[1]->set_markup_with_mnemonic(_("_S:"));
        _s[1]->set_tooltip_text(_("Saturation"));
        _b[1]->set_tooltip_text(_("Saturation"));

        _l[2]->set_markup_with_mnemonic(_("_V:"));
        _s[2]->set_tooltip_text(_("Value"));
        _b[2]->set_tooltip_text(_("Value"));

        _l[3]->set_markup_with_mnemonic(_("_A:"));
        _s[3]->set_tooltip_text(_("Alpha (opacity)"));
        _b[3]->set_tooltip_text(_("Alpha (opacity)"));
        _s[0]->setMap(sp_color_scales_hue_map());
        _l[4]->set_visible(false);
        _s[4]->set_visible(false);
        _b[4]->set_visible(false);
        _updating = true;

        setScaled(_a[0], c[0]);
        setScaled(_a[1], c[1]);
        setScaled(_a[2], c[2]);
        setScaled(_a[3], c[3]);

        _updateSliders(CSC_CHANNELS_ALL);
        _updating = false;
    } else if constexpr (MODE == SPColorScalesMode::CMYK) {
        _setRangeLimit(100.0);
        _l[0]->set_markup_with_mnemonic(_("_C:"));
        _s[0]->set_tooltip_text(_("Cyan"));
        _b[0]->set_tooltip_text(_("Cyan"));

        _l[1]->set_markup_with_mnemonic(_("_M:"));
        _s[1]->set_tooltip_text(_("Magenta"));
        _b[1]->set_tooltip_text(_("Magenta"));

        _l[2]->set_markup_with_mnemonic(_("_Y:"));
        _s[2]->set_tooltip_text(_("Yellow"));
        _b[2]->set_tooltip_text(_("Yellow"));

        _l[3]->set_markup_with_mnemonic(_("_K:"));
        _s[3]->set_tooltip_text(_("Black"));
        _b[3]->set_tooltip_text(_("Black"));

        _l[4]->set_markup_with_mnemonic(_("_A:"));
        _s[4]->set_tooltip_text(_("Alpha (opacity)"));
        _b[4]->set_tooltip_text(_("Alpha (opacity)"));

        _s[0]->setMap(nullptr);
        _l[4]->set_visible(true);
        _s[4]->set_visible(true);
        _b[4]->set_visible(true);
        _updating = true;

        setScaled(_a[0], c[0]);
        setScaled(_a[1], c[1]);
        setScaled(_a[2], c[2]);
        setScaled(_a[3], c[3]);
        setScaled(_a[4], c[4]);

        _updateSliders(CSC_CHANNELS_ALL);
        _updating = false;
    } else if constexpr (MODE == SPColorScalesMode::HSLUV) {
        _setRangeLimit(100.0);

        _l[0]->set_markup_with_mnemonic(_("_H*:"));
        _s[0]->set_tooltip_text(_("Hue"));
        _b[0]->set_tooltip_text(_("Hue"));
        _a[0]->set_upper(360.0);

        _l[1]->set_markup_with_mnemonic(_("_S*:"));
        _s[1]->set_tooltip_text(_("Saturation"));
        _b[1]->set_tooltip_text(_("Saturation"));

        _l[2]->set_markup_with_mnemonic(_("_L*:"));
        _s[2]->set_tooltip_text(_("Lightness"));
        _b[2]->set_tooltip_text(_("Lightness"));

        _l[3]->set_markup_with_mnemonic(_("_A:"));
        _s[3]->set_tooltip_text(_("Alpha (opacity)"));
        _b[3]->set_tooltip_text(_("Alpha (opacity)"));

        _s[0]->setMap(hsluvHueMap(0.0f, 0.0f, &_sliders_maps[0]));
        _s[1]->setMap(hsluvSaturationMap(0.0f, 0.0f, &_sliders_maps[1]));
        _s[2]->setMap(hsluvLightnessMap(0.0f, 0.0f, &_sliders_maps[2]));

        _l[4]->set_visible(false);
        _s[4]->set_visible(false);
        _b[4]->set_visible(false);
        _updating = true;

        setScaled(_a[0], c[0]);
        setScaled(_a[1], c[1]);
        setScaled(_a[2], c[2]);
        setScaled(_a[3], c[3]);

        _updateSliders(CSC_CHANNELS_ALL);
        _updating = false;
    } else if constexpr (MODE == SPColorScalesMode::OKLAB) {
        _setRangeLimit(100.0);

        _l[0]->set_markup_with_mnemonic(_("_H<sub>OK</sub>:"));
        _s[0]->set_tooltip_text(_("Hue"));
        _b[0]->set_tooltip_text(_("Hue"));
        _a[0]->set_upper(360.0);

        _l[1]->set_markup_with_mnemonic(_("_S<sub>OK</sub>:"));
        _s[1]->set_tooltip_text(_("Saturation"));
        _b[1]->set_tooltip_text(_("Saturation"));

        _l[2]->set_markup_with_mnemonic(_("_L<sub>OK</sub>:"));
        _s[2]->set_tooltip_text(_("Lightness"));
        _b[2]->set_tooltip_text(_("Lightness"));

        _l[3]->set_markup_with_mnemonic(_("_A:"));
        _s[3]->set_tooltip_text(_("Alpha (opacity)"));
        _b[3]->set_tooltip_text(_("Alpha (opacity)"));

        _l[4]->set_visible(false);
        _s[4]->set_visible(false);
        _b[4]->set_visible(false);
        _updating = true;

        for (size_t i : {0, 1, 2}) {
            setScaled(_a[i], c[i]);
        }
        setScaled(_a[3], c.getOpacity());

        _updateSliders(CSC_CHANNELS_ALL);
        _updating = false;
    } else {
        g_warning("file %s: line %d: Illegal color selector mode", __FILE__, __LINE__);
    }

    if (no_alpha) {
        auto alpha_index = c.getOpacityChannel();
        _l[alpha_index]->set_visible(false);
        _s[alpha_index]->set_visible(false);
        _b[alpha_index]->set_visible(false);
    }
}

template <SPColorScalesMode MODE>
SPColorScalesMode ColorScales<MODE>::getMode() const { return MODE; }

template <SPColorScalesMode MODE>
void ColorScales<MODE>::_sliderAnyGrabbed()
{
    if (_updating) { return; }

    if (!_dragging) {
        _dragging = true;
        _color->grab();
    }
}

template <SPColorScalesMode MODE>
void ColorScales<MODE>::_sliderAnyReleased()
{
    if (_updating) { return; }

    if (_dragging) {
        _dragging = false;
        _color->release();
    }
}

template <SPColorScalesMode MODE>
void ColorScales<MODE>::_sliderAnyChanged()
{
    if (_updating) { return; }

    _recalcColor();
}

template <SPColorScalesMode MODE>
void ColorScales<MODE>::_adjustmentChanged(int channel)
{
    if (_updating) { return; }

    _updateSliders(channel);
    _recalcColor();
}

template <SPColorScalesMode MODE>
void ColorScales<MODE>::_wheelChanged()
{
    if constexpr (
            MODE == SPColorScalesMode::NONE ||
            MODE == SPColorScalesMode::RGB ||
            MODE == SPColorScalesMode::CMYK)
    {
        return;
    }

    if (_updating) { return; }

    _updating = true;
    _color_changed.block();

    // Color
    if (_wheel->isAdjusting()) {
        _color->grab();
    } else {
        _color->release();
    }
    _color->setAll(_wheel->getColor());

    // Sliders
    _updateDisplay(false);

    _color_changed.unblock();
    _updating = false;
}

template <SPColorScalesMode MODE>
void ColorScales<MODE>::_updateSliders(guint channel_pin)
{
    auto color = _getColor();

    // Opacity is not shown in color sliders
    if (channel_pin == color.getOpacityChannel())
        return;
    color.enableOpacity(false);

    if constexpr (MODE == SPColorScalesMode::HSLUV) {
        if (channel_pin != CSC_CHANNEL_H) {
            /* Update hue */
            _s[0]->setMap(hsluvHueMap(color[1], color[2], &_sliders_maps[0]));
        }
        if (channel_pin != CSC_CHANNEL_S) {
            /* Update saturation (scaled chroma) */
            _s[1]->setMap(hsluvSaturationMap(color[0], color[2], &_sliders_maps[1]));
        }
        if (channel_pin != CSC_CHANNEL_V) {
            /* Update lightness */
            _s[2]->setMap(hsluvLightnessMap(color[0], color[1], &_sliders_maps[2]));
        }
    } else if constexpr (MODE == SPColorScalesMode::OKLAB) {
        if (channel_pin != CSC_CHANNEL_H) {
            _s[0]->setMap(Inkscape::Colors::Space::render_hue_scale(color[1], color[2], &_sliders_maps[0]));
        }
        if (channel_pin != CSC_CHANNEL_S) {
            _s[1]->setMap(Inkscape::Colors::Space::render_saturation_scale(360.0 * color[0], color[2], &_sliders_maps[1]));
        }
        if (channel_pin != CSC_CHANNEL_V) {
            _s[2]->setMap(Inkscape::Colors::Space::render_lightness_scale(360.0 * color[0], color[1], &_sliders_maps[2]));
        }
    }

    // We request the opacity channel even though color has no opacity, this is ok as Color::set will handle it.
    for (auto &channel : color.getSpace()->getComponents(true)) {
        // Ignore any hue, any mapped and any updated channels
        if (channel.id != "a" && (MODE == SPColorScalesMode::HSLUV || MODE == SPColorScalesMode::OKLAB))
            continue;
        if (channel.scale == 360 || channel.index == channel_pin)
            continue;

        auto low = color;
        auto mid = color;
        auto high = color;
        low.set(channel.index, 0.0);
        mid.set(channel.index, 0.5);
        high.set(channel.index, 1.0);
        _s[channel.index]->setColors(low.toRGBA(), mid.toRGBA(), high.toRGBA());
    }
}

static guchar const *sp_color_scales_hue_map()
{
    static std::array<guchar, 4 * 1024> const map = []() {
        std::array<guchar, 4 * 1024> m;

        guchar *p;
        p = m.data();
        auto color = Colors::Color(Colors::Space::Type::HSL, {0, 1.0, 0.5});
        for (gint h = 0; h < 1024; h++) {
            color.set(0, h / 1024.0);
            auto rgb = *color.converted(Colors::Space::Type::RGB);
            *p++ = SP_COLOR_F_TO_U(rgb[0]);
            *p++ = SP_COLOR_F_TO_U(rgb[1]);
            *p++ = SP_COLOR_F_TO_U(rgb[2]);
            *p++ = 0xFF;
        }

        return m;
    }();

    return map.data();
}

static void sp_color_interp(guchar *out, gint steps, gfloat *start, gfloat *end)
{
    gfloat s[3] = {
        (end[0] - start[0]) / steps,
        (end[1] - start[1]) / steps,
        (end[2] - start[2]) / steps
    };

    guchar *p = out;
    for (int i = 0; i < steps; i++) {
        *p++ = SP_COLOR_F_TO_U(start[0] + s[0] * i);
        *p++ = SP_COLOR_F_TO_U(start[1] + s[1] * i);
        *p++ = SP_COLOR_F_TO_U(start[2] + s[2] * i);
        *p++ = 0xFF;
    }
}

// TODO: consider turning this into a generator (without memory allocation).
template <typename T>
static std::vector<T> range (int const steps, T start, T end)
{
    T step = (end - start) / (steps - 1);

    std::vector<T> out;
    out.reserve(steps);

    for (int i = 0; i < steps-1; i++) {
        out.emplace_back(start + step * i);
    }
    out.emplace_back(end);

    return out;
}

static guchar const *sp_color_scales_hsluv_map(guchar *map,
        std::function<void(float*, float)> callback)
{
    // Only generate 21 colors and interpolate between them to get 1024
    constexpr static int STEPS = 21;
    constexpr static int COLORS = (STEPS+1) * 3;
    static auto const steps = range<float>(STEPS+1, 0.f, 1.f);

    // Generate color steps
    gfloat colors[COLORS];
    for (int i = 0; i < STEPS+1; i++) {
        callback(colors+(i*3), steps[i]);
    }

    for (int i = 0; i < STEPS; i++) {
        int a = steps[i] * 1023,
            b = steps[i+1] * 1023;
        sp_color_interp(map+(a * 4), b-a, colors+(i*3), colors+((i+1)*3));
    }

    return map;
}

template <SPColorScalesMode MODE>
guchar const *ColorScales<MODE>::hsluvHueMap(gfloat s, gfloat l,
        std::array<guchar, 4 * 1024> *map)
{
    auto color = Colors::Color(Colors::Space::Type::HSLUV, {0, s, l});
    return sp_color_scales_hsluv_map(map->data(), [&color] (float *colors, float h) {
        color.set(0, (double)h);
        auto rgb = *color.converted(Colors::Space::Type::RGB);
        *colors++ = rgb[0];
        *colors++ = rgb[1];
        *colors++ = rgb[2];
    });
}

template <SPColorScalesMode MODE>
guchar const *ColorScales<MODE>::hsluvSaturationMap(gfloat h, gfloat l,
        std::array<guchar, 4 * 1024> *map)
{
    auto color = Colors::Color(Colors::Space::Type::HSLUV, {h, 0, l});
    return sp_color_scales_hsluv_map(map->data(), [&color] (float *colors, float s) {
        color.set(1, (double)s);
        auto rgb = *color.converted(Colors::Space::Type::RGB);
        *colors++ = rgb[0];
        *colors++ = rgb[1];
        *colors++ = rgb[2];
    });
}

template <SPColorScalesMode MODE>
guchar const *ColorScales<MODE>::hsluvLightnessMap(gfloat h, gfloat s,
        std::array<guchar, 4 * 1024> *map)
{
    auto color = Colors::Color(Colors::Space::Type::HSLUV, {h, s, 0});
    return sp_color_scales_hsluv_map(map->data(), [&color] (float *colors, float l) {
        color.set(2, (double)l);
        auto rgb = *color.converted(Colors::Space::Type::RGB);
        *colors++ = rgb[0];
        *colors++ = rgb[1];
        *colors++ = rgb[2];
    });
}

template <SPColorScalesMode MODE>
ColorScalesFactory<MODE>::ColorScalesFactory()
{}

template <SPColorScalesMode MODE>
Gtk::Widget *ColorScalesFactory<MODE>::createWidget(Inkscape::UI::SelectedColor color, bool no_alpha) const
{
    Gtk::Widget *w = Gtk::make_managed<ColorScales<MODE>>(std::move(color), no_alpha);
    return w;
}

template <SPColorScalesMode MODE>
Glib::ustring ColorScalesFactory<MODE>::modeName() const
{
    if constexpr (MODE == SPColorScalesMode::RGB) {
        return gettext(ColorScales<>::SUBMODE_NAMES[1]);
    } else if constexpr (MODE == SPColorScalesMode::HSL) {
        return gettext(ColorScales<>::SUBMODE_NAMES[2]);
    } else if constexpr (MODE == SPColorScalesMode::CMYK) {
        return gettext(ColorScales<>::SUBMODE_NAMES[3]);
    } else if constexpr (MODE == SPColorScalesMode::HSV) {
        return gettext(ColorScales<>::SUBMODE_NAMES[4]);
    } else if constexpr (MODE == SPColorScalesMode::HSLUV) {
        return gettext(ColorScales<>::SUBMODE_NAMES[5]);
    } else if constexpr (MODE == SPColorScalesMode::OKLAB) {
        return gettext(ColorScales<>::SUBMODE_NAMES[6]);
    } else {
        return gettext(ColorScales<>::SUBMODE_NAMES[0]);
    }
}

// Explicit instantiations
template class ColorScales<SPColorScalesMode::NONE>;
template class ColorScales<SPColorScalesMode::RGB>;
template class ColorScales<SPColorScalesMode::HSL>;
template class ColorScales<SPColorScalesMode::CMYK>;
template class ColorScales<SPColorScalesMode::HSV>;
template class ColorScales<SPColorScalesMode::HSLUV>;
template class ColorScales<SPColorScalesMode::OKLAB>;

template class ColorScalesFactory<SPColorScalesMode::NONE>;
template class ColorScalesFactory<SPColorScalesMode::RGB>;
template class ColorScalesFactory<SPColorScalesMode::HSL>;
template class ColorScalesFactory<SPColorScalesMode::CMYK>;
template class ColorScalesFactory<SPColorScalesMode::HSV>;
template class ColorScalesFactory<SPColorScalesMode::HSLUV>;
template class ColorScalesFactory<SPColorScalesMode::OKLAB>;

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
