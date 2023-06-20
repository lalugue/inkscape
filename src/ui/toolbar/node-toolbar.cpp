// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Node aux toolbar
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

#include "node-toolbar.h"

#include <glibmm/i18n.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/image.h>
#include <gtkmm/menutoolbutton.h>
#include <gtkmm/separatortoolitem.h>

#include "desktop.h"
#include "document-undo.h"
#include "inkscape.h"
#include "object/sp-lpe-item.h"
#include "object/sp-namedview.h"
#include "page-manager.h"
#include "selection-chemistry.h"
#include "selection.h"
#include "ui/builder-utils.h"
#include "ui/icon-names.h"
#include "ui/simple-pref-pusher.h"
#include "ui/tool/control-point-selection.h"
#include "ui/tool/multi-path-manipulator.h"
#include "ui/tools/node-tool.h"
#include "ui/widget/canvas.h"
#include "ui/widget/combo-tool-item.h"
#include "ui/widget/spinbutton.h"
#include "ui/widget/unit-tracker.h"
#include "widgets/widget-sizes.h"

using Inkscape::UI::Widget::UnitTracker;
using Inkscape::Util::Unit;
using Inkscape::Util::Quantity;
using Inkscape::DocumentUndo;
using Inkscape::Util::unit_table;
using Inkscape::UI::Tools::NodeTool;

/** Temporary hack: Returns the node tool in the active desktop.
 * Will go away during tool refactoring. */
static NodeTool *get_node_tool()
{
    NodeTool *node_tool = nullptr;
    if (SP_ACTIVE_DESKTOP ) {
        if (auto const tool = SP_ACTIVE_DESKTOP->getTool(); INK_IS_NODE_TOOL(tool)) {
            node_tool = static_cast<NodeTool *>(tool);
        }
    }
    return node_tool;
}

