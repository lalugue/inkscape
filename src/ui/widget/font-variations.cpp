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

#include <glibmm/refptr.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/enums.h>
#include <gtkmm/object.h>
#include <gtkmm/sizegroup.h>
#include <gtkmm/spinbutton.h>
#include <iostream>
#include <iomanip>

#include <gtkmm.h>
#include <glibmm/i18n.h>

#include <libnrtype/font-instance.h>
#include "libnrtype/font-factory.h"

#include "font-variations.h"

// For updating from selection
#include "desktop.h"
#include "object/sp-text.h"
#include "svg/css-ostringstream.h"

namespace Inkscape {
namespace UI {
namespace Widget {

FontVariationAxis::FontVariationAxis(Glib::ustring name_, OTVarAxis const &axis)
    : name(std::move(name_))
{

    // std::cout << "FontVariationAxis::FontVariationAxis:: "
    //           << " name: " << name
    //           << " min:  " << axis.minimum
    //           << " def:  " << axis.def
    //           << " max:  " << axis.maximum
    //           << " val:  " << axis.set_val << std::endl;

    set_column_spacing(3);

    label = Gtk::make_managed<Gtk::Label>(name + ":");
    label->set_xalign(0.0f);
    add(*label);

    edit = Gtk::make_managed<Gtk::SpinButton>();
    edit->set_max_width_chars(5);
    edit->set_valign(Gtk::ALIGN_CENTER);
    edit->set_margin_top(2);
    edit->set_margin_bottom(2);
    add(*edit);

    precision = 2 - int( log10(axis.maximum - axis.minimum));
    if (precision < 0) precision = 0;

    auto adj = Gtk::Adjustment::create(axis.set_val, axis.minimum, axis.maximum);
    adj->set_step_increment(1);
    edit->set_adjustment(adj);
    edit->set_digits(precision);

    auto adj_scale = Gtk::Adjustment::create(axis.set_val, axis.minimum, axis.maximum);
    scale = Gtk::make_managed<Gtk::Scale>();
    scale->set_digits(precision);
    scale->set_hexpand(true);
    scale->set_adjustment(adj_scale);
    scale->get_style_context()->add_class("small-slider");
    scale->set_draw_value(false);
    add( *scale );

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
        auto const axis = Gtk::make_managed<FontVariationAxis>(a.first, a.second);
        axes.push_back( axis );
        add( *axis );
        size_group->add_widget( *(axis->get_label()) ); // Keep labels the same width
        size_group_edit->add_widget(*axis->get_editbox());
        axis->get_scale()->get_adjustment()->signal_value_changed().connect(
            [=](){ on_variations_change(); }
        );
    }

    show_all_children();
}

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

void
FontVariations::on_variations_change() {
    // std::cout << "FontVariations::on_variations_change: " << get_css_string() << std::endl;;
    signal_changed.emit ();
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
