// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author:
 *   Felipe Corrêa da Silva Sanches <juca@members.fsf.org>
 *   Tavmjong Bah <tavmjong@free.fr>
 *
 * Copyright (C) 2018 Felipe Corrêa da Silva Sanches, Tavmong Bah
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <cmath>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/enums.h>
#include <gtkmm/object.h>
#include <gtkmm/sizegroup.h>
#include <gtkmm/spinbutton.h>
#include <iostream>
#include <iomanip>
#include <map>

#include <gtkmm.h>
#include <glibmm/i18n.h>

#include <libnrtype/font-instance.h>
#include <utility>
#include "libnrtype/font-factory.h"

#include "font-variations.h"

// For updating from selection
#include "desktop.h"
#include "object/sp-text.h"
#include "svg/css-ostringstream.h"

namespace Inkscape {
namespace UI {
namespace Widget {

std::pair<Glib::ustring, Glib::ustring> get_axis_name(const Glib::ustring& abbr) {
    // Transformed axis names;
    // from https://fonts.google.com/knowledge/using_type/introducing_parametric_axes
    // CC BY-SA 4.0
    static std::map<std::string, std::pair<Glib::ustring, Glib::ustring>> map = {
        // “Grade” (GRAD in CSS) is an axis that can be used to alter stroke thicknesses (or other forms)
        // without affecting the type's overall width, inter-letter spacing, or kerning — unlike altering weight.
        {"GRAD", std::make_pair(_("Grade"), _("Alter stroke thicknesses (or other forms) without affecting the type's overall width"))},
        // “Parametric Thick Stroke”, XOPQ, is a reference to its logical name, “X Opaque”,
        // which describes how it alters the opaque stroke forms of glyphs typically in the X dimension
        {"XOPQ", std::make_pair(_("X-opaque"), _("Alter the opaque stroke forms of glyphs in the X dimension"))},
        // “Parametric Thin Stroke”, YOPQ, is a reference to its logical name, “Y Opaque”,
        // which describes how it alters the opaque stroke forms of glyphs typically in the Y dimension 
        {"YOPQ", std::make_pair(_("Y-opaque"), _("Alter the opaque stroke forms of glyphs in the Y dimension"))},
        // “Parametric Counter Width”, XTRA, is a reference to its logical name, “X-Transparent,”
        // which describes how it alters a font’s transparent spaces (also known as negative shapes)
        // inside and around all glyphs along the X dimension
        {"XTRA", std::make_pair(_("X-transparent"), _("Alter the transparent spaces inside and around all glyphs along the X dimension"))},
        // “Parametric Lowercase Height”
        {"YTLC", std::make_pair(_("Lowercase height"), _("Vary the height of counters and other spaces between the baseline and x-height"))},
        // “Parametric Uppercase Counter Height”
        {"YTUC", std::make_pair(_("Uppercase height"), _("Vary the height of uppercase letterforms"))},
        // “Parametric Ascender Height”
        {"YTAS", std::make_pair(_("Ascender height"), _("Vary the height of lowercase ascenders"))},
        // “Parametric Descender Depth”
        {"YTDE", std::make_pair(_("Descender depth"), _("Vary the depth of lowercase descenders"))},
        // “Parametric Figure Height”
        {"YTFI", std::make_pair(_("Figure height"), _("Vary the height of figures"))},
        // “Optical Size”
        // Optical sizes in a variable font are different versions of a typeface optimized for use at singular specific sizes,
        // such as 14 pt or 144 pt. Small (or body) optical sizes tend to have less stroke contrast, more open and wider spacing,
        // and a taller x-height than those of their large (or display) counterparts.
        {"OpticalSize", std::make_pair(_("Optical size"), _("Optimize the typeface for use at specific size"))},
        // Slant controls the font file’s slant parameter for oblique styles.
        {"Slant", std::make_pair(_("Slant"), _("Controls the font file’s slant parameter for oblique styles"))},
        // Weight controls the font file’s weight parameter. 
        {"Weight", std::make_pair(_("Weight"), _("Controls the font file’s weight parameter"))},
        // Width controls the font file’s width parameter.
        {"Width", std::make_pair(_("Width"), _("Controls the font file’s width parameter"))},
    };

    auto it = map.find(abbr.raw());
    if (it != end(map)) {
        return it->second;
    }
    else {
        return std::make_pair(abbr, "");
    }
}

FontVariationAxis::FontVariationAxis(Glib::ustring name_, OTVarAxis const &axis, Glib::ustring label_, Glib::ustring tooltip)
    : name(std::move(name_))
{
    // std::cout << "FontVariationAxis::FontVariationAxis:: "
    //           << " name: " << name
    //           << " min:  " << axis.minimum
    //           << " def:  " << axis.def
    //           << " max:  " << axis.maximum
    //           << " val:  " << axis.set_val << std::endl;

    set_column_spacing(3);

    label = Gtk::make_managed<Gtk::Label>(label_ + ":");
    label->set_tooltip_text(tooltip);
    label->set_xalign(0.0f); // left-align
    add(*label);

    edit = Gtk::make_managed<Gtk::SpinButton>();
    edit->set_max_width_chars(5);
    edit->set_valign(Gtk::ALIGN_CENTER);
    edit->set_margin_top(2);
    edit->set_margin_bottom(2);
    edit->set_tooltip_text(tooltip);
    add(*edit);

    auto magnitude = static_cast<int>(log10(axis.maximum - axis.minimum));
    precision = 2 - magnitude;
    if (precision < 0) precision = 0;

    auto adj = Gtk::Adjustment::create(axis.set_val, axis.minimum, axis.maximum);
    auto step = pow(10.0, -precision);
    adj->set_step_increment(step);
    adj->set_page_increment(step * 10.0);
    edit->set_adjustment(adj);
    edit->set_digits(precision);

    auto adj_scale = Gtk::Adjustment::create(axis.set_val, axis.minimum, axis.maximum);
    adj_scale->set_step_increment(step);
    adj_scale->set_page_increment(step * 10.0);
    scale = Gtk::make_managed<Gtk::Scale>();
    scale->set_digits (precision);
    scale->set_hexpand(true);
    scale->set_adjustment(adj_scale);
    scale->get_style_context()->add_class("small-slider");
    scale->set_draw_value(false);
    add(*scale);

    // sync slider with spin button
    g_object_bind_property(adj->gobj(), "value", adj_scale->gobj(), "value", GBindingFlags(G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL));

    def = axis.def; // Default value
}


// ------------------------------------------------------------- //

FontVariations::FontVariations () :
    Gtk::Grid ()
{
    // std::cout << "FontVariations::FontVariations" << std::endl;
    set_orientation( Gtk::ORIENTATION_VERTICAL );
    set_name ("FontVariations");
    size_group = Gtk::SizeGroup::create(Gtk::SIZE_GROUP_HORIZONTAL);
    size_group_edit = Gtk::SizeGroup::create(Gtk::SIZE_GROUP_HORIZONTAL);
    show_all_children();
}


// Update GUI based on query.
void FontVariations::update(Glib::ustring const &font_spec)
{
    auto children = get_children();
    for (auto child : children) {
        if (auto group = dynamic_cast<FontVariationAxis*>(child)) {
            size_group->remove_widget(*group->get_label());
            size_group_edit->remove_widget(*group->get_editbox());
        }
        remove(*child);
    }
    axes.clear();

    auto res = FontFactory::get().FaceFromFontSpecification(font_spec.c_str());
    if (!res) return;

    for (auto &a : res->get_opentype_varaxes()) {
        // std::cout << "Creating axis: " << a.first << std::endl;
        auto label_tooltip = get_axis_name(a.first);
        auto axis = Gtk::make_managed<FontVariationAxis>(a.first, a.second, label_tooltip.first, label_tooltip.second);
        axes.push_back(axis);
        add(*axis);
        size_group->add_widget(*(axis->get_label())); // Keep labels the same width
        size_group_edit->add_widget(*axis->get_editbox());
        axis->get_scale()->get_adjustment()->signal_value_changed().connect(
            [=](){ signal_changed.emit (); }
        );
    }

    show_all_children();
}

#if false
void
FontVariations::fill_css( SPCSSAttr *css ) {

    // Eventually will want to favor using 'font-weight', etc. but at the moment these
    // can't handle "fractional" values. See CSS Fonts Module Level 4.
    sp_repr_css_set_property(css, "font-variation-settings", get_css_string().c_str());
}

Glib::ustring
FontVariations::get_css_string() {

    Glib::ustring css_string;

    for (auto axis: axes) {
        Glib::ustring name = axis->get_name();

        // Translate the "named" axes. (Additional names in 'stat' table, may need to handle them.)
        if (name == "Width")  name = "wdth";       // 'font-stretch'
        if (name == "Weight") name = "wght";       // 'font-weight'
        if (name == "OpticalSize") name = "opsz";  // 'font-optical-sizing' Can trigger glyph substitution.
        if (name == "Slant")  name = "slnt";       // 'font-style'
        if (name == "Italic") name = "ital";       // 'font-style' Toggles from Roman to Italic.

        std::stringstream value;
        value << std::fixed << std::setprecision(axis->get_precision()) << axis->get_value();
        css_string += "'" + name + "' " + value.str() + "', ";
    }

    return css_string;
}
#endif

Glib::ustring
FontVariations::get_pango_string(bool include_defaults) const {

    Glib::ustring pango_string;

    if (!axes.empty()) {

        pango_string += "@";

        for (auto axis: axes) {
            if (!include_defaults && axis->get_value() == axis->get_def()) continue;
            Glib::ustring name = axis->get_name();

            // Translate the "named" axes. (Additional names in 'stat' table, may need to handle them.)
            if (name == "Width")  name = "wdth";       // 'font-stretch'
            if (name == "Weight") name = "wght";       // 'font-weight'
            if (name == "OpticalSize") name = "opsz";  // 'font-optical-sizing' Can trigger glyph substitution.
            if (name == "Slant")  name = "slnt";       // 'font-style'
            if (name == "Italic") name = "ital";       // 'font-style' Toggles from Roman to Italic.

            CSSOStringStream str;
            str << std::fixed << std::setprecision(axis->get_precision()) << axis->get_value();
            pango_string += name + "=" + str.str() + ",";
        }

        pango_string.erase (pango_string.size() - 1); // Erase last ',' or '@'
    }

    return pango_string;
}

bool FontVariations::variations_present() const {
    return !axes.empty();
}

Glib::RefPtr<Gtk::SizeGroup> FontVariations::get_size_group(int index) {
    switch (index) {
        case 0: return size_group;
        case 1: return size_group_edit;
        default: return Glib::RefPtr<Gtk::SizeGroup>();
    }
}

} // namespace Widget
} // namespace UI
} // namespace Inkscape

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
