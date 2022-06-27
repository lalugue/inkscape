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
#include "iconrenderer.h"
#include "ui/builder-utils.h"
#include "ui/icon-loader.h"
#include "svg/css-ostringstream.h"

using Inkscape::UI::create_builder;

namespace Inkscape {
namespace UI {
namespace Widget {

struct FontListColumnModel : public Gtk::TreeModelColumnRecord {
    Gtk::TreeModelColumn<Inkscape::FontInfo> font;
    Gtk::TreeModelColumn<bool> missing;
    Gtk::TreeModelColumn<Glib::ustring> missing_name;
    Gtk::TreeModelColumn<int> icon;
    
    FontListColumnModel() {
        add(missing_name);
        add(missing);
        add(icon);
        add(font);
    }
};

FontListColumnModel g_column_model;

Glib::ustring get_full_name(const Inkscape::FontInfo& font_info) {
    if (!font_info.face) return "";

    auto desc = font_info.face->describe();

// g_message("full: '%s' '%s' - '%s', s: '%s'", font_info.ff->get_name().c_str(), font_info.face->get_name().c_str(), desc.to_string().c_str(), style.c_str());

    auto name = desc.to_string();
    // TODO: sanitize name, remove some stray characters; delegate this to common fn
    auto& raw = name.raw();
    if (raw.size() > 0 && raw[raw.size() - 1] == ',') {
        auto copy = name.raw();
        copy.pop_back();
        name = copy;
    }

    return name;
}

class CellFontRenderer : public Gtk::CellRendererText {
public:
    Gtk::Widget* _tree = nullptr;
    Inkscape::FontInfo _font;
    bool _show_font_name = true;
    int _font_size = 200; // size in %, where 100 is normal UI font size
    Glib::ustring _sample_text; // text to render (font preview)
    bool _missing = false;

