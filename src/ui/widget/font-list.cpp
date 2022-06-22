// SPDX-License-Identifier: GPL-2.0-or-later

#include <set>
#include <glibmm/markup.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/searchentry.h>
#include <gtkmm/scale.h>
#include <glibmm/i18n.h>
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

struct FontListColumnModel : public Gtk::TreeModelColumnRecord {
    Gtk::TreeModelColumn<Inkscape::FontInfo*> font;
    
    FontListColumnModel() {
        add(font);
    }
};

FontListColumnModel g_column_model;

Glib::ustring get_full_name(const Inkscape::FontInfo& font_info) {
    auto desc = font_info.face->describe();
    return /*font_info.ff->get_name() + ' ' +*/ desc.to_string(); // font_info.face->get_name();
}

static int s_fsize = 200;
static bool s_fname_show = true;
static Glib::ustring s_sample_text;

void get_cell_data_func(Gtk::CellRenderer* renderer, const Gtk::TreeIter& iter) {
    Gtk::TreeModel::Row row = *iter;
    Inkscape::FontInfo* font = row[g_column_model.font];

    auto name = get_full_name(*font);
    auto fname = Glib::Markup::escape_text(name);
    auto text = s_sample_text.empty() ? fname : s_sample_text;
    auto font_desc = Glib::Markup::escape_text(Inkscape::get_font_description(font->ff, font->face).to_string());
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

const char* get_sort_icon(Inkscape::FontOrder order) {
    const char* icon = nullptr;

    switch (order) {
        case Inkscape::FontOrder::by_name:
            icon = "sort-alphabetically-symbolic";
            break;

        case Inkscape::FontOrder::by_weight:
            icon = "sort-by-weight-symbolic";
            break;

        case Inkscape::FontOrder::by_width:
            icon = "sort-by-width-symbolic";
            break;

        default:
            g_warning("Missing case in get_sort_icon");
            break;
    }

    return icon;
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

    std::pair<const char*, Inkscape::FontOrder> sorting[3] = {
        {"sort-by-name", Inkscape::FontOrder::by_name},
        {"sort-by-weight", Inkscape::FontOrder::by_weight},
        {"sort-by-width", Inkscape::FontOrder::by_width}
    };
    for (auto&& el : sorting) {
        auto& item = get_widget<Gtk::MenuItem>(_builder, el.first);
        item.signal_activate().connect([=](){ sort_fonts(el.second); });
        // pack icon and text into MenuItem, since MenuImageItem is deprecated
        auto text = item.get_label();
        auto hbox = Gtk::manage(new Gtk::Box);
        Gtk::Image* img = sp_get_icon_image(get_sort_icon(el.second), Gtk::ICON_SIZE_BUTTON);
        hbox->pack_start(*img, false, true, 4);
        auto label = Gtk::manage(new Gtk::Label);
        label->set_label(text);
        hbox->pack_start(*label, false, true, 4);
        hbox->show_all();
        item.remove();
        item.add(*hbox);
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

g_message("get font start");
    _fonts = Inkscape::get_all_fonts();
g_message("get font done");

    sort_fonts(Inkscape::FontOrder::by_name);
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

void FontList::sort_fonts(Inkscape::FontOrder order) {
    Inkscape::sort_fonts(_fonts, order);

    if (const char* icon = get_sort_icon(order)) {
        auto& sort = get_widget<Gtk::Image>(_builder, "sort-icon");
        sort.set_from_icon_name(icon, Gtk::ICON_SIZE_BUTTON);
    }

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
    font_count.set_text(count == total ? _("All fonts") : Glib::ustring::format(count, ' ', _("of"), ' ', total, ' ', _("fonts")));

}

}}} // namespaces
