// SPDX-License-Identifier: GPL-2.0-or-later

#include <set>
#include <glibmm/markup.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/searchentry.h>
#include <gtkmm/scale.h>
#include <pangomm.h>
#include <libnrtype/font-factory.h>
#include <libnrtype/font-instance.h>

#include "font-list.h"
#include "ui/builder-utils.h"
#include "ui/icon-loader.h"

using Inkscape::UI::create_builder;

namespace Inkscape {
namespace UI {
namespace Widget {

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

static int s_fsize = 200;
static bool s_fname_show = true;
static Glib::ustring s_sample_text;

void get_cell_data_func(Gtk::CellRenderer* renderer, const Gtk::TreeIter& iter) {
    Gtk::TreeModel::Row row = *iter;
    FontList::FontInfo* font = row[g_column_model.font];

    auto name = get_full_name(*font);
    auto fname = Glib::Markup::escape_text(name);
    auto text = s_sample_text.empty() ? fname : s_sample_text;
    auto font_desc = Glib::Markup::escape_text(get_font_description(font->ff, font->face).to_string());
    auto markup = Glib::ustring::format("<span size='", s_fsize, "%' font='", font_desc, "'>", text, "</span>");
    if (s_fname_show) {
        markup += Glib::ustring::format("\n<span size='small'>", fname, "</span>");
    }
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
    _cell_renderer._tree = &_font_list;
    _font_list_store = Gtk::ListStore::create(g_column_model);

    set_hexpand();
    set_vexpand();
    pack_start(_main_grid, true, true);
    set_margin_start(0);
    set_margin_end(0);
    set_margin_top(5);
    set_margin_bottom(0);
    show_all();

    auto options = &get_widget<Gtk::ToggleButton>(_builder, "btn-options");
    // set_icon(options, "gears-symbolic");
    auto options_grid = &get_widget<Gtk::Grid>(_builder, "options-grid");
    options->signal_toggled().connect([=](){
        if (options->get_active()) options_grid->show(); else options_grid->hide();
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

    static const char* filter_boxes[] = {"id-monospaced", "id-oblique", "id-other"};

    for (auto&& id : filter_boxes) {
        auto& checkbtn = get_widget<Gtk::CheckButton>(_builder, id);
        checkbtn.signal_toggled().connect([=](){ filter(); });
    }
    get_widget<Gtk::Button>(_builder, "id-reset-filter").signal_clicked().connect([=](){
        for (auto&& id : filter_boxes) {
            auto& checkbtn = get_widget<Gtk::CheckButton>(_builder, id);
            checkbtn.set_active();
        }
        //TODO single filtering
    });

    auto search = &get_widget<Gtk::SearchEntry>(_builder, "font-search");
    search->signal_changed().connect([=](){ filter(); });

    auto size = &get_widget<Gtk::Scale>(_builder, "preview-font-size");
    size->set_value(s_fsize);
    size->signal_value_changed().connect([=](){
        s_fsize = size->get_value();
        // _font_list.queue_draw();
        // resize
        filter();
    });

    auto show_names = &get_widget<Gtk::CheckButton>(_builder, "show-font-name");
    show_names->signal_toggled().connect([=](){
        s_fname_show = show_names->get_active();
        // resize
        filter();
    });

    auto sample = &get_widget<Gtk::Entry>(_builder, "sample-text");
    sample->signal_changed().connect([=](){
        s_sample_text = sample->get_text();
        filter();
    });

    get_widget<Gtk::MenuItem>(_builder, "id-font-names").signal_activate().connect([=](){
        sample->set_text("");
        // s_sample_text.clear();
        filter();
    });

    std::pair<const char*, const char*> samples[5] = {
        {"id-alphanum", "AbcdEfgh1234"},
        {"id-digits", "1234567890"},
        {"id-lowercase", "abcdefghijklmnopqrstuvwxyz"},
        {"id-uppercase", "ABCDEFGHIJKLMNOPQRSTUVWXYZ"},
        {"id-fox", "The quick brown fox jumps over the lazy dog."}
    };
    for (auto&& el : samples) {
        auto& item = get_widget<Gtk::MenuItem>(_builder, el.first);
        item.signal_activate().connect([=](){
            sample->set_text(el.second);
            // s_sample_text = el.second;
            filter();
        });
    }

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

            double caps_height = 0.0;
            bool monospaced = false;
            bool oblique = false;
            try {
                auto font = FontFactory::get().Face(desc.gobj());
                monospaced = font->is_fixed_width();
                oblique = font->is_oblique();
                auto glyph = font->LoadGlyph(font->MapUnicodeChar('E'));
                if (glyph) {
                    // bbox: L T R B
                    caps_height = glyph->bbox[3] - glyph->bbox[1]; // caps height normalized to 0..1
                    auto& path = glyph->pathvector;
// g_message("ch: %f", caps_height);
// caps_height = font->get_E_height();
                    // auto& b = glyph->bbox;
        // g_message("box: %f %f %f %f", b[0], b[1], b[2], b[3]);
                }
            }
            catch (...) {
                g_warning("Error loading font %s", key.c_str());
            }
// g_message("> %s", desc.to_string().c_str());
            desc = get_font_description(ff, face);
            auto weight = calculate_font_weight(desc, caps_height);

            desc = get_font_description(ff, face);
            auto width = calculate_font_width(desc);

            FontInfo info { ff, face, weight, width, monospaced, oblique };
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

void FontList::CellFontRenderer::render_vfunc(const ::Cairo::RefPtr<::Cairo::Context>& cr, Widget& widget, const Gdk::Rectangle& background_area, const Gdk::Rectangle& cell_area, Gtk::CellRendererState flags) {

    CellRenderer::render_vfunc(cr, widget, background_area, cell_area, flags);

    // add separator if we display two lines per font
    if (s_fname_show) {
        auto context = _tree->get_style_context();
        Gtk::StateFlags sflags = _tree->get_state_flags();
        if (flags & Gtk::CELL_RENDERER_SELECTED) {
            sflags |= Gtk::STATE_FLAG_SELECTED;
        }
        Gdk::RGBA fg = context->get_color(sflags);
        cr->save();
        cr->set_source_rgba(fg.get_red(), fg.get_green(), fg.get_blue(), 0.12);

        auto y = background_area.get_y() + background_area.get_height() - 1;
        cr->move_to(background_area.get_x(), y);
        cr->line_to(background_area.get_x() + background_area.get_width(), y);
        cr->set_line_width(1);
        cr->stroke();
        cr->restore();
    }
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

    auto& sort = get_widget<Gtk::Image>(_builder, "sort-icon");
    if (icon) sort.set_from_icon_name(icon, Gtk::ICON_SIZE_BUTTON); //set_icon(sort, icon);

    filter();
}

void FontList::filter() {
    auto& search = get_widget<Gtk::SearchEntry>(_builder, "font-search");
    auto& oblique = get_widget<Gtk::CheckButton>(_builder, "id-oblique");
    auto& monospaced = get_widget<Gtk::CheckButton>(_builder, "id-monospaced");
    auto& others = get_widget<Gtk::CheckButton>(_builder, "id-other");
    Show params;
    params.monospaced = monospaced.get_active();
    params.oblique = oblique.get_active();
    params.others = others.get_active();
    filter(search.get_text(), params);
}

void FontList::filter(Glib::ustring text, const Show& params) {
    auto filter = text.lowercase();

    _font_list_store->freeze_notify();
    _font_list_store->clear();

    for (auto&& f : _fonts) {
        bool filter_in =
            params.oblique && f.oblique ||
            params.monospaced && f.monospaced ||
            params.others && (!f.oblique && !f.monospaced);

        if (!filter_in) continue;

        if (!text.empty()) {
            auto name = get_full_name(f);
            if (name.lowercase().find(filter) == Glib::ustring::npos) continue;
        }

        Gtk::TreeModel::iterator treeModelIter = _font_list_store->append();
        (*treeModelIter)[g_column_model.font] = &f;
    }

    _font_list_store->thaw_notify();

    auto& font_count = get_widget<Gtk::Label>(_builder, "font-count");
    auto count = _font_list_store->children().size();
    auto total = _fonts.size();
    font_count.set_text(count == total ? ("All fonts") : Glib::ustring::format(count, "/", total, " ", ("fonts")));

}

}}} // namespaces
