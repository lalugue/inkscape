// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_PAINTBUCKET_TOOLBAR_H
#define SEEN_PAINTBUCKET_TOOLBAR_H

/**
 * @file
 * Paintbucket aux toolbar
 */
/* Authors:
 *   MenTaLguY <mental@rydia.net>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Frank Felfe <innerspace@iname.com>
 *   John Cliff <simarilius@yahoo.com>
 *   David Turner <novalis@gnu.org>
 *   Josh Andler <scislac@scislac.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Maximilian Albert <maximilian.albert@gmail.com>
 *   Tavmjong Bah <tavmjong@free.fr>
 *   Abhishek Sharma
 *   Kris De Gussem <Kris.DeGussem@gmail.com>
 *
 * Copyright (C) 2004 David Turner
 * Copyright (C) 2003 MenTaLguY
 * Copyright (C) 1999-2011 authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <memory>
#include <glibmm/refptr.h>
#include <gtk/gtk.h>

#include "toolbar.h"

namespace Gtk {
class Adjustment;
} // namespace Gtk

class SPDesktop;

namespace Inkscape::UI {

namespace Widget {
class ComboToolItem;
class UnitTracker;
} // namespace Widget

namespace Toolbar {

class PaintbucketToolbar final : public Toolbar {
private:
    UI::Widget::ComboToolItem *_channels_item;
    UI::Widget::ComboToolItem *_autogap_item;

    Glib::RefPtr<Gtk::Adjustment> _threshold_adj;
    Glib::RefPtr<Gtk::Adjustment> _offset_adj;

    std::unique_ptr<UI::Widget::UnitTracker> _tracker;

    void channels_changed(int channels);
    void threshold_changed();
    void offset_changed();
    void autogap_changed(int autogap);
    void defaults();

protected:
    PaintbucketToolbar(SPDesktop *desktop);

public:
    static GtkWidget * create(SPDesktop *desktop);
};

} // namespace Toolbar

} // namespace Inkscape::UI

#endif /* !SEEN_PAINTBUCKET_TOOLBAR_H */

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
