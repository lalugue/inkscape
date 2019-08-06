// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INK_EXTENSION_PARAMNOTEBOOK_H_SEEN
#define INK_EXTENSION_PARAMNOTEBOOK_H_SEEN

/** \file
 * Notebook parameter for extensions.
 */

/*
 * Author:
 *   Johan Engelen <johan@shouraizou.nl>
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 2006 Author
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "parameter.h"

#include <vector>

#include <glibmm/ustring.h>


namespace Gtk {
class Widget;
}


namespace Inkscape {
namespace Extension {

class Extension;


/** A class to represent a notebook parameter of an extension. */
class ParamNotebook : public InxParameter {
private:
    /** Internal value. */
    Glib::ustring _value;

    /**
     * A class to represent the pages of a notebook parameter of an extension.
     */
    class ParamNotebookPage : public InxParameter {
        friend class ParamNotebook;
    public:
        ParamNotebookPage(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext);
        ~ParamNotebookPage() override;

        Gtk::Widget *get_widget(SPDocument *doc, Inkscape::XML::Node *node, sigc::signal<void> *changeSignal) override;
        char *get_text() { return _text; };
        InxParameter *get_param(const char *name) override;
    }; /* class ParamNotebookPage */

    /** A table to store the pages with parameters for this notebook.
      * This only gets created if there are pages in this notebook */
    std::vector<ParamNotebookPage*> _pages;

public:
    ParamNotebook(Inkscape::XML::Node *xml, Inkscape::Extension::Extension *ext);
    ~ParamNotebook() override;

    Gtk::Widget *get_widget(SPDocument *doc, Inkscape::XML::Node *node, sigc::signal<void> *changeSignal) override;

    std::string value_to_string() const override;

    InxParameter *get_param (const char *name) override;

    const Glib::ustring& get (const SPDocument * /*doc*/, const Inkscape::XML::Node * /*node*/) { return _value; }
    const Glib::ustring& set (const int in, SPDocument *doc, Inkscape::XML::Node *node);
}; /* class ParamNotebook */





}  // namespace Extension
}  // namespace Inkscape

#endif /* INK_EXTENSION_PARAMNOTEBOOK_H_SEEN */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
