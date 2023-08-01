// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief SVG Fonts dialog
 */
/* Authors:
 *   Felipe Corrêa da Silva Sanches <juca@members.fsf.org>
 *
 * Copyright (C) 2008 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_DIALOG_SVG_FONTS_H
#define INKSCAPE_UI_DIALOG_SVG_FONTS_H

#include <2geom/pathvector.h>
#include <gtkmm/box.h>
#include <gtkmm/grid.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/entry.h>
#include <gtkmm/iconview.h>
#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treeview.h>

#include "attributes.h"
#include "helper/auto-connection.h"
#include "ui/operation-blocker.h"
#include "ui/dialog/dialog-base.h"
#include "ui/widget/spinbutton.h"
#include "xml/helper-observer.h"

namespace Gtk {
class Scale;
} // namespace Gtk

class SPGlyph;
class SPGlyphKerning;
class SvgFont;

class SvgFontDrawingArea : public Gtk::DrawingArea {
public:
    SvgFontDrawingArea();
    void set_text(Glib::ustring);
    void set_svgfont(SvgFont*);
    void set_size(int x, int y);
    void redraw();

private:
    int _x,_y;
    SvgFont* _svgfont;
    Glib::ustring _text;
    bool on_draw(const Cairo::RefPtr<Cairo::Context> &cr) override;
};

class SPFont;

namespace Inkscape {
namespace UI {
namespace Dialog {

class GlyphComboBox : public Gtk::ComboBoxText {
public:
    GlyphComboBox();
    void update(SPFont*);
};

// cell text renderer for SVG font glyps (relying on Cairo "user font");
// it can accept mouse clicks and report them via signal_clicked()
class SvgGlyphRenderer : public Gtk::CellRenderer {
public:
    SvgGlyphRenderer() :
        Glib::ObjectBase(typeid(CellRenderer)),
        Gtk::CellRenderer(),
        _property_active(*this, "active", true),
        _property_activatable(*this, "activatable", true),
        _property_glyph(*this, "glyph", "") {

        property_mode() = Gtk::CELL_RENDERER_MODE_ACTIVATABLE;
    }

    ~SvgGlyphRenderer() override = default;

    Glib::PropertyProxy<Glib::ustring> property_glyph() { return _property_glyph.get_proxy(); }
    Glib::PropertyProxy<bool> property_active() { return _property_active.get_proxy(); }
    Glib::PropertyProxy<bool> property_activatable() { return _property_activatable.get_proxy(); }

    sigc::signal<void (const GdkEvent*, const Glib::ustring&)>& signal_clicked() {
        return _signal_clicked;
    }
 
    void set_svg_font(SvgFont* font) {
        _font = font;
    }

    void set_font_size(int size) {
        _font_size = size;
    }

    void set_tree(Gtk::Widget* tree) {
        _tree = tree;
    }

    void set_cell_size(int w, int h) {
        _width = w;
        _height = h;
    }

    int get_width() const {
        return _width;
    }

    void render_vfunc(const Cairo::RefPtr<Cairo::Context>& cr, Gtk::Widget& widget, const Gdk::Rectangle& background_area, const Gdk::Rectangle& cell_area, Gtk::CellRendererState flags) override;

    bool activate_vfunc(GdkEvent* event, Gtk::Widget& widget, const Glib::ustring& path, const Gdk::Rectangle& background_area, const Gdk::Rectangle& cell_area, Gtk::CellRendererState flags) override;

    void get_preferred_width_vfunc(Gtk::Widget& widget, int& min_w, int& nat_w) const override {
        min_w = nat_w = _width;
    }

    void get_preferred_height_vfunc(Gtk::Widget& widget, int& min_h, int& nat_h) const override {
        min_h = nat_h = _height;
    }

private:
    int _width = 0;
    int _height = 0;
    int _font_size = 0;
    Glib::Property<Glib::ustring> _property_glyph;
    Glib::Property<bool> _property_active;
    Glib::Property<bool> _property_activatable;
    SvgFont* _font = nullptr;
    Gtk::Widget* _tree = nullptr;
    sigc::signal<void (const GdkEvent*, const Glib::ustring&)> _signal_clicked;
};


class SvgFontsDialog : public DialogBase
{
public:
    SvgFontsDialog();
    ~SvgFontsDialog() override = default;

    void documentReplaced() override;

private:
    void update_fonts(bool document_replaced);
    SvgFont* get_selected_svgfont();
    SPFont* get_selected_spfont();
    SPGlyph* get_selected_glyph();
    SPGlyphKerning* get_selected_kerning_pair();

    void on_font_selection_changed();
    void on_kerning_pair_selection_changed();
    void on_preview_text_changed();
    void on_kerning_pair_changed();
    void on_kerning_value_changed();
    void on_setfontdata_changed();
    void add_font();

    // Used for font-family
    class AttrEntry
    {
    public:
        AttrEntry(SvgFontsDialog* d, gchar* lbl, Glib::ustring tooltip, const SPAttr attr);
        void set_text(const char*);
        Gtk::Entry* get_entry() { return &entry; }
        Gtk::Label* get_label() { return _label; }
    private:
        SvgFontsDialog* dialog;
        void on_attr_changed();
        Gtk::Entry entry;
        SPAttr attr;
        Gtk::Label* _label;
    };

    class AttrSpin
    {
    public:
        AttrSpin(SvgFontsDialog* d, gchar* lbl, Glib::ustring tooltip, const SPAttr attr);
        void set_value(double v);
        void set_range(double low, double high);
        Inkscape::UI::Widget::SpinButton* getSpin() { return &spin; }
        Gtk::Label* get_label() { return _label; }
    private:
        SvgFontsDialog* dialog;
        void on_attr_changed();
        Inkscape::UI::Widget::SpinButton spin;
        SPAttr attr;
        Gtk::Label* _label;
    };

    OperationBlocker _update;

    void update_glyphs(SPGlyph* changed_glyph = nullptr);
    void update_glyph(SPGlyph* glyph);
    void set_glyph_row(const Gtk::TreeRow& row, SPGlyph& glyph);
    void refresh_svgfont();
    void update_sensitiveness();
    void update_global_settings_tab();
    void populate_glyphs_box();
    void populate_kerning_pairs_box();
    void set_glyph_description_from_selected_path();
    void missing_glyph_description_from_selected_path();
    void reset_missing_glyph_description();
    void add_glyph();
    void glyph_unicode_edit(const Glib::ustring&, const Glib::ustring&);
    void glyph_name_edit(   const Glib::ustring&, const Glib::ustring&);
    void glyph_advance_edit(const Glib::ustring&, const Glib::ustring&);
    void remove_selected_glyph();
    void remove_selected_font();
    void remove_selected_kerning_pair();
    void font_selected(SvgFont* svgfont, SPFont* spfont);

    void add_kerning_pair();

    void create_glyphs_popup_menu(Gtk::Widget& parent, sigc::slot<void ()> rem);
    void glyphs_list_button_release(GdkEventButton* event);

    void create_fonts_popup_menu(Gtk::Widget& parent, sigc::slot<void ()> rem);
    void fonts_list_button_release(GdkEventButton* event);

    void create_kerning_pairs_popup_menu(Gtk::Widget& parent, sigc::slot<void ()> rem);
    void kerning_pairs_list_button_release(GdkEventButton* event);

    Gtk::TreeModel::iterator get_selected_glyph_iter();
    void set_selected_glyph(SPGlyph* glyph);
    void edit_glyph(SPGlyph* glyph);
    void sort_glyphs(SPFont* font);

    Inkscape::XML::SignalObserver _defs_observer; //in order to update fonts
    Inkscape::XML::SignalObserver _glyphs_observer;
    Inkscape::auto_connection _defs_observer_connection;

    Gtk::Box* AttrCombo(gchar* lbl, const SPAttr attr);
    Gtk::Box* global_settings_tab();

    // <font>
    Gtk::Label* _font_label;
    AttrSpin*  _horiz_adv_x_spin;
    AttrSpin*  _horiz_origin_x_spin;
    AttrSpin*  _horiz_origin_y_spin;

    // <font-face>
    Gtk::Label* _font_face_label;
    AttrEntry* _familyname_entry;
    AttrSpin*  _units_per_em_spin;
    AttrSpin*  _ascent_spin;
    AttrSpin*  _descent_spin;
    AttrSpin*  _cap_height_spin;
    AttrSpin*  _x_height_spin;

    Gtk::Box* kerning_tab();
    Gtk::Box* glyphs_tab();
    Gtk::Button _font_add;
    Gtk::Button _font_remove;

    class Columns : public Gtk::TreeModel::ColumnRecord
    {
    public:
        Columns() {
            add(spfont);
            add(svgfont);
            add(label);
        }

        Gtk::TreeModelColumn<SPFont*> spfont;
        Gtk::TreeModelColumn<SvgFont*> svgfont;
        Gtk::TreeModelColumn<Glib::ustring> label;
    };
    Glib::RefPtr<Gtk::ListStore> _model;
    Columns _columns;
    Gtk::TreeView _FontsList;
    Gtk::ScrolledWindow _fonts_scroller;

    /* Glyph Tab */
    class GlyphsColumns : public Gtk::TreeModel::ColumnRecord
    {
    public:
        GlyphsColumns() {
            add(glyph_node);
            add(glyph_name);
            add(unicode);
            add(UplusCode);
            add(advance);
            add(name_markup);
        }

        Gtk::TreeModelColumn<SPGlyph*> glyph_node;
        Gtk::TreeModelColumn<Glib::ustring> glyph_name;
        Gtk::TreeModelColumn<Glib::ustring> unicode;
        Gtk::TreeModelColumn<Glib::ustring> UplusCode;
        Gtk::TreeModelColumn<double> advance;
        Gtk::TreeModelColumn<Glib::ustring> name_markup;
    };
    enum GlyphColumnIndex { ColGlyph, ColName, ColString, ColUplusCode, ColAdvance };
    GlyphsColumns _GlyphsListColumns;
    Glib::RefPtr<Gtk::ListStore> _GlyphsListStore;
    Gtk::TreeView _GlyphsList;
    Gtk::ScrolledWindow _GlyphsListScroller;
    Gtk::ScrolledWindow _glyphs_icon_scroller;
    Gtk::IconView _glyphs_grid;
    SvgGlyphRenderer* _glyph_renderer = nullptr;
    SvgGlyphRenderer* _glyph_cell_renderer = nullptr;

    /* Kerning Tab */
    class KerningPairColumns : public Gtk::TreeModel::ColumnRecord
    {
    public:
        KerningPairColumns() {
            add(first_glyph);
            add(second_glyph);
            add(kerning_value);
            add(spnode);
        }

        Gtk::TreeModelColumn<Glib::ustring> first_glyph;
        Gtk::TreeModelColumn<Glib::ustring> second_glyph;
        Gtk::TreeModelColumn<double> kerning_value;
        Gtk::TreeModelColumn<SPGlyphKerning *> spnode;
    };

    KerningPairColumns _KerningPairsListColumns;
    Glib::RefPtr<Gtk::ListStore> _KerningPairsListStore;
    Gtk::TreeView _KerningPairsList;
    Gtk::ScrolledWindow _KerningPairsListScroller;
    Gtk::Button add_kernpair_button;

    Gtk::Grid _header_box;
    Gtk::Grid _grid;
    Gtk::Box global_vbox;
    Gtk::Box glyphs_vbox;
    Gtk::Box kerning_vbox;
    Gtk::Entry _preview_entry;
    bool _show_glyph_list = true;
    void set_glyphs_view_mode(bool list);

    Gtk::Menu _FontsContextMenu;
    Gtk::Menu _GlyphsContextMenu;
    Gtk::Menu _KerningPairsContextMenu;

    SvgFontDrawingArea _font_da, kerning_preview;
    GlyphComboBox first_glyph, second_glyph;
    SPGlyphKerning* kerning_pair;
    Inkscape::UI::Widget::SpinButton setwidth_spin;
    Gtk::Scale* kerning_slider;

    class EntryWidget : public Gtk::Box
    {
    public:
        EntryWidget()
        : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL) {
            this->add(this->_label);
            this->add(this->_entry);
        }
        void set_label(const gchar* l){
            this->_label.set_text(l);
        }
    private:
        Gtk::Label _label;
        Gtk::Entry _entry;
    };
    EntryWidget _font_family, _font_variant;
};

} // namespace Dialog
} // namespace UI
} // namespace Inkscape

#endif //#ifndef INKSCAPE_UI_DIALOG_SVG_FONTS_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