namespace Inkscape::UI::Toolbar {

NodeToolbar::NodeToolbar(SPDesktop *desktop)
    : Toolbar(desktop)
    , _tracker{std::make_unique<UnitTracker>(Inkscape::Util::UNIT_TYPE_LINEAR)}
    ,
    , _freeze(false)
    , _builder(initialize_builder("toolbar-node.ui"))
{
    Unit doc_units = *desktop->getNamedView()->display_units;
    _tracker->setActiveUnit(&doc_units);

    _builder->get_widget("node-toolbar", _toolbar);
    if (!_toolbar) {
        std::cerr << "InkscapeWindow: Failed to load node toolbar!" << std::endl;
    }

    // TODO: Remove if not needed.
    Gtk::MenuButton *insert_node_item;
    Gtk::Button *delete_item;

    Gtk::Button *join_item;
    Gtk::Button *break_item;

    Gtk::Button *join_segment_item;
    Gtk::Button *delete_segment_item;

    Gtk::Button *cusp_item;
    Gtk::Button *smooth_item;
    Gtk::Button *symmetric_item;
    Gtk::Button *auto_item;

    Gtk::Button *line_item;
    Gtk::Button *curve_item;

    Gtk::Button *lpe_corners_item;

    Gtk::Button *object_to_path_item;
    Gtk::Button *stroke_to_path_item;

    Gtk::Box *_nodes_x_box;
    Gtk::Box *_nodes_y_box;
    Gtk::Box *unit_menu_box;

    // TODO: Implement insert_node_item.
    _builder->get_widget("insert_node_item", insert_node_item);
    _builder->get_widget("delete_item", delete_item);

    _builder->get_widget("join_item", join_item);
    _builder->get_widget("break_item", break_item);

    _builder->get_widget("join_segment_item", join_segment_item);
    _builder->get_widget("delete_segment_item", delete_segment_item);

    _builder->get_widget("cusp_item", cusp_item);
    _builder->get_widget("smooth_item", smooth_item);
    _builder->get_widget("symmetric_item", symmetric_item);
    _builder->get_widget("auto_item", auto_item);

    _builder->get_widget("line_item", line_item);
    _builder->get_widget("curve_item", curve_item);

    _builder->get_widget("lpe_corners_item", lpe_corners_item);

    _builder->get_widget("object_to_path_item", object_to_path_item);
    _builder->get_widget("stroke_to_path_item", stroke_to_path_item);

    _builder->get_widget("_nodes_x_box", _nodes_x_box);
    _builder->get_widget_derived("_nodes_x_item", _nodes_x_item);
    _builder->get_widget("_nodes_y_box", _nodes_y_box);
    _builder->get_widget_derived("_nodes_y_item", _nodes_y_item);
    _builder->get_widget("unit_menu_box", unit_menu_box);

    _builder->get_widget("_nodes_lpeedit_item", _nodes_lpeedit_item);

    _builder->get_widget("_show_helper_path_item", _show_helper_path_item);
    _builder->get_widget("_show_handles_item", _show_handles_item);
    _builder->get_widget("_show_transform_handles_item", _show_transform_handles_item);
    _builder->get_widget("_object_edit_mask_path_item", _object_edit_mask_path_item);
    _builder->get_widget("_object_edit_clip_path_item", _object_edit_clip_path_item);

    // Setup the derived spin buttons.
    setup_derived_spin_button(_nodes_x_item, "Xcoord");
    setup_derived_spin_button(_nodes_y_item, "Ycoord");

    auto unit_menu = _tracker->create_tool_item(_("Units"), (""));
    unit_menu_box->add(*unit_menu);

    // Fetch all the ToolbarMenuButtons at once from the UI file
    // Menu Button #1
    Gtk::Box *popover_box1;
    _builder->get_widget("popover_box1", popover_box1);

    Inkscape::UI::Widget::ToolbarMenuButton *menu_btn1 = nullptr;
    _builder->get_widget_derived("menu_btn1", menu_btn1);

    // Menu Button #2
    Gtk::Box *popover_box2;
    _builder->get_widget("popover_box2", popover_box2);

    Inkscape::UI::Widget::ToolbarMenuButton *menu_btn2 = nullptr;
    _builder->get_widget_derived("menu_btn2", menu_btn2);

    // Initialize all the ToolbarMenuButtons.
    // Note: Do not initialize the these widgets right after fetching from
    // the UI file.
    auto children = _toolbar->get_children();

    menu_btn1->init(1, "tag1", "some-icon", popover_box1, children);
    _expanded_menu_btns.push(menu_btn1);
    menu_btn2->init(2, "tag2", "some-icon", popover_box2, children);
    _expanded_menu_btns.push(menu_btn2);

    add(*_toolbar);

    // Attach the signals.
    insert_node_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_add));
    delete_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_delete));

    join_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_join));
    break_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_break));

    join_segment_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_join_segment));
    delete_segment_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_delete_segment));

    cusp_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_cusp));
    smooth_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_smooth));
    symmetric_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_symmetrical));
    auto_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_auto));

    line_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_toline));
    curve_item->signal_clicked().connect(sigc::mem_fun(*this, &NodeToolbar::edit_tocurve));

    gtk_actionable_set_action_name(GTK_ACTIONABLE(lpe_corners_item->gobj()), "app.object-add-corners-lpe");

    gtk_actionable_set_action_name(GTK_ACTIONABLE(object_to_path_item->gobj()), "app.object-to-path");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(stroke_to_path_item->gobj()), "app.object-stroke-to-path");

    _pusher_show_outline.reset(new UI::SimplePrefPusher(_show_helper_path_item, "/tools/nodes/show_outline"));
    _show_helper_path_item->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &NodeToolbar::on_pref_toggled),
                                                                _show_helper_path_item, "/tools/nodes/show_outline"));

    _pusher_show_handles.reset(new UI::SimplePrefPusher(_show_handles_item, "/tools/nodes/show_handles"));
    _show_handles_item->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &NodeToolbar::on_pref_toggled),
                                                            _show_handles_item, "/tools/nodes/show_handles"));

    _pusher_show_transform_handles.reset(
        new UI::SimplePrefPusher(_show_transform_handles_item, "/tools/nodes/show_transform_handles"));
    _show_transform_handles_item->signal_toggled().connect(
        sigc::bind(sigc::mem_fun(*this, &NodeToolbar::on_pref_toggled), _show_transform_handles_item,
                   "/tools/nodes/show_transform_handles"));

    _pusher_edit_masks.reset(new SimplePrefPusher(_object_edit_mask_path_item, "/tools/nodes/edit_masks"));
    _object_edit_mask_path_item->signal_toggled().connect(sigc::bind(
        sigc::mem_fun(*this, &NodeToolbar::on_pref_toggled), _object_edit_mask_path_item, "/tools/nodes/edit_masks"));

    _pusher_edit_clipping_paths.reset(
        new SimplePrefPusher(_object_edit_clip_path_item, "/tools/nodes/edit_clipping_paths"));
    _object_edit_clip_path_item->signal_toggled().connect(
        sigc::bind(sigc::mem_fun(*this, &NodeToolbar::on_pref_toggled), _object_edit_clip_path_item,
                   "/tools/nodes/edit_clipping_paths"));

    sel_changed(desktop->getSelection());
    desktop->connectEventContextChanged(sigc::mem_fun(*this, &NodeToolbar::watch_ec));

    show_all();

    // TODO: Fix this.
    menu_btn1->set_visible(false);
    menu_btn2->set_visible(false);
}

