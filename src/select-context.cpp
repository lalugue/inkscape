#define __SP_SELECT_CONTEXT_C__

/*
 * Selection and transformation context
 *
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 1999-2002 Lauris Kaplinski
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <config.h>
#include <math.h>
#include <string.h>
#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include "macros.h"
#include "helper/sp-canvas-util.h"
#include "rubberband.h"
#include "inkscape-private.h"
#include "document.h"
#include "selection.h"
#include "desktop-affine.h"
#include "seltrans-handles.h"
#include "sp-cursor.h"
#include "pixmaps/cursor-select-m.xpm"
#include "pixmaps/cursor-select-d.xpm"
#include "pixmaps/handles.xpm"
#include "helper/sp-intl.h"
#include "widgets/spw-utilities.h"

#include "select-context.h"
#include "selection-chemistry.h"
#include "path-chemistry.h"
#include "desktop.h"
#include "dialogs/object-properties.h"
#include "sp-metrics.h"
#include "sp-item.h"
#include "desktop-snap.h"
#include "prefs-utils.h"

static void sp_select_context_class_init (SPSelectContextClass * klass);
static void sp_select_context_init (SPSelectContext * select_context);
static void sp_select_context_dispose (GObject *object);

static void sp_select_context_setup (SPEventContext *ec);
static void sp_select_context_set (SPEventContext *ec, const gchar *key, const gchar *val);
static gint sp_select_context_root_handler (SPEventContext * event_context, GdkEvent * event);
static gint sp_select_context_item_handler (SPEventContext * event_context, SPItem * item, GdkEvent * event);

static GtkWidget *sp_select_context_config_widget (SPEventContext *ec);

static void sp_selection_moveto (SPSelTrans * seltrans, double x, double y, guint state);

static SPEventContextClass * parent_class;

static GdkCursor *CursorSelectMouseover = NULL;
static GdkCursor *CursorSelectDragging = NULL;
GdkPixbuf * handles[13];

gint rb_escaped = 0; // if non-zero, rubberband was canceled by esc, so the next button release should not deselect
gint drag_escaped = 0; // if non-zero, drag was canceled by esc

static gint xp = 0, yp = 0; // where drag started
static gint tolerance = 0;
static bool within_tolerance = false;

GtkType
sp_select_context_get_type (void)
{
	static GType type = 0;
	if (!type) {
		GTypeInfo info = {
			sizeof (SPSelectContextClass),
			NULL, NULL,
			(GClassInitFunc) sp_select_context_class_init,
			NULL, NULL,
			sizeof (SPSelectContext),
			4,
			(GInstanceInitFunc) sp_select_context_init,
			NULL,	/* value_table */
		};
		type = g_type_register_static (SP_TYPE_EVENT_CONTEXT, "SPSelectContext", &info, (GTypeFlags)0);
	}
	return type;
}

