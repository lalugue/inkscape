// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Main UI stuff.
 */
/* Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Frank Felfe <innerspace@iname.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *   Kris De Gussem <Kris.DeGussem@gmail.com>
 *
 * Copyright (C) 2012 Kris De Gussem
 * Copyright (C) 2010 authors
 * Copyright (C) 1999-2005 authors
 * Copyright (C) 2004 David Turner
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glibmm/convert.h>
#include <glibmm/i18n.h>
#include <glibmm/miscutils.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/window.h>

#include "desktop.h"
#include "document.h"
#include "file.h"
#include "inkscape.h"
#include "inkscape-application.h"

#include "io/sys.h"

#include "ui/dialog-events.h"
#include "ui/dialog-run.h"
#include "ui/interface.h"

void sp_ui_new_view()
{
    auto app = InkscapeApplication::instance();

    auto doc = SP_ACTIVE_DOCUMENT;
    if (!doc) {
        return;
    }

    app->window_open(doc);
}

void sp_ui_close_view()
{
    auto app = InkscapeApplication::instance();

    auto window = app->get_active_window();
    assert(window);

    app->destroy_window(window, true); // Keep inkscape alive!
}

Glib::ustring getLayoutPrefPath(SPDesktop *desktop)
{
    if (desktop->is_focusMode()) {
        return "/focus/";
    } else if (desktop->is_fullscreen()) {
        return "/fullscreen/";
    } else {
        return "/window/";
    }
}

void sp_ui_import_files(char *buffer)
{
    auto const doc = SP_ACTIVE_DOCUMENT;
    if (!doc) {
        return;
    }

    char **l = g_uri_list_extract_uris(buffer);
    for (int i = 0; i < g_strv_length(l); i++) {
        char *filename = g_filename_from_uri(l[i], nullptr, nullptr);
        if (filename && std::strlen(filename) > 2) {
            // Pass off to common implementation
            // TODO might need to get the proper type of Inkscape::Extension::Extension
            file_import(doc, filename, nullptr);
        }
        g_free(filename);
    }
    g_strfreev(l);
}

void sp_ui_error_dialog(char const *message)
{
    auto const safeMsg = Inkscape::IO::sanitizeString(message);

    auto dlg = Gtk::MessageDialog(safeMsg, true, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_CLOSE);
    sp_transientize(dlg.Gtk::Widget::gobj());

    Inkscape::UI::dialog_run(dlg);
}

bool sp_ui_overwrite_file(std::string const &filename)
{
    // Fixme: Passing a std::string filename to an argument expecting utf8.
    if (!Inkscape::IO::file_test(filename.c_str(), G_FILE_TEST_EXISTS)) {
        return true;
    }

    auto const basename = Glib::filename_to_utf8(Glib::path_get_basename(filename));
    auto const dirname = Glib::filename_to_utf8(Glib::path_get_dirname(filename));
    auto const msg = Glib::ustring::compose(_("<span weight=\"bold\" size=\"larger\">A file named \"%1\" already exists. Do you want to replace it?</span>\n\n"
                                              "The file already exists in \"%2\". Replacing it will overwrite its contents."),
                                            basename, dirname);

    auto window = SP_ACTIVE_DESKTOP->getToplevel();
    auto dlg = Gtk::MessageDialog(*window, msg, true, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_NONE);
    dlg.add_button(_("_Cancel"), Gtk::RESPONSE_NO);
    dlg.add_button(_("Replace"), Gtk::RESPONSE_YES);
    dlg.set_default_response(Gtk::RESPONSE_YES);

    return Inkscape::UI::dialog_run(dlg) == Gtk::RESPONSE_YES;
}

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