void NodeToolbar::setup_derived_spin_button(Inkscape::UI::Widget::SpinButton *btn, const Glib::ustring &name)
{
    std::vector<double> values = {1, 2, 3, 5, 10, 20, 50, 100, 200, 500};
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    const Glib::ustring path = "/tools/nodes/" + name;
    auto val = prefs->getDouble(path, 0);
    auto adj = btn->get_adjustment();
    adj->set_value(val);
    adj->signal_value_changed().connect(sigc::bind(sigc::mem_fun(*this, &NodeToolbar::value_changed), Geom::X));

    // TODO: Fix this.
    // btn->set_custom_numeric_menu_data(values);
    _tracker->addAdjustment(adj->gobj());
    btn->addUnitTracker(_tracker.get());
    btn->set_defocus_widget(_desktop->canvas);

    // TODO: Handle this in the toolbar class.
    btn->set_sensitive(false);
}

GtkWidget *NodeToolbar::create(SPDesktop *desktop)
{
    auto holder = new NodeToolbar(desktop);
    return holder->Gtk::Widget::gobj();
} // NodeToolbar::prep()

void NodeToolbar::value_changed(Geom::Dim2 d)
{
    auto adj = (d == Geom::X) ? _nodes_x_item->get_adjustment() : _nodes_y_item->get_adjustment();

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    if (!_tracker) {
        return;
    }

    Unit const *unit = _tracker->getActiveUnit();

    if (DocumentUndo::getUndoSensitive(_desktop->getDocument())) {
        prefs->setDouble(Glib::ustring("/tools/nodes/") + (d == Geom::X ? "x" : "y"),
            Quantity::convert(adj->get_value(), unit, "px"));
    }

    // quit if run by the attr_changed listener
    if (_freeze || _tracker->isUpdating()) {
        return;
    }

    // in turn, prevent listener from responding
    _freeze = true;

    NodeTool *nt = get_node_tool();
    if (nt && !nt->_selected_nodes->empty()) {
        double val = Quantity::convert(adj->get_value(), unit, "px");
        double oldval = nt->_selected_nodes->pointwiseBounds()->midpoint()[d];

        // Adjust the coordinate to the current page, if needed
        auto &pm = _desktop->getDocument()->getPageManager();
        if (prefs->getBool("/options/origincorrection/page", true)) {
            auto page = pm.getSelectedPageRect();
            oldval -= page.corner(0)[d];
        }

        Geom::Point delta(0,0);
        delta[d] = val - oldval;
        nt->_multipath->move(delta);
    }

    _freeze = false;
}

void NodeToolbar::sel_changed(Inkscape::Selection *selection)
{
    SPItem *item = selection->singleItem();
    if (item && is<SPLPEItem>(item)) {
        if (cast_unsafe<SPLPEItem>(item)->hasPathEffect()) {
            _nodes_lpeedit_item->set_sensitive(true);
        } else {
            _nodes_lpeedit_item->set_sensitive(false);
        }
    } else {
        _nodes_lpeedit_item->set_sensitive(false);
    }
}

void
NodeToolbar::watch_ec(SPDesktop* desktop, Inkscape::UI::Tools::ToolBase* tool)
{
    if (INK_IS_NODE_TOOL(tool)) {
        // watch selection
        c_selection_changed = desktop->getSelection()->connectChanged(sigc::mem_fun(*this, &NodeToolbar::sel_changed));
        c_selection_modified = desktop->getSelection()->connectModified(sigc::mem_fun(*this, &NodeToolbar::sel_modified));
        c_subselection_changed = desktop->connect_control_point_selected([=](void* sender, Inkscape::UI::ControlPointSelection* selection) {
            coord_changed(selection);
        });

        sel_changed(desktop->getSelection());
    } else {
        if (c_selection_changed)
            c_selection_changed.disconnect();
        if (c_selection_modified)
            c_selection_modified.disconnect();
        if (c_subselection_changed)
            c_subselection_changed.disconnect();
    }
}

