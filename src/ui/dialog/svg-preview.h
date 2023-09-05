// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Implementation of the file dialog interfaces defined in filedialogimpl.h
 */
/* Authors:
 *   Bob Jamison
 *   Johan Engelen <johan@shouraizou.nl>
 *   Joel Holdsworth
 *   Bruno Dilly
 *   Other dudes from The Inkscape Organization
 *
 * Copyright (C) 2004-2008 Authors
 * Copyright (C) 2004-2007 The Inkscape Organization
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_SVG_PREVIEW_H
#define SEEN_SVG_PREVIEW_H

#include <cerrno>
#include <memory>
#include <unistd.h>
#include <sys/stat.h>

#include <gtkmm/box.h>

#include "filedialog.h"
#include "document.h"

namespace Glib {
class ustring;
} // namespace Glib

namespace Inkscape {

class URI;

namespace UI {

namespace View {
class SVGViewWidget;
} // namespace View

namespace Dialog {

/*#########################################################################
### SVG Preview Widget
#########################################################################*/

/**
 * Simple class for displaying an SVG file in the "preview widget."
 * Currently, this is just a wrapper of the sp_svg_view Gtk widget.
 * Hopefully we will eventually replace with a pure Gtkmm widget.
 */
class SVGPreview final : public Gtk::Box
{
public:
    SVGPreview();
    ~SVGPreview() final;

    bool setDocument(SPDocument *doc);
    bool setFileName(Glib::ustring const &fileName);
    bool setFromMem(char const *xmlBuffer);
    bool set(Glib::ustring const &fileName, int dialogType);
    bool setURI(URI &uri);

    /**
     * Show image embedded in SVG
     */
    void showImage(Glib::ustring const &fileName);

    /**
     * Show the "No preview" image
     */
    void showNoPreview();

    /**
     * Show the "Too large" image
     */
    void showTooLarge(long fileLength);

private:
    // Ensure correct destruction order: viewer before document.
    // So, declaring viewer after document means we ensure that.

    /**
     * The svg document we are currently showing
     */
    std::unique_ptr<SPDocument> document;

    /**
     * The sp_svg_view widget
     */
    std::unique_ptr<Inkscape::UI::View::SVGViewWidget> viewer;

    /**
     * are we currently showing the "no preview" image?
     */
    bool showingNoPreview;
};

} // namespace Dialog
} // namespace UI
} // namespace Inkscape

#endif // SEEN_SVG_PREVIEW_H

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
