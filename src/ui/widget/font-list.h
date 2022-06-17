// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef INKSCAPE_UI_WIDGET_FONT_LIST_H
#define INKSCAPE_UI_WIDGET_FONT_LIST_H

#include <gtkmm/builder.h>
#include <gtkmm/grid.h>
#include <gtkmm/box.h>
#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>

namespace Inkscape {
namespace UI {
namespace Widget {

class FontList : public Gtk::Box {
public:
    FontList();
    
    struct FontInfo {
        Glib::RefPtr<Pango::FontFamily> ff;
        Glib::RefPtr<Pango::FontFace> face;
        // Glib::ustring css_name;     // Style as Pango/CSS would write it.
        // Glib::ustring display_name; // Style as Font designer named it.
        double weight;  // proxy for font weight - how black it is
        double width;   // proxy for font width - how compressed/extended it is
    };
private:
    enum class Sort { by_name, by_weight, by_width };
    void sort_fonts(Sort order);
    void filter();
    void filter(Glib::ustring text);

    Glib::RefPtr<Gtk::Builder> _builder;
    Gtk::Grid& _main_grid;
    Gtk::TreeView& _font_list;
    Gtk::TreeViewColumn _text_column;
    Gtk::CellRendererText _cell_renderer;
    Glib::RefPtr<Gtk::ListStore> _font_list_store;
    std::vector<FontInfo> _fonts;
    Sort _order = Sort::by_name;
    Glib::ustring _filter;
};

}}} // namespaces

#endif // INKSCAPE_UI_WIDGET_FONT_LIST_H