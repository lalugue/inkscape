// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef INKSCAPE_UI_WIDGET_FONT_LIST_H
#define INKSCAPE_UI_WIDGET_FONT_LIST_H

#include <gtkmm/builder.h>
#include <gtkmm/grid.h>
#include <gtkmm/box.h>
#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>
#include "util/font-discovery.h"

namespace Inkscape {
namespace UI {
namespace Widget {

class FontList : public Gtk::Box {
public:
    FontList();
    
    // implementation details ------------

    // struct FontInfo {
    //     Glib::RefPtr<Pango::FontFamily> ff;
    //     Glib::RefPtr<Pango::FontFace> face;
    //     double weight;  // proxy for font weight - how black it is
    //     double width;   // proxy for font width - how compressed/extended it is
    //     bool monospaced;
    //     bool oblique;
    // };
    // enum class Sort { by_name, by_weight, by_width };
private:
    void sort_fonts(Inkscape::FontOrder order);
    void filter();
    struct Show {
        bool monospaced;
        bool oblique;
        bool others;
    };
    void filter(Glib::ustring text, const Show& params);

    Glib::RefPtr<Gtk::Builder> _builder;
    Gtk::Grid& _main_grid;
    Gtk::TreeView& _font_list;
    Gtk::TreeViewColumn _text_column;
    Glib::RefPtr<Gtk::ListStore> _font_list_store;
    std::vector<FontInfo> _fonts;
    Inkscape::FontOrder _order = Inkscape::FontOrder::by_name;
    Glib::ustring _filter;

    class CellFontRenderer : public Gtk::CellRendererText {
    public:
        Gtk::Widget* _tree = nullptr;
        void render_vfunc(const ::Cairo::RefPtr< ::Cairo::Context>& cr, Widget& widget, const Gdk::Rectangle& background_area, const Gdk::Rectangle& cell_area, Gtk::CellRendererState flags) override;
    } _cell_renderer;
};

}}} // namespaces

#endif // INKSCAPE_UI_WIDGET_FONT_LIST_H