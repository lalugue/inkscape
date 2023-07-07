// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Page toolbar
 */
/* Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2021 Martin Owens
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_PAGE_TOOLBAR_H
#define SEEN_PAGE_TOOLBAR_H

#include <string>
#include <glibmm/refptr.h>
#include <gtkmm/toolbar.h>

#include "toolbar.h"

#include "ui/widget/spinbutton.h"
#include "helper/auto-connection.h"

namespace Gtk {
class ComboBoxText;
class Entry;
class EntryCompletion;
class Label;
class ListStore;
class Popover;
class Button;
} // namespace Gtk

class SPDesktop;
class SPDocument;
class SPPage;

namespace Inkscape {

class PaperSize;

namespace UI {

namespace Tools {
class ToolBase;
} // namespace Tools

namespace Toolbar {

class PageToolbar : public Toolbar
{
public:
    PageToolbar(SPDesktop *desktop);
    ~PageToolbar() final;

    static GtkWidget *create(SPDesktop *desktop);

protected:
    void labelEdited();
    void bleedsEdited();
    void marginsEdited();
    void marginTopEdited();
    void marginRightEdited();
    void marginBottomEdited();
    void marginLeftEdited();
    void marginSideEdited(int side, const Glib::ustring &value);
    void sizeChoose(const std::string &preset_key);
    void sizeChanged();
    void setSizeText(SPPage *page = nullptr, bool display_only = true);
    void setMarginText(SPPage *page = nullptr);

private:
    SPDocument *_document;

    void toolChanged(SPDesktop *desktop, Inkscape::UI::Tools::ToolBase *tool);
    void pagesChanged();
    void selectionChanged(SPPage *page);
    void on_parent_changed(Gtk::Widget *prev) final;
    void populate_sizes();

    Inkscape::auto_connection _ec_connection;
    Inkscape::auto_connection _doc_connection;
    Inkscape::auto_connection _pages_changed;
    Inkscape::auto_connection _page_selected;
    Inkscape::auto_connection _page_modified;
    Inkscape::auto_connection _label_edited;
    Inkscape::auto_connection _size_edited;

    bool was_referenced;
    Glib::RefPtr<Gtk::Builder> builder;
    Gtk::ComboBoxText *combo_page_sizes;
    Gtk::Entry *entry_page_sizes;
    Gtk::Entry *text_page_margins;
    Gtk::Entry *text_page_bleeds;
    Gtk::Entry *text_page_label;
    Gtk::Entry *text_page_width;
    Gtk::Entry *text_page_height;
    Gtk::Label *label_page_pos;
    Gtk::Button *btn_page_backward;
    Gtk::Button *btn_page_foreward;
    Gtk::Button *btn_page_delete;
    Gtk::Button *btn_move_toggle;
    Gtk::Separator *sep1;

    Glib::RefPtr<Gtk::ListStore> sizes_list;
    Glib::RefPtr<Gtk::ListStore> sizes_search;
    Glib::RefPtr<Gtk::EntryCompletion> sizes_searcher;

    Gtk::Popover *margin_popover;

    Inkscape::UI::Widget::MathSpinButton *margin_top;
    Inkscape::UI::Widget::MathSpinButton *margin_right;
    Inkscape::UI::Widget::MathSpinButton *margin_bottom;
    Inkscape::UI::Widget::MathSpinButton *margin_left;

    double _unit_to_size(std::string number, std::string unit_str, std::string const &backup);
};

} // namespace Toolbar
} // namespace UI
} // namespace Inkscape

#endif /* !SEEN_PAGE_TOOLBAR_H */

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
