// SPDX-License-Identifier: GPL-2.0-or-later

#include "font-discovery.h"
#include "io/resource.h"

#include <libnrtype/font-factory.h>
#include <libnrtype/font-instance.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>
#include <unordered_map>

#ifdef G_OS_WIN32
#include <filesystem>
namespace filesystem = std::filesystem;
#else
// Waiting for compiler on MacOS to catch up to C++x17
#include <boost/filesystem.hpp>
namespace filesystem = boost::filesystem;
#endif

namespace Inkscape {

// Attempt to estimate how heavy given typeface is by drawing some capital letters and counting
// black pixels (alpha channel). This is imperfect, but reasonable proxy for font weight, as long
// as Pango can instantiate correct font.
double calculate_font_weight(Pango::FontDescription& desc, double caps_height) {
// g_message("fch: %f\n", caps_height);
    // pixmap with enough room for a few characters; the rest will be cropped
    auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, 128, 64);
    auto context = Cairo::Context::create(surface);
    auto layout = Pango::Layout::create(context);
    const char* txt = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    layout->set_text(txt);
    // Glib::ustring name(pango_font_family_get_name(ff) + (',' + style));
    // Pango::FontDescription desc(name);
    auto size = 22 * PANGO_SCALE;
    if (caps_height > 0) {
        size /= caps_height;
    }
    desc.set_size(size);
    layout->set_font_description(desc);
    context->move_to(1, 1);
    layout->show_in_cairo_context(context);
    surface->flush();

/* test to check bounds:
static int ii = 0;
std::ostringstream ost;
// if (desc.to_string().find("Roboto") != Glib::ustring::npos) {
  ost << "/Users/mike/junk/test-" << ii++ << '-' << desc.to_string() << ".png";
  surface->write_to_png(ost.str().c_str());
// }
*/

    auto pixels = surface->get_data();
    auto width = surface->get_width();
    auto stride = surface->get_stride() / width;
    auto height = surface->get_height();
    double sum = 0;
    for (auto y = 0; y < height; ++y) {
        for (auto x = 0; x < width; ++x) {
            sum += pixels[3]; // read alpha
            pixels += stride;
        }
    }
    auto weight = sum / (width * height);
// g_message("%d %d %d - %s %f", width, stride, height, name.c_str(), weight);
    return weight;
}

// calculate width of a A-Z string to try to measure average character width
double calculate_font_width(Pango::FontDescription& desc) {
    auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, 1, 1);
    auto context = Cairo::Context::create(surface);
    auto layout = Pango::Layout::create(context);
    const char* txt = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    layout->set_text(txt);
    // Glib::ustring name(pango_font_family_get_name(ff) + (',' + style));
    // Pango::FontDescription desc(name);
    desc.set_size(72 * PANGO_SCALE);
    layout->set_font_description(desc);
    // layout->show_in_cairo_context(context);
    Pango::Rectangle ink, rect;
    layout->get_extents(ink, rect);
// g_message("%d %d - %s", ink.get_width() / PANGO_SCALE, rect.get_width() / PANGO_SCALE, name.c_str());
    return static_cast<double>(ink.get_width()) / PANGO_SCALE / strlen(txt);
}

// calculate value to order font's styles
int get_font_style_order(const Pango::FontDescription& desc) {
    return
        desc.get_weight()  * 1'000'000 +
        desc.get_style()   * 10'000 +
        desc.get_stretch() * 100 +
        desc.get_variant();
}

// sort fonts in requested order, in-place
void sort_fonts(std::vector<FontInfo>& fonts, FontOrder order) {
    switch (order) {
        case FontOrder::by_name:
            std::sort(begin(fonts), end(fonts), [](const FontInfo& a, const FontInfo& b) {
                auto na = a.ff->get_name();
                auto nb = b.ff->get_name();
                if (na != nb) {
                    return na < nb;
                }
                return get_font_style_order(a.face->describe()) < get_font_style_order(b.face->describe());
            });
            break;

        case FontOrder::by_weight:
            std::sort(begin(fonts), end(fonts), [](const FontInfo& a, const FontInfo& b) { return a.weight < b.weight; });
            break;

        case FontOrder::by_width:
            std::sort(begin(fonts), end(fonts), [](const FontInfo& a, const FontInfo& b) { return a.width < b.width; });
            break;

        default:
            g_warning("Missing case in sort_fonts");
            break;
    }
}