static void
sp_select_context_class_init (SPSelectContextClass * klass)
{
	GObjectClass *object_class;
	SPEventContextClass * event_context_class;

	object_class = (GObjectClass *) klass;
	event_context_class = (SPEventContextClass *) klass;

	parent_class = (SPEventContextClass*)g_type_class_peek_parent (klass);

	object_class->dispose = sp_select_context_dispose;

	event_context_class->setup = sp_select_context_setup;
	event_context_class->set = sp_select_context_set;
	event_context_class->root_handler = sp_select_context_root_handler;
	event_context_class->item_handler = sp_select_context_item_handler;
	event_context_class->config_widget = sp_select_context_config_widget;

	// cursors in select context
	CursorSelectMouseover = sp_cursor_new_from_xpm (cursor_select_m_xpm , 1, 1); 
	CursorSelectDragging = sp_cursor_new_from_xpm (cursor_select_d_xpm , 1, 1); 
	// selection handles
      	handles[0]  = gdk_pixbuf_new_from_xpm_data ((const gchar **)handle_scale_nw_xpm);
	handles[1]  = gdk_pixbuf_new_from_xpm_data ((const gchar **)handle_scale_ne_xpm);
	handles[2]  = gdk_pixbuf_new_from_xpm_data ((const gchar **)handle_scale_h_xpm);
	handles[3]  = gdk_pixbuf_new_from_xpm_data ((const gchar **)handle_scale_v_xpm);
	handles[4]  = gdk_pixbuf_new_from_xpm_data ((const gchar **)handle_rotate_nw_xpm);
	handles[5]  = gdk_pixbuf_new_from_xpm_data ((const gchar **)handle_rotate_n_xpm);
	handles[6]  = gdk_pixbuf_new_from_xpm_data ((const gchar **)handle_rotate_ne_xpm);
	handles[7]  = gdk_pixbuf_new_from_xpm_data ((const gchar **)handle_rotate_e_xpm);
	handles[8]  = gdk_pixbuf_new_from_xpm_data ((const gchar **)handle_rotate_se_xpm);
	handles[9]  = gdk_pixbuf_new_from_xpm_data ((const gchar **)handle_rotate_s_xpm);
	handles[10] = gdk_pixbuf_new_from_xpm_data ((const gchar **)handle_rotate_sw_xpm);
	handles[11] = gdk_pixbuf_new_from_xpm_data ((const gchar **)handle_rotate_w_xpm);
	handles[12] = gdk_pixbuf_new_from_xpm_data ((const gchar **)handle_center_xpm);

}

static void
sp_select_context_init (SPSelectContext * sc)
{
	sc->dragging = FALSE;
	sc->moved = FALSE;
	sc->button_press_shift = FALSE;
}