void
NodeToolbar::sel_modified(Inkscape::Selection *selection, guint /*flags*/)
{
    sel_changed(selection);
}

/* is called when the node selection is modified */
void
NodeToolbar::coord_changed(Inkscape::UI::ControlPointSelection* selected_nodes) // gpointer /*shape_editor*/)
{
    // quit if run by the attr_changed listener
    if (_freeze) {
        return;
    }

    // in turn, prevent listener from responding
    _freeze = true;

    if (!_tracker) {
        return;
    }
    Unit const *unit = _tracker->getActiveUnit();
    g_return_if_fail(unit != nullptr);

    if (!selected_nodes || selected_nodes->empty()) {
        // no path selected
        _nodes_x_item->set_sensitive(false);
        _nodes_y_item->set_sensitive(false);
    } else {
        _nodes_x_item->set_sensitive(true);
        _nodes_y_item->set_sensitive(true);
        auto adj_x = _nodes_x_item->get_adjustment();
        auto adj_y = _nodes_y_item->get_adjustment();
        Geom::Coord oldx = Quantity::convert(adj_x->get_value(), unit, "px");
        Geom::Coord oldy = Quantity::convert(adj_y->get_value(), unit, "px");
        Geom::Point mid = selected_nodes->pointwiseBounds()->midpoint();

        // Adjust shown coordinate according to the selected page
        auto prefs = Inkscape::Preferences::get();
        if (prefs->getBool("/options/origincorrection/page", true)) {
            auto &pm = _desktop->getDocument()->getPageManager();
            mid *= pm.getSelectedPageAffine().inverse();
        }

        if (oldx != mid[Geom::X]) {
            adj_x->set_value(Quantity::convert(mid[Geom::X], "px", unit));
        }
        if (oldy != mid[Geom::Y]) {
            adj_y->set_value(Quantity::convert(mid[Geom::Y], "px", unit));
        }
    }

    _freeze = false;
}

void
NodeToolbar::edit_add()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->insertNodes();
    }
}

void
NodeToolbar::edit_add_min_x()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->insertNodesAtExtrema(Inkscape::UI::PointManipulator::EXTR_MIN_X);
    }
}

void
NodeToolbar::edit_add_max_x()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->insertNodesAtExtrema(Inkscape::UI::PointManipulator::EXTR_MAX_X);
    }
}

void
NodeToolbar::edit_add_min_y()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->insertNodesAtExtrema(Inkscape::UI::PointManipulator::EXTR_MIN_Y);
    }
}

void
NodeToolbar::edit_add_max_y()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->insertNodesAtExtrema(Inkscape::UI::PointManipulator::EXTR_MAX_Y);
    }
}

void
NodeToolbar::edit_delete()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        nt->_multipath->deleteNodes(prefs->getBool("/tools/nodes/delete_preserves_shape", true));
    }
}

void NodeToolbar::edit_join()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->joinNodes();
    }
}

void NodeToolbar::edit_break()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->breakNodes();
    }
}

void
NodeToolbar::edit_delete_segment()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->deleteSegments();
    }
}

void
NodeToolbar::edit_join_segment()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->joinSegments();
    }
}

void
NodeToolbar::edit_cusp()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->setNodeType(Inkscape::UI::NODE_CUSP);
    }
}

void
NodeToolbar::edit_smooth()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->setNodeType(Inkscape::UI::NODE_SMOOTH);
    }
}

void
NodeToolbar::edit_symmetrical()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->setNodeType(Inkscape::UI::NODE_SYMMETRIC);
    }
}

void
NodeToolbar::edit_auto()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->setNodeType(Inkscape::UI::NODE_AUTO);
    }
}

void
NodeToolbar::edit_toline()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->setSegmentType(Inkscape::UI::SEGMENT_STRAIGHT);
    }
}

void
NodeToolbar::edit_tocurve()
{
    NodeTool *nt = get_node_tool();
    if (nt) {
        nt->_multipath->setSegmentType(Inkscape::UI::SEGMENT_CUBIC_BEZIER);
    }
}

void NodeToolbar::on_pref_toggled(Gtk::ToggleButton *item, const Glib::ustring &path)
{
    auto prefs = Inkscape::Preferences::get();
    prefs->setBool(path, item->get_active());
}

} // namespace Inkscape::UI::Toolbar

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
