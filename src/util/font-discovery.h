// SPDX-License-Identifier: GPL-2.0-or-later

#include <vector>
#include <pangomm.h>
namespace Inkscape {
struct FontInfo {
    Glib::RefPtr<Pango::FontFamily> ff;
    Glib::RefPtr<Pango::FontFace> face;
    double weight;   // proxy for font weight - how black it is
    double width;    // proxy for font width - how compressed/extended it is
    bool monospaced; // fixed-width font
    bool oblique;    // italic or oblique font
};

enum class FontOrder { by_name, by_weight, by_width };

// Use font factory and cached font details to return a list of all fonts available to Inkscape
std::vector<FontInfo> get_all_fonts();

// change order
void sort_fonts(std::vector<FontInfo>& fonts, FontOrder order);

Pango::FontDescription get_font_description(const Glib::RefPtr<Pango::FontFamily>& ff, const Glib::RefPtr<Pango::FontFace>& face);

} // namespace