    void render_vfunc(const ::Cairo::RefPtr< ::Cairo::Context>& cr, Gtk::Widget& widget, const Gdk::Rectangle& background_area, const Gdk::Rectangle& cell_area, Gtk::CellRendererState flags) override;
};

CellFontRenderer& get_renderer(Gtk::CellRenderer& renderer) {
    return dynamic_cast<CellFontRenderer&>(renderer);
}

void get_cell_data_func(Gtk::CellRenderer* cell_renderer, const Gtk::TreeIter& iter) {
    auto& renderer = get_renderer(*cell_renderer);

    Gtk::TreeModel::Row row = *iter;
    const Inkscape::FontInfo& font = row[g_column_model.font];
    bool missing = row[g_column_model.missing];
    Glib::ustring&& missing_name = row[g_column_model.missing_name];
// if (missing){
    // g_message("missing font: %s", missing_name.c_str());
// }
    auto name = missing ? missing_name : get_full_name(font);
    auto fname = Glib::Markup::escape_text(name);
    // if no sample text given, then render font name
    auto text = renderer._sample_text.empty() ? fname : renderer._sample_text;
// if (font.face) {
//     text += ':' + std::to_string(font.weight); // font.face->describe().get_weight());
// }
    auto font_desc = Glib::Markup::escape_text(
        missing ? "Sans" : Inkscape::get_font_description(font.ff, font.face).to_string());
    auto markup = Glib::ustring::format("<span size='", renderer._font_size, "%' font='", font_desc, "'>", text, "</span>");
    if (renderer._show_font_name) {
        markup += Glib::ustring::format("\n<span size='small' font='Noto Sans'>", fname, "</span>");
    }
    renderer.set_property("markup", markup);
    renderer._font = font;
    renderer._missing = missing;
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
    _font_list(get_widget<Gtk::TreeView>(_builder, "font-list")),
    _font_size(get_widget<Gtk::ComboBoxText>(_builder, "font-size")),
    _font_size_scale(get_widget<Gtk::Scale>(_builder, "font-size-scale"))
{
    _cell_renderer = std::make_unique<CellFontRenderer>();
    static_cast<CellFontRenderer*>(_cell_renderer.get())->_tree = &_font_list;

    _cell_icon_renderer = std::make_unique<IconRenderer>();
    auto ico = static_cast<IconRenderer*>(_cell_icon_renderer.get());
    ico->add_icon("empty-icon-symbolic");
    ico->add_icon("missing-element-symbolic");

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
    size->set_value(get_renderer(*_cell_renderer)._font_size);
    size->signal_value_changed().connect([=](){
        get_renderer(*_cell_renderer)._font_size = size->get_value();
        // _font_list.queue_draw();
        // resize
        filter();
    });

    auto show_names = &get_widget<Gtk::CheckButton>(_builder, "show-font-name");
    show_names->signal_toggled().connect([=](){
        get_renderer(*_cell_renderer)._show_font_name = show_names->get_active();
        // resize
        filter();
    });

    auto sample = &get_widget<Gtk::Entry>(_builder, "sample-text");
    sample->signal_changed().connect([=](){
        get_renderer(*_cell_renderer)._sample_text = sample->get_text();
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

    // _text_column.pack_start(*_cell_icon_renderer, false);
    _text_column.pack_start(*_cell_renderer, true);
    _text_column.set_fixed_width(100); // limit minimal width to keep entire dialog narrow; column can still grow
    // _text_column.add_attribute(*_cell_renderer, "text", 0);
    _text_column.set_cell_data_func(*_cell_renderer, &get_cell_data_func);
    _text_column.set_expand();
    // _text_column.add_attribute(ico->property_icon(), g_column_model.icon);
    _font_list.append_column(_text_column);

    // _icon_column.pack_start(*_cell_icon_renderer, false);
    // _icon_column.set_fixed_width(16);
    // _icon_column.add_attribute(*_cell_icon_renderer, "icon", 0);
    // _icon_column.add_attribute(ico->property_icon(), g_column_model.icon);
    // _font_list.append_column(_icon_column); //"ico", *_cell_icon_renderer);

    _font_list.set_search_column(-1); // disable, we have a separate search/filter
    _font_list.set_enable_search(false);
    _font_list.set_model(_font_list_store);

g_message("get font start");
    _fonts = Inkscape::get_all_fonts();
g_message("get font done");

    const auto step = std::pow(2.0, 1.0 / 3.0);
    // formula for slow exponential growth for font size slider: cube root of 2 to the power N;
    // use only whole numbers (ints); start from '4' (index+6), smaller sizes are not very useful;
    // exponential progression is used to cover range of more interesting font sizes
    auto size_growth = [=](double index) { return static_cast<int>(std::round(std::pow(step, index + 6))); };
    // inverse of the above exponential formula
    auto inverse = [=](double size) { return std::log(size) / std::log(step) - 6; };

    _font_size_scale.signal_value_changed().connect([=](){
        if (_update.pending()) return;

        auto scoped = _update.block();
        auto size = size_growth(_font_size_scale.get_value());
        _font_size.get_entry()->set_text(std::to_string(size));
        _signal_changed.emit();
    });

    _font_size.signal_changed().connect([=](){
        if (_update.pending()) return;

        auto scoped = _update.block();
        auto text = _font_size.get_active_text();
        if (!text.empty()) {
            auto size = ::atof(text.c_str());
            if (size > 0) {
                _font_size_scale.set_value(inverse(size));
                _signal_changed.emit();
            }
        }
    });
    _font_size.set_active_text("10");

    sort_fonts(Inkscape::FontOrder::by_name);

    _font_list.get_selection()->signal_changed().connect([=](){
        if (!_update.pending()) {
            auto scoped = _update.block();
            _signal_changed.emit();
        }
    });

    // double-click to apply:
    _font_list.signal_row_activated().connect([=](const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn*){
        if (!_update.pending()) {
            auto scoped = _update.block();
            _signal_apply.emit();
        }
    });

}

void CellFontRenderer::render_vfunc(const ::Cairo::RefPtr<::Cairo::Context>& cr, Gtk::Widget& widget, const Gdk::Rectangle& background_area, const Gdk::Rectangle& cell_area, Gtk::CellRendererState flags) {

    Gdk::Rectangle bgnd(background_area);
    Gdk::Rectangle area(cell_area);
    auto margin = _missing ? 20 : 4; // space for icon
    bgnd.set_x(bgnd.get_x() + margin);
    area.set_x(area.get_x() + margin);

    CellRendererText::render_vfunc(cr, widget, bgnd, area, flags);

    if (_missing) {
        // missing font: draw icon?
    }

    // add separator if we display two lines per font
    if (_show_font_name) {
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

bool FontList::select_font(const Glib::ustring& fontspec) {
    bool found = false;

    _font_list_store->foreach([&](const Gtk::TreeModel::Path& path, const Gtk::TreeModel::iterator& iter){
        const auto& row = *iter;
        const auto& font = row.get_value(g_column_model.font);
        auto missing = row.get_value(g_column_model.missing);
        if (missing) {
            auto spec = row.get_value(g_column_model.missing_name);
            if (spec == fontspec) {
                _font_list.get_selection()->select(row);
                _font_list.scroll_to_row(path);
                found = true;
                return true; // stop
            }
        }
        else {
            auto spec = Inkscape::get_inkscape_fontspec(font.ff, font.face);
            if (spec == fontspec) {
                _font_list.get_selection()->select(row);
                _font_list.scroll_to_row(path);
                found = true;
                return true; // stop
            }
        }
        return false; // continue
    });

    return found;
}

void FontList::filter() {
    // todo: save selection
    Inkscape::FontInfo selected;
    auto it = _font_list.get_selection()->get_selected();
    if (it) {
        selected = it->get_value(g_column_model.font);
    }

    auto& search = get_widget<Gtk::SearchEntry>(_builder, "font-search");
    auto& oblique = get_widget<Gtk::CheckButton>(_builder, "id-oblique");
    auto& monospaced = get_widget<Gtk::CheckButton>(_builder, "id-monospaced");
    auto& others = get_widget<Gtk::CheckButton>(_builder, "id-other");
    Show params;
    params.monospaced = monospaced.get_active();
    params.oblique = oblique.get_active();
    params.others = others.get_active();
    filter(search.get_text(), params);

    if (!_current_fspec.empty()) {
        add_font(_current_fspec, false);
    }

    if (it) {
        // reselect
    }
}

void FontList::filter(Glib::ustring text, const Show& params) {
    auto filter = text.lowercase();

    _font_list_store->freeze_notify();
    _font_list_store->clear();
    _extra_fonts = 0;
    // _fspec_to_row.clear();

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
        auto& row = *treeModelIter;
        row[g_column_model.font] = f;
        row[g_column_model.missing] = false;
        row[g_column_model.missing_name] = Glib::ustring();
        row[g_column_model.icon] = 0;
    }

    // for (auto&& row : _font_list_store->children()) {
    //     const auto& font = row.get_value(g_column_model.font);
    //     auto key = Inkscape::get_fontspec(font.ff, font.face).raw();
    //     _fspec_to_row[key] = &row;
    // }

    _font_list_store->thaw_notify();

    update_font_count();
}

void FontList::update_font_count() {
    auto& font_count = get_widget<Gtk::Label>(_builder, "font-count");
    auto count = _font_list_store->children().size();
    auto total = _fonts.size();
    // count could be larger than total if we insert "missing" font(s)
    auto label = count >= total ? _("All fonts") : Glib::ustring::format(count, ' ', _("of"), ' ', total, ' ', _("fonts"));
    // if (_extra_fonts > 0) {
    //     // indicate that extra rows were inserted to avoid surprises of incorrect count
    //     label += Glib::ustring::format(" +", _extra_fonts);
    // }
    font_count.set_text(label);
}

double FontList::get_fontsize() const {
    auto text = _font_size.get_entry()->get_text();
    if (!text.empty()) {
        auto size = ::atof(text.c_str());
        if (size > 0) return size;
    }
    return _current_fsize;
}

Glib::ustring FontList::get_fontspec() const {
    if (auto iter = _font_list.get_selection()->get_selected()) {
        auto missing = iter->get_value(g_column_model.missing);
        if (missing) {
            return iter->get_value(g_column_model.missing_name);
        }
        const auto& font = iter->get_value(g_column_model.font);
        auto fspec = Inkscape::get_inkscape_fontspec(font.ff, font.face);
// g_message("get fspec: %s", fspec.c_str());
        return fspec;
    }

    return "Sans"; // no selection
}

void FontList::set_current_font(const Glib::ustring& family, const Glib::ustring& face) {
    if (_update.pending()) return;
// g_message("setcur: %s - %s", family.c_str(), face.c_str());

    auto fontspec = Inkscape::get_fontspec(family, face);
    if (fontspec == _current_fspec) {
        select_font(fontspec);
        return;
    }

    _current_fspec = fontspec;

    if (!fontspec.empty()) {
        add_font(fontspec, true);
    }
}

void FontList::set_current_size(double size) {
    _current_fsize = size;
    if (_update.pending()) return;
    //todo
// g_message("set fsize: %f", size);

    CSSOStringStream os;
    os.precision(3);os << size;
    _font_size.get_entry()->set_text(os.str());
}

void FontList::add_font(const Glib::ustring& fontspec, bool select) {
// g_message("addfnt: %s", fontspec.c_str());
    // auto it = std::find_if(begin(_fonts), end(_fonts), [&](const FontInfo& f){
    //     return Inkscape::get_fontspec(f.ff, f.face) == fontspec;
    // });
    // if (it != end(_fonts)) {
// g_message("addfnt2: %s", fontspec.c_str());
        // found matching font; check if it is currently in the tree list

    bool found = select_font(fontspec); // found in the tree view?
    if (found) return;

    auto it = std::find_if(begin(_fonts), end(_fonts), [&](const FontInfo& f){
        return Inkscape::get_inkscape_fontspec(f.ff, f.face) == fontspec;
    });

    if (it != end(_fonts)) {
        // font found in the "all fonts" vector, but
// g_message("addfnt4: %s", fontspec.c_str());
        // this font is filtered out; add it temporarily to the tree list
        Gtk::TreeModel::iterator treeModelIter = _font_list_store->prepend();
        auto& top_row = *treeModelIter;
        top_row[g_column_model.font] = *it;
        top_row[g_column_model.missing] = false;
        top_row[g_column_model.missing_name] = Glib::ustring();
        top_row[g_column_model.icon] = 0;
        // _fspec_to_row[fontspec.raw()] = &top_row;

        if (select) {
            _font_list.get_selection()->select(top_row);
            auto path = _font_list_store->get_path(treeModelIter);
            _font_list.scroll_to_row(path);
        }

        ++_extra_fonts; // extra font entry inserted
    }
    else {
// g_message("addfnt5: %s", fontspec.c_str());
        // font not found; insert a placeholder to show missing font: font is used in a document,
        // but not available in the system
        Inkscape::FontInfo missing;
        Gtk::TreeModel::iterator treeModelIter = _font_list_store->prepend();
        auto& top_row = *treeModelIter;
        top_row[g_column_model.font] = missing;
        top_row[g_column_model.missing] = true;
        top_row[g_column_model.missing_name] = fontspec;// TODO fname
        top_row[g_column_model.icon] = 1;
        // _fspec_to_row[fontspec.raw()] = &top_row;

        if (select) {
            _font_list.get_selection()->select(top_row);
            auto path = _font_list_store->get_path(treeModelIter);
            _font_list.scroll_to_row(path);
        }

        ++_extra_fonts; // extra font entry for a missing font added
    }
// g_message("addfnt6: %s", fontspec.c_str());
    update_font_count();
}

}}} // namespaces
