// SPDX-License-Identifier: GPL-2.0-or-later

#include <set>
#include <glibmm/markup.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/searchentry.h>
#include <pangomm.h>
#include <libnrtype/font-factory.h>
#include "font-list.h"
#include "ui/builder-utils.h"
#include "ui/icon-loader.h"

using Inkscape::UI::create_builder;

namespace Inkscape {
namespace UI {
namespace Widget {

// Attempt to estimate how heavy given typeface is by drawing some capital letters and counting
// black pixels (alpha channel). This is imperfect, but a reasonable proxy for font weight.
double calculate_font_weight(Pango::FontDescription& desc) {
    // pixmap with enough room for a few characters; the rest will be cropped
    auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, 128, 64);
    auto context = Cairo::Context::create(surface);
    auto layout = Pango::Layout::create(context);
    const char* txt = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    layout->set_text(txt);
    // Glib::ustring name(pango_font_family_get_name(ff) + (',' + style));
    // Pango::FontDescription desc(name);
    desc.set_size(22 * PANGO_SCALE);
    layout->set_font_description(desc);
    context->move_to(1, 1);
    layout->show_in_cairo_context(context);
    surface->flush();

/* test to check bounds:
static int ii = 0;
std::ostringstream ost;
if (desc.to_string().find("Roboto") != Glib::ustring::npos) {
  ost << "/Users/mike/junk/test-" << ii++ << '-' << desc.to_string() << ".png";
  surface->write_to_png(ost.str().c_str());
}
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

struct FontListColumnModel : public Gtk::TreeModelColumnRecord {
    Gtk::TreeModelColumn<FontList::FontInfo*> font;
    
    FontListColumnModel() {
        add(font);
    }
};

FontListColumnModel g_column_model;

Glib::ustring get_full_name(const FontList::FontInfo& font_info) {
    auto desc = font_info.face->describe();
    // desc.unset_fields(Pango::FONT_MASK_SIZE);
    return /*font_info.ff->get_name() + ' ' +*/ desc.to_string(); // font_info.face->get_name();
}

Pango::FontDescription get_font_description(Glib::RefPtr<Pango::FontFamily>& ff, Glib::RefPtr<Pango::FontFace>& face) {
    // return face->describe();
    auto desc = Pango::FontDescription(ff->get_name() + ", " + face->get_name());
    desc.unset_fields(Pango::FONT_MASK_SIZE);
    return desc;
}

void get_cell_data_func(Gtk::CellRenderer* renderer, const Gtk::TreeIter& iter) {
    Gtk::TreeModel::Row row = *iter;
    FontList::FontInfo* font = row[g_column_model.font];

    auto name = get_full_name(*font);
    auto fname = Glib::Markup::escape_text(name);
    auto font_desc = Glib::Markup::escape_text(get_font_description(font->ff, font->face).to_string());
    auto markup = "<span size='150%' font='" + font_desc + "'>" + fname + "</span>";
    renderer->set_property("markup", markup);
}

static void set_icon(Gtk::Button& btn, gchar const* pixmap) {
    if (Gtk::Image* img = sp_get_icon_image(pixmap, Gtk::ICON_SIZE_BUTTON)) {
        btn.set_image(*img);
    }
    else {
        g_warning("No icon found: %s", pixmap);
    }
}

FontList::FontList() :
    _builder(create_builder("font-list.glade")),
    _main_grid(get_widget<Gtk::Grid>(_builder, "main-grid")),
    _font_list(get_widget<Gtk::TreeView>(_builder, "font-list"))
{
    set_hexpand();
    set_vexpand();
    pack_start(_main_grid, true, true);
    set_margin_start(0);
    set_margin_end(0);
    set_margin_top(5);
    set_margin_bottom(0);
    show_all();

    auto& options = get_widget<Gtk::Button>(_builder, "btn-options");
    set_icon(options, "gears-symbolic");
    auto options_grid = &get_widget<Gtk::Grid>(_builder, "options-grid");
    options.signal_clicked().connect([=](){
        if (options_grid->is_visible()) options_grid->hide(); else options_grid->show();
    });

    std::pair<const char*, Sort> sorting[3] = {
        {"sort-by-name", Sort::by_name}, {"sort-by-weight", Sort::by_weight}, {"sort-by-width", Sort::by_width}
    };
    for (auto&& el : sorting) {
        auto& item = get_widget<Gtk::MenuItem>(_builder, el.first);
        item.signal_activate().connect([=](){
            sort_fonts(el.second);
        });
    }
    // auto& by_name = get_widget<Gtk::MenuItem>(_builder, "sort-by-name");
    // by_name.signal_activate().connect([=](){
    //     sort_fonts(Sort::by_name);
    // });

    auto search = &get_widget<Gtk::SearchEntry>(_builder, "font-search");
    search->signal_changed().connect([=](){ filter(); });

    _font_list_store = Gtk::ListStore::create(g_column_model);

    _text_column.pack_start(_cell_renderer, false);
    _text_column.set_fixed_width(120); // limit minimal width to keep entire dialog narrow; column can still grow
    _text_column.add_attribute(_cell_renderer, "text", 0);
    _text_column.set_cell_data_func(_cell_renderer, &get_cell_data_func);

    _font_list.append_column("font", g_column_model.font);
    _font_list.append_column(_text_column);
    _font_list.set_model(_font_list_store);

g_message("get font families");
    auto families = FontFactory::get().GetUIFamilies();
g_message("get font styles");
    for (auto [key, font_family] : families) {
        auto ff = Glib::wrap(font_family);
        auto faces = ff->list_faces();
        std::set<std::string> styles;
        for (auto face : faces) {
            if (face->is_synthesized()) continue;
            // ff->is_monospace
            // face->is

            // auto desc = get_font_description(ff, face);
            auto desc = face->describe();
            desc.unset_fields(Pango::FONT_MASK_SIZE);
            std::string key = desc.to_string();
            if (styles.count(key)) continue;

            styles.insert(key);
// g_message("> %s", desc.to_string().c_str());
            desc = get_font_description(ff, face);
            auto weight = calculate_font_weight(desc);

            desc = get_font_description(ff, face);
            auto width = calculate_font_width(desc);

            FontInfo info { ff, face, weight, width };
            _fonts.emplace_back(info);
        }
/*
        GList* list = font_factory::Default()->GetUIStyles(ff);

        for (auto temp = list; temp; temp = temp->next) {
            auto style = static_cast<StyleNames*>(temp->data);
            Glib::ustring name(ff->get_name() + ',' + style->CssName);
            Pango::FontDescription desc(name);
            auto weight = calculate_font_weight(desc, style->CssName);
            auto width = calculate_font_width(desc, style->CssName);
            FontInfo info { ff, style->CssName, style->DisplayName, weight, width };
            _fonts.emplace_back(info);
        }
*/
    }
g_message("get font done");

    sort_fonts(Sort::by_name);
}

void FontList::sort_fonts(Sort order) {
    const char* icon = nullptr;

    switch (order) {
        case Sort::by_name:
            std::sort(begin(_fonts), end(_fonts), [](const FontInfo& a, const FontInfo& b) {
                auto na = a.ff->get_name();
                auto nb = b.ff->get_name();
                if (na != nb) {
                    return na < nb;
                }
                return get_font_style_order(a.face->describe()) < get_font_style_order(b.face->describe());
            });
            icon = "sort-alphabetically-symbolic";
            break;

        case Sort::by_weight:
            std::sort(begin(_fonts), end(_fonts), [](const FontInfo& a, const FontInfo& b) { return a.weight < b.weight; });
            icon = "sort-by-weight-symbolic";
            break;

        case Sort::by_width:
            std::sort(begin(_fonts), end(_fonts), [](const FontInfo& a, const FontInfo& b) { return a.width < b.width; });
            icon = "sort-by-width-symbolic";
            break;

        default:
            g_warning("Missing case in sort_fonts");
            break;
    }

    auto& sort = get_widget<Gtk::MenuButton>(_builder, "btn-sort");
    if (icon) set_icon(sort, icon);

    filter();
}

void FontList::filter() {
    auto search = &get_widget<Gtk::SearchEntry>(_builder, "font-search");
    filter(search->get_text());
}

void FontList::filter(Glib::ustring text) {
    auto filter = text.lowercase();

    _font_list_store->freeze_notify();
    _font_list_store->clear();

    for (auto&& f : _fonts) {
        auto name = get_full_name(f);
        if (text.empty() || name.lowercase().find(filter) != Glib::ustring::npos) {
            Gtk::TreeModel::iterator treeModelIter = _font_list_store->append();
            (*treeModelIter)[g_column_model.font] = &f;
        }
    }

    _font_list_store->thaw_notify();
}

}}} // namespaces