static void
sp_select_context_dispose (GObject *object)
{
	SPSelectContext *sc;

	sc = SP_SELECT_CONTEXT (object);

	if (sc->grabbed) {
		sp_canvas_item_ungrab (sc->grabbed, GDK_CURRENT_TIME);
		sc->grabbed = NULL;
	}

	sp_sel_trans_shutdown (&sc->seltrans);

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
sp_select_context_setup (SPEventContext *ec)
{
	SPSelectContext * select_context;

	select_context = SP_SELECT_CONTEXT (ec);

	if (((SPEventContextClass *) parent_class)->setup)
		((SPEventContextClass *) parent_class)->setup (ec);

	sp_sel_trans_init (&select_context->seltrans, ec->desktop);

	sp_event_context_read (ec, "show");
	sp_event_context_read (ec, "transform");
}

static void
sp_select_context_set (SPEventContext *ec, const gchar *key, const gchar *val)
{
	SPSelectContext *sc;

	sc = SP_SELECT_CONTEXT (ec);

	if (!strcmp (key, "show")) {
		if (val && !strcmp (val, "outline")) {
			sc->seltrans.show = SP_SELTRANS_SHOW_OUTLINE;
		} else {
			sc->seltrans.show = SP_SELTRANS_SHOW_CONTENT;
		}
	} else if (!strcmp (key, "transform")) {
		if (val && !strcmp (val, "keep")) {
			sc->seltrans.transform = SP_SELTRANS_TRANSFORM_KEEP;
		} else {
			sc->seltrans.transform = SP_SELTRANS_TRANSFORM_OPTIMIZE;
		}
	}
}

static gint
sp_select_context_item_handler (SPEventContext *event_context, SPItem *item, GdkEvent *event)
{
	SPDesktop *desktop;
	SPSelectContext *sc;
	SPSelTrans *seltrans;
	SPSelection *selection;
	SPItem *item_at_point = NULL, *group_at_point = NULL;
	GdkCursor *cursor;
	NRPoint p;
	gint ret = FALSE;

	desktop = event_context->desktop;
	sc = SP_SELECT_CONTEXT (event_context);
	seltrans = &sc->seltrans;
	selection = SP_DT_SELECTION (desktop);

	tolerance = prefs_get_int_attribute_limited ("options.dragtolerance", "value", 0, 0, 100);

	switch (event->type) {
	case GDK_2BUTTON_PRESS:
		if (event->button.button == 1) {
			ret = TRUE;
		}
		break;
	case GDK_BUTTON_PRESS:
		if (event->button.button == 1) {
			/* Left mousebutton */

			// save drag origin
			xp = (gint) event->button.x; 
			yp = (gint) event->button.y;
			within_tolerance = true;

			if (!(event->button.state & GDK_SHIFT_MASK || event->button.state & GDK_CONTROL_MASK)) {
				// if shift or ctrl was pressed, do not move objects; 
				// pass the event to root handler which will perform rubberband, shift-click, ctrl-click, ctrl-drag

				sc->dragging = TRUE;
				sc->moved = FALSE;
				sc->item = item;

				rb_escaped = drag_escaped = 0;

 				sp_canvas_item_grab (SP_CANVAS_ITEM (desktop->drawing),
 						 GDK_KEY_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
 						 GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK,
 						 NULL, event->button.time);
 				sc->grabbed = SP_CANVAS_ITEM (desktop->drawing);
			
				ret = TRUE;
			}
		}
		break;
	case GDK_MOTION_NOTIFY:
		if (event->motion.state & GDK_BUTTON1_MASK) {
			/* Left mousebutton */
			if (sc->dragging) {
				ret = TRUE;

				if ( within_tolerance
				     && ( abs( (gint) event->motion.x - xp ) < tolerance )
				     && ( abs( (gint) event->motion.y - yp ) < tolerance ) ) {
					break; // do not drag if we're within tolerance from origin
				}
				// Once the user has moved farther than tolerance from the original location 
				// (indicating they intend to move the object, not click), then always process the 
				// motion notify coordinates as given (no snapping back to origin)
				within_tolerance = false; 

				sp_desktop_w2d_xy_point (desktop, &p, event->motion.x, event->motion.y);
				if (!sc->moved) {
					item_at_point = sp_desktop_item_at_point (desktop, event->button.x, event->button.y, TRUE);
					group_at_point = sp_desktop_group_at_point (desktop, event->button.x, event->button.y);
					// if neither a group nor an item (possibly in a group) at point are selected, set selection to the item passed with the event
					if ((!item_at_point || !sp_selection_item_selected (selection, item_at_point)) && 
					    (!group_at_point || !sp_selection_item_selected (selection, group_at_point))) {
						// have to select here since selecting is done when releasing
						sp_sel_trans_reset_state (seltrans);
						if (!sp_selection_item_selected (selection, sc->item))
							sp_selection_set_item (selection, sc->item);
					} // otherwise, do not change selection so that dragging selected-within-group items is possible
					sp_sel_trans_grab (seltrans, &p, -1, -1, FALSE);
					sc->moved = TRUE;
				}
				sp_selection_moveto (seltrans, p.x, p.y, event->button.state);
			}
		}
		break;
	case GDK_BUTTON_RELEASE:
		xp = yp = 0; 
		if (event->button.button == 1) {
			if (sc->moved) {
				// item has been moved
				sp_sel_trans_ungrab (seltrans);
				sc->moved = FALSE;
				sp_selection_update_statusbar (SP_DT_SELECTION (desktop));
			} else {
				// item has not been moved -> do selecting
				if (!sp_selection_is_empty (selection)) {
					if (event->button.state & GDK_SHIFT_MASK) {
						sp_sel_trans_reset_state (seltrans);
						if (sp_selection_item_selected (selection, sc->item)) {
							sp_selection_remove_item (selection, sc->item);
						} else {
							sp_selection_add_item (selection, sc->item);
						}
					} else {
						if (sp_selection_item_selected (selection, sc->item)) {
							sp_sel_trans_increase_state (seltrans);
						} else {
							sp_sel_trans_reset_state (seltrans);
							sp_selection_set_item (selection, sc->item);
						}
					}
				} else {
					sp_sel_trans_reset_state (seltrans);
					sp_selection_set_item (selection, sc->item);
				}
			}
			sc->dragging = FALSE;
			sc->item = NULL;
			if (sc->grabbed) {
				sp_canvas_item_ungrab (sc->grabbed, event->button.time);
				sc->grabbed = NULL;
			}
			ret = TRUE;
		} 
		break;
	case GDK_ENTER_NOTIFY:
		cursor = gdk_cursor_new (GDK_FLEUR);
		gdk_window_set_cursor (GTK_WIDGET (SP_DT_CANVAS (desktop))->window, cursor);
		gdk_cursor_destroy (cursor);
		break;
	case GDK_LEAVE_NOTIFY:
		gdk_window_set_cursor (GTK_WIDGET (SP_DT_CANVAS (desktop))->window, event_context->cursor);
		break;
	case GDK_KEY_PRESS:
		if (event->key.keyval == GDK_space) {
			if (sc->dragging && sc->grabbed) {
			/* stamping mode: show content mode moving */
				sp_sel_trans_stamp(seltrans);
				ret = TRUE;
			}
		}
		break;
	default:
		break;
	}

	if (!ret) {
		if (((SPEventContextClass *) parent_class)->item_handler)
			ret = ((SPEventContextClass *) parent_class)->item_handler (event_context, item, event);
	}

	return ret;
}

static gint
sp_select_context_root_handler (SPEventContext *event_context, GdkEvent * event)
{
	SPDesktop *desktop;
	SPSelectContext *sc;
	SPSelTrans *seltrans;
	SPSelection *selection;
	SPItem *item = NULL, *group = NULL;
	SPItem *item_at_point = NULL, *group_at_point = NULL, *item_in_group = NULL;
	gint ret = FALSE;
	NRPoint p;
	NRRect b;
	GSList *l;
	double nudge;

	desktop = event_context->desktop;
	sc = SP_SELECT_CONTEXT (event_context);
	seltrans = &sc->seltrans;
	selection = SP_DT_SELECTION (desktop);
	nudge = prefs_get_double_attribute_limited ("options.nudgedistance", "value", 2.8346457, 0, 1000); // default is 1 mm
	tolerance = prefs_get_int_attribute_limited ("options.dragtolerance", "value", 0, 0, 100);
	int snaps = prefs_get_int_attribute ("options.rotationsnapsperpi", "value", 12);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (event->button.button == 1) {

			// save drag origin
			xp = (gint) event->button.x; 
			yp = (gint) event->button.y;
			within_tolerance = true;

			sp_desktop_w2d_xy_point (desktop, &p, event->button.x, event->button.y);
			sp_rubberband_start (desktop, p.x, p.y);
			sp_canvas_item_grab (SP_CANVAS_ITEM (desktop->acetate),
					     GDK_KEY_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK,
					     NULL, event->button.time);
			sc->grabbed = SP_CANVAS_ITEM (desktop->acetate);
			
			// remember that shift or ctrl was on before button press
			// (originally intended by lauris for partial selects and then abandoned)
			sc->button_press_shift = (event->button.state & GDK_SHIFT_MASK) ? TRUE : FALSE;
			sc->button_press_ctrl = (event->button.state & GDK_CONTROL_MASK) ? TRUE : FALSE;

			sc->moved = FALSE;

			rb_escaped = drag_escaped = 0;

			ret = TRUE;
		}
		break;
	case GDK_MOTION_NOTIFY:
		if (event->motion.state & GDK_BUTTON1_MASK) {
			sp_desktop_w2d_xy_point (desktop, &p, event->motion.x, event->motion.y);

			if ( within_tolerance
			     && ( abs( (gint) event->motion.x - xp ) < tolerance )
			     && ( abs( (gint) event->motion.y - yp ) < tolerance ) ) {
				break; // do not drag if we're within tolerance from origin
			}
			// Once the user has moved farther than tolerance from the original location 
			// (indicating they intend to move the object, not click), then always process the 
			// motion notify coordinates as given (no snapping back to origin)
			within_tolerance = false; 

			if (sc->button_press_ctrl) // if ctrl was pressed and we're away from the origin, we want to ctrl-drag rather than click
				sc->dragging = TRUE;

			if (sc->dragging) {
				/* User has dragged fast, so we get events on root (lauris)*/
				// not only that; we will end up here when ctrl-dragging as well
				sp_rubberband_stop ();
				item_at_point = sp_desktop_item_at_point (desktop, event->button.x, event->button.y, FALSE);
				if (item_at_point || sc->moved) { // drag only if starting from a point, or if something is already grabbed
					if (!sc->moved) {
						item_in_group = sp_desktop_item_at_point (desktop, event->button.x, event->button.y, TRUE);
						group_at_point = sp_desktop_group_at_point (desktop, event->button.x, event->button.y);
						// if neither a group nor an item (possibly in a group) at point are selected, set selection to the item at point
						if ((!item_in_group || !sp_selection_item_selected (selection, item_in_group)) && 
								(!group_at_point || !sp_selection_item_selected (selection, group_at_point))) {
							// have to select here since selecting is done when releasing
							sp_sel_trans_reset_state (seltrans);
							// when simply ctrl-dragging, we don't want to go into groups
							if (item_at_point && !sp_selection_item_selected (selection, item_at_point))
								sp_selection_set_item (selection, item_at_point);
						} // otherwise, do not change selection so that dragging selected-within-group items is possible
						sp_sel_trans_grab (seltrans, &p, -1, -1, FALSE);
						sc->moved = TRUE;
					} 
					sp_selection_moveto (seltrans, p.x, p.y, event->button.state);
					ret = TRUE;
				} else {
					sc->dragging = FALSE;
				}
			} else {
				sp_rubberband_move (p.x, p.y);
			}
		}
		break;
	case GDK_BUTTON_RELEASE:
		xp = yp = 0; 
		if ((event->button.button == 1) && (sc->grabbed)) {
			if (sc->dragging) {
				if (sc->moved) {
					// item has been moved
					sp_sel_trans_ungrab (seltrans);
					sc->moved = FALSE;
					sp_selection_update_statusbar (SP_DT_SELECTION (desktop));
				} else {
					// item has not been moved -> simply a click, do selecting
					if (!sp_selection_is_empty (selection)) {
						if (event->button.state & GDK_SHIFT_MASK) { 
							// with shift, toggle selection
							sp_sel_trans_reset_state (seltrans);
							if (sp_selection_item_selected (selection, sc->item)) {
								sp_selection_remove_item (selection, sc->item);
							} else {
								sp_selection_add_item (selection, sc->item);
							}
						} else {
							// without shift, increase state (i.e. toggle scale/rotation handles)
							if (sp_selection_item_selected (selection, sc->item)) {
								sp_sel_trans_increase_state (seltrans);
							} else {
								sp_sel_trans_reset_state (seltrans);
								sp_selection_set_item (selection, sc->item);
							}
						}
					} else {
						sp_sel_trans_reset_state (seltrans);
						sp_selection_set_item (selection, sc->item);
					}
				}
				sc->dragging = FALSE;
				sc->item = NULL;
			} else {
				if (sp_rubberband_rect (&b) && !within_tolerance) {
					// this was a rubberband drag
					sp_rubberband_stop ();
					sp_sel_trans_reset_state (seltrans);
					// find out affected items:
					l = sp_document_items_in_box (SP_DT_DOCUMENT (desktop), &b);
					if (event->button.state & GDK_SHIFT_MASK) {
						// with shift, add to selection
						while (l) {
							item = SP_ITEM (l->data);
							if (sp_selection_item_selected (selection, item)) {
								// uncomment if you want toggle behavior for shift-rubberband
								//	sp_selection_remove_item (selection, item);
							} else {
								sp_selection_add_item (selection, item);
							}
							l = g_slist_remove (l, item);
						}
					} else {
						// without shift, simply select anew
						sp_selection_set_item_list (selection, l);
					}
				} else { // it was just a click, or a too small rubberband
					sp_rubberband_stop ();
					if (sc->button_press_shift && !rb_escaped && !drag_escaped) {
						// this was a shift-click, select what was clicked upon

						sc->button_press_shift = FALSE;

						if (sc->button_press_ctrl) {
							item = sp_desktop_item_at_point (desktop, event->button.x, event->button.y, TRUE);
							group = sp_desktop_group_at_point (desktop, event->button.x, event->button.y);
							sc->button_press_ctrl = FALSE;
						} else {
							item = sp_desktop_item_at_point (desktop, event->button.x, event->button.y, FALSE);
						}
						// if there's both a group and an item at point, deselect group to prevent double selection
						if (group) {
							if (sp_selection_item_selected (selection, group)) {
								sp_selection_remove_item (selection, group);
							}
						}
						if (item) {
							// toggle selected status
							if (sp_selection_item_selected (selection, item)) {
								sp_selection_remove_item (selection, item);
							} else {
								sp_selection_add_item (selection, item);
							}
							item = NULL;
						}

					} else if (sc->button_press_ctrl && !rb_escaped && !drag_escaped) { // ctrl-click 

						sc->button_press_ctrl = FALSE;

						item = sp_desktop_item_at_point (desktop, event->button.x, event->button.y, TRUE);

						if (item) {
							if (sp_selection_item_selected (selection, item)) {
								sp_sel_trans_increase_state (seltrans);
							} else {
								sp_sel_trans_reset_state (seltrans);
								sp_selection_set_item (selection, item);
							}
							item = NULL;
						}

					} else { // click without shift, simply deselect
						if (!sp_selection_is_empty (selection)) {
							if (!(rb_escaped) && !(drag_escaped)) // unless something was cancelled
								sp_selection_empty (selection);
							rb_escaped = 0;
							ret = TRUE;
						}
					}
				}
				ret = TRUE;
			}
			if (sc->grabbed) {
				sp_canvas_item_ungrab (sc->grabbed, event->button.time);
				sc->grabbed = NULL;
			}
		} 
		sc->button_press_shift = FALSE;
		break;
	case GDK_KEY_PRESS: // keybindings for select context
		switch (event->key.keyval) {  
		case GDK_Left: // move selection left
		case GDK_KP_Left: 
		case GDK_KP_4: 
			if (!MOD__CTRL) { // not ctrl
				if (MOD__ALT) { // alt
					if (MOD__SHIFT) sp_selection_move_screen (-10, 0); // shift
					else sp_selection_move_screen (-1, 0); // no shift
				}
				else { // no alt
					if (MOD__SHIFT) sp_selection_move (-10*nudge, 0); // shift
					else sp_selection_move (-nudge, 0); // no shift
				}
				ret = TRUE;
			}
			break;
		case GDK_Up: // move selection up
		case GDK_KP_Up: 
		case GDK_KP_8: 
			if (!MOD__CTRL) { // not ctrl
				if (MOD__ALT) { // alt
					if (MOD__SHIFT) sp_selection_move_screen (0, 10); // shift
					else sp_selection_move_screen (0, 1); // no shift
				}
				else { // no alt
					if (MOD__SHIFT) sp_selection_move (0, 10*nudge); // shift
					else sp_selection_move (0, nudge); // no shift
				}
				ret = TRUE;
			}
			break;
		case GDK_Right: // move selection right
		case GDK_KP_Right: 
		case GDK_KP_6: 
			if (!MOD__CTRL) { // not ctrl
				if (MOD__ALT) { // alt
					if (MOD__SHIFT) sp_selection_move_screen (10, 0); // shift
					else sp_selection_move_screen (1, 0); // no shift
				}
				else { // no alt
					if (MOD__SHIFT) sp_selection_move (10*nudge, 0); // shift
					else sp_selection_move (nudge, 0); // no shift
				}
				ret = TRUE;
			}
			break;
		case GDK_Down: // move selection down
		case GDK_KP_Down: 
		case GDK_KP_2: 
			if (!MOD__CTRL) { // not ctrl
				if (MOD__ALT) { // alt
					if (MOD__SHIFT) sp_selection_move_screen (0, -10); // shift
					else sp_selection_move_screen (0, -1); // no shift
				}
				else { // no alt
					if (MOD__SHIFT) sp_selection_move (0, -10*nudge); // shift
					else sp_selection_move (0, -nudge); // no shift
				}
				ret = TRUE;
			}
			break;
		case GDK_Escape:
			if (sc->dragging) {
				if (sc->moved) { // cancel dragging an object
					sp_sel_trans_ungrab (seltrans);
					sc->moved = FALSE;
					sc->dragging = FALSE;
					sc->item = NULL;
					sp_document_undo (SP_DT_DOCUMENT (desktop));
					drag_escaped = 1;
					sp_selection_update_statusbar (SP_DT_SELECTION (desktop));
					sp_view_set_statusf_flash (SP_VIEW(SP_EVENT_CONTEXT(sc)->desktop), "Move cancelled.");
				}
			} else {
				if (sp_rubberband_rect (&b)) { // cancel rubberband
					sp_rubberband_stop ();
					rb_escaped = 1;
					sp_selection_update_statusbar (SP_DT_SELECTION (desktop));
					sp_view_set_statusf_flash (SP_VIEW(SP_EVENT_CONTEXT(sc)->desktop), "Selection cancelled.");
				} else {
					sp_selection_empty (selection); // deselect
				}
			}
			ret = TRUE;
			break;
		case GDK_a:
		case GDK_A:
			if (MOD__CTRL_ONLY) {
				sp_edit_select_all (NULL, NULL);
				ret = TRUE;
			}
			break;
		case GDK_Tab: // Tab - cycle selection forward
			if (!(MOD__CTRL_ONLY || (MOD__CTRL && MOD__SHIFT))) {
				sp_selection_item_next ();
				ret = TRUE;
			}
			break;
		case GDK_ISO_Left_Tab: // Shift Tab - cycle selection backward
			if (!(MOD__CTRL_ONLY || (MOD__CTRL && MOD__SHIFT))) {
				sp_selection_item_prev ();
				ret = TRUE;
			} 
			break;
		case GDK_space:
		/* stamping mode: show outline mode moving */
		/* FIXME: Is next condition ok? (lauris) */
			if (sc->dragging && sc->grabbed) {
				sp_sel_trans_stamp(seltrans);
				ret = TRUE;
			}
			break;
 		case GDK_x:
 		case GDK_X:
 			if (MOD__ALT_ONLY) {
 				gpointer hb = sp_search_by_data_recursive (desktop->owner->aux_toolbox, (gpointer) "altx");
 				if (hb && GTK_IS_WIDGET(hb)) {
					gtk_widget_grab_focus (GTK_WIDGET (hb));
				}
 				ret = TRUE;
 			}
			break;
 		case GDK_bracketleft:
			sp_selection_rotate (selection, 180/snaps);
			ret = TRUE;
			break;
 		case GDK_bracketright:
			sp_selection_rotate (selection, -180/snaps);
			ret = TRUE;
			break;
 		}
	  break;
	default:
	  break;
	}
	
	if (!ret) {
		if (((SPEventContextClass *) parent_class)->root_handler)
			ret = ((SPEventContextClass *) parent_class)->root_handler (event_context, event);
	}
	
	return ret;
}