Pango::FontDescription get_font_description(const Glib::RefPtr<Pango::FontFamily>& ff, const Glib::RefPtr<Pango::FontFace>& face) {
    auto name = face->get_name();
    auto desc = Pango::FontDescription(name.empty() ? ff->get_name() : ff->get_name() + ", " + name);
    desc.unset_fields(Pango::FONT_MASK_SIZE);
    return desc;
}

const char font_cache[] = "font-cache-v1.ini";

void save_font_cache(const std::vector<FontInfo>& fonts) {
    auto keyfile = std::make_unique<Glib::KeyFile>();

    Glib::ustring mono("monospaced");
    Glib::ustring oblique("oblique");
    Glib::ustring weight("weight");
    Glib::ustring width("width");

    for (auto&& font : fonts) {
        auto desc = get_font_description(font.ff, font.face);
        auto group = desc.to_string(); 
        keyfile->set_boolean(group, mono, font.monospaced);
        keyfile->set_boolean(group, oblique, font.oblique);
        keyfile->set_double(group, weight, font.weight);
        keyfile->set_double(group, width, font.width);
    }

    std::string filename = Glib::build_filename(Inkscape::IO::Resource::profile_path(), font_cache);
    keyfile->save_to_file(filename);
}

std::unordered_map<std::string, FontInfo> load_cached_font_info() {
    std::unordered_map<std::string, FontInfo> info;

    try {
        auto keyfile = std::make_unique<Glib::KeyFile>();
        std::string filename = Glib::build_filename(Inkscape::IO::Resource::profile_path(), font_cache);

#ifdef G_OS_WIN32
        bool exists = filesystem::exists(filesystem::u8path(filename));
#else
        bool exists = filesystem::exists(filesystem::path(filename));
#endif

        if (exists && keyfile->load_from_file(filename)) {

            Glib::ustring mono("monospaced");
            Glib::ustring oblique("oblique");
            Glib::ustring weight("weight");
            Glib::ustring width("width");

            for (auto&& group : keyfile->get_groups()) {
                FontInfo font;
                font.monospaced = keyfile->get_boolean(group, mono);
                font.oblique = keyfile->get_boolean(group, oblique);
                font.weight = keyfile->get_double(group, weight);
                font.width = keyfile->get_double(group, width);

                info[group.raw()] = font;
            }
        }
    }
    catch (Glib::Error &error) {
        std::cerr << G_STRFUNC << ": font cache not loaded - " << error.what().raw() << std::endl;
    }

    return info;
}

std::vector<FontInfo> get_all_fonts() {
    std::vector<FontInfo> fonts;
    auto cache = load_cached_font_info();

    std::vector<PangoFontFamily*> families;
    FontFactory::get().GetUIFamilies(families);

    bool update_cache = false;

    for (auto font_family : families) {
        auto ff = Glib::wrap(font_family);
        auto faces = ff->list_faces();
        std::set<std::string> styles;
        for (auto face : faces) {
            if (face->is_synthesized()) continue;

            auto desc = face->describe();
            desc.unset_fields(Pango::FONT_MASK_SIZE);
            std::string key = desc.to_string();
            if (styles.count(key)) continue;

            styles.insert(key);

            FontInfo info = { ff, face };

            desc = get_font_description(ff, face);
            auto it = cache.find(desc.to_string().raw());
            if (it == cache.end()) {
                // font not found in a cache; calculate metrics

                update_cache = true;

                double caps_height = 0.0;

                try {
                    auto font = FontFactory::get().Face(desc.gobj());
                    info.monospaced = font->is_fixed_width();
                    info.oblique = font->is_oblique();
                    auto glyph = font->LoadGlyph(font->MapUnicodeChar('E'));
                    if (glyph) {
                        // bbox: L T R B
                        caps_height = glyph->bbox[3] - glyph->bbox[1]; // caps height normalized to 0..1
                        auto& path = glyph->pathvector;
    // g_message("ch: %f", caps_height);
    // caps_height = font->get_E_height();
                        // auto& b = glyph->bbox;
                    }
                }
                catch (...) {
                    g_warning("Error loading font %s", key.c_str());
                }
                desc = get_font_description(ff, face);
                info.weight = calculate_font_weight(desc, caps_height);

                desc = get_font_description(ff, face);
                info.width = calculate_font_width(desc);
            }
            else {
                // font in a cache already
                info = it->second;
            }

            info.ff = ff;
            info.face = face;
            fonts.emplace_back(info);
        }
    }

    if (update_cache) {
        save_font_cache(fonts);
    }

    return fonts;
}

} // namespace