static void
sp_selection_moveto (SPSelTrans * seltrans, double x, double y, guint state)
{
	double dx, dy;
	NRMatrix move;
	NRPoint p, norm = {0,0};
	GString * xs, * ys;
	gchar status[80];
	SPDesktop * desktop;

	desktop = seltrans->desktop;

	sp_sel_trans_point_desktop (seltrans, &p);
	dx = x - p.x;
	dy = y - p.y;

	if (state & GDK_MOD1_MASK) {
		dx /= 10;
		dy /= 10;
	}

	dx = sp_desktop_horizontal_snap_list (desktop, seltrans->spp, seltrans->spp_length, dx);
	dy = sp_desktop_vertical_snap_list (desktop, seltrans->spp, seltrans->spp_length, dy);

	if (state & GDK_CONTROL_MASK) {
		if (fabs (dx) > fabs (dy)) {
			dy = 0.0;
		} else {
			dx = 0.0;
		}
	}

	nr_matrix_set_translate (&move, dx, dy);
	sp_sel_trans_transform (seltrans, &move, &norm);

	// status text
	xs = SP_PT_TO_METRIC_STRING (dx, SP_DEFAULT_METRIC);
	ys = SP_PT_TO_METRIC_STRING (dy, SP_DEFAULT_METRIC);
	sprintf (status, "Move  %s, %s", xs->str, ys->str);
	sp_view_set_status (SP_VIEW (seltrans->desktop), status, FALSE);
	g_string_free (xs, FALSE);
	g_string_free (ys, FALSE);
}

/* Gtk styff */

static void
sp_select_context_show_toggled (GtkToggleButton *button, SPSelectContext *sc)
{
	if (gtk_toggle_button_get_active (button)) {
		const gchar *val;
		val = (const gchar*)gtk_object_get_data (GTK_OBJECT (button), "value");
		sp_repr_set_attr (SP_EVENT_CONTEXT_REPR (sc), "show", val);
	}
}

static void
sp_select_context_transform_toggled (GtkToggleButton *button, SPSelectContext *sc)
{
	if (gtk_toggle_button_get_active (button)) {
		const gchar *val;
		val = (const gchar*)gtk_object_get_data (GTK_OBJECT (button), "value");
		sp_repr_set_attr (SP_EVENT_CONTEXT_REPR (sc), "transform", val);
	}
}

static GtkWidget *
sp_select_context_config_widget (SPEventContext *ec)
{
	SPSelectContext *sc;
	GtkWidget *vb, *f, *fb, *b;

	sc = SP_SELECT_CONTEXT (ec);

	vb = gtk_vbox_new (FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (vb), 4);

	f = gtk_frame_new (_("Visual transformation"));
	gtk_widget_show (f);
	gtk_box_pack_start (GTK_BOX (vb), f, FALSE, FALSE, 0);

	fb = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (fb);
	gtk_container_add (GTK_CONTAINER (f), fb);

	b = gtk_radio_button_new_with_label (NULL, _("Show content"));
	gtk_widget_show (b);
	gtk_object_set_data (GTK_OBJECT (b),  "value", (void*)"content");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b), sc->seltrans.show == SP_SELTRANS_SHOW_CONTENT);
	gtk_box_pack_start (GTK_BOX (fb), b, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (b), "toggled", GTK_SIGNAL_FUNC (sp_select_context_show_toggled), sc);

	b = gtk_radio_button_new_with_label (gtk_radio_button_group (GTK_RADIO_BUTTON (b)), _("Show outline"));
	gtk_widget_show (b);
	gtk_object_set_data (GTK_OBJECT (b), "value", (void*)"outline");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b), sc->seltrans.show == SP_SELTRANS_SHOW_OUTLINE);
	gtk_box_pack_start (GTK_BOX (fb), b, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (b), "toggled", GTK_SIGNAL_FUNC (sp_select_context_show_toggled), sc);

	f = gtk_frame_new (_("Object transformation"));
	gtk_widget_show (f);
	gtk_box_pack_start (GTK_BOX (vb), f, FALSE, FALSE, 0);

	fb = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (fb);
	gtk_container_add (GTK_CONTAINER (f), fb);

	b = gtk_radio_button_new_with_label (NULL, _("Optimize"));
	gtk_widget_show (b);
	gtk_object_set_data (GTK_OBJECT (b), "value", (void*)"optimize");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b), sc->seltrans.transform == SP_SELTRANS_TRANSFORM_OPTIMIZE);
	gtk_box_pack_start (GTK_BOX (fb), b, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (b), "toggled", GTK_SIGNAL_FUNC (sp_select_context_transform_toggled), sc);

	b = gtk_radio_button_new_with_label (gtk_radio_button_group (GTK_RADIO_BUTTON (b)), _("Preserve"));
	gtk_widget_show (b);
	gtk_object_set_data (GTK_OBJECT (b), "value", (void*)"keep");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b), sc->seltrans.transform == SP_SELTRANS_TRANSFORM_KEEP);
	gtk_box_pack_start (GTK_BOX (fb), b, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (b), "toggled", GTK_SIGNAL_FUNC (sp_select_context_transform_toggled), sc);

	return vb;
}
