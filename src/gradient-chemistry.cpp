#define __SP_GRADIENT_CHEMISTRY_C__

/*
 * Various utility methods for gradients
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak
 *
 * Copyright (C) 2001-2005 authors
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */


#include "style.h"
#include "document-private.h"
#include "desktop-style.h"

#include "sp-gradient-reference.h"
#include "sp-linear-gradient.h"
#include "sp-radial-gradient.h"
#include "sp-stop.h"
#include "widgets/gradient-vector.h"

#include "sp-text.h"
#include "sp-tspan.h"
#include <libnr/nr-matrix-fns.h>
#include "xml/repr.h"
#include "svg/svg.h"
#include "svg/svg-color.h"


// Terminology:
//
// "vector" is a gradient that has stops but not position coords. It can be referenced by one or
// more privates. Objects should not refer to it directly. It has no radial/linear distinction.
//
// "private" is a gradient that has no stops but has position coords (e.g. center, radius etc for a
// radial). It references a vector for the actual colors. Each private is only used by one
// object. It is either linear or radial.

static void sp_gradient_repr_set_link(Inkscape::XML::Node *repr, SPGradient *gr);

SPGradient *
sp_gradient_ensure_vector_normalized(SPGradient *gr)
{
    g_return_val_if_fail(gr != NULL, NULL);
    g_return_val_if_fail(SP_IS_GRADIENT(gr), NULL);

    /* If we are already normalized vector, just return */
    if (gr->state == SP_GRADIENT_STATE_VECTOR) return gr;
    /* Fail, if we have wrong state set */
    if (gr->state != SP_GRADIENT_STATE_UNKNOWN) {
        g_warning("file %s: line %d: Cannot normalize private gradient to vector (%s)", __FILE__, __LINE__, SP_OBJECT_ID(gr));
        return NULL;
    }

    /* First make sure we have vector directly defined (i.e. gr has its own stops) */
    if (!gr->has_stops) {
        /* We do not have stops ourselves, so flatten stops as well */
        sp_gradient_ensure_vector(gr);
        g_assert(gr->vector.built);
        // this adds stops from gr->vector as children to gr
        sp_gradient_repr_write_vector (gr);
    }

    /* If gr hrefs some other gradient, remove the href */
    if (gr->ref->getObject()) {
        /* We are hrefing someone, so require flattening */
        SP_OBJECT(gr)->updateRepr(((SPObject *) gr)->repr, SP_OBJECT_WRITE_EXT | SP_OBJECT_WRITE_ALL);
        sp_gradient_repr_set_link(SP_OBJECT_REPR(gr), NULL);
    }

    /* Everything is OK, set state flag */
    gr->state = SP_GRADIENT_STATE_VECTOR;
    return gr;
}

/**
 * Creates new private gradient for the given vector
 */

static SPGradient *
sp_gradient_get_private_normalized(SPDocument *document, SPGradient *vector, SPGradientType type)
{
    g_return_val_if_fail(document != NULL, NULL);
    g_return_val_if_fail(vector != NULL, NULL);
    g_return_val_if_fail(SP_IS_GRADIENT(vector), NULL);
    g_return_val_if_fail(SP_GRADIENT_HAS_STOPS(vector), NULL);

    SPDefs *defs = (SPDefs *) SP_DOCUMENT_DEFS(document);

    // create a new private gradient of the requested type
    Inkscape::XML::Node *repr;
    if (type == SP_GRADIENT_TYPE_LINEAR) {
        repr = sp_repr_new("svg:linearGradient");
    } else {
        repr = sp_repr_new("svg:radialGradient");
    }

    // privates are garbage-collectable
    repr->setAttribute("inkscape:collect", "always");

    // link to vector
    sp_gradient_repr_set_link(repr, vector);

    /* Append the new private gradient to defs */
    SP_OBJECT_REPR(defs)->appendChild(repr);
    Inkscape::GC::release(repr);

    // get corresponding object
    SPGradient *gr = (SPGradient *) document->getObjectByRepr(repr);
    g_assert(gr != NULL);
    g_assert(SP_IS_GRADIENT(gr));

    return gr;
}

/**
Count how many times gr is used by the styles of o and its descendants
*/
guint
count_gradient_hrefs(SPObject *o, SPGradient *gr)
{
    if (!o)
        return 1;

    guint i = 0;

    SPStyle *style = SP_OBJECT_STYLE(o);
    if (style
        && style->fill.type == SP_PAINT_TYPE_PAINTSERVER
        && SP_IS_GRADIENT(SP_STYLE_FILL_SERVER(style))
        && SP_GRADIENT(SP_STYLE_FILL_SERVER(style)) == gr)
    {
        i ++;
    }
    if (style
        && style->stroke.type == SP_PAINT_TYPE_PAINTSERVER
        && SP_IS_GRADIENT(SP_STYLE_STROKE_SERVER(style))
        && SP_GRADIENT(SP_STYLE_STROKE_SERVER(style)) == gr)
    {
        i ++;
    }

    for (SPObject *child = sp_object_first_child(o);
         child != NULL; child = SP_OBJECT_NEXT(child)) {
        i += count_gradient_hrefs(child, gr);
    }

    return i;
}


/**
 * If gr has other users, create a new private; also check if gr links to vector, relink if not
 */
SPGradient *
sp_gradient_fork_private_if_necessary(SPGradient *gr, SPGradient *vector,
                                      SPGradientType type, SPObject *o)
{
    g_return_val_if_fail(gr != NULL, NULL);
    g_return_val_if_fail(SP_IS_GRADIENT(gr), NULL);

    // Orphaned gradient, no vector with stops at the end of the line; this used to be an assert
    // but i think we should not abort on this - maybe just write a validity warning into some sort
    // of log
    if (!vector || !SP_GRADIENT_HAS_STOPS(vector))
        return (gr);

    // user is the object that uses this gradient; normally it's item but for tspans, we
    // check its ancestor text so that tspans don't get different gradients from their
    // texts.
    SPObject *user = o;
    while (SP_IS_TSPAN(user)) {
        user = SP_OBJECT_PARENT(user);
    }

    // Check the number of uses of the gradient within this object;
    // if we are private and there are no other users,
    if (SP_OBJECT_HREFCOUNT(gr) <= count_gradient_hrefs(user, gr)) {
        // check vector
        if ( gr != vector && gr->ref->getObject() != vector ) {
            /* our href is not the vector, and vector is different from gr; relink */
            sp_gradient_repr_set_link(SP_OBJECT_REPR(gr), vector);
        }
        return gr;
    }

    SPDocument *doc = SP_OBJECT_DOCUMENT(gr);
    SPObject *defs = SP_DOCUMENT_DEFS(doc);

    if ((gr->has_stops) ||
        (gr->state != SP_GRADIENT_STATE_UNKNOWN) ||
        (SP_OBJECT_PARENT(gr) != SP_OBJECT(defs)) ||
        (SP_OBJECT_HREFCOUNT(gr) > 1)) {
        // we have to clone a fresh new private gradient for the given vector

        // create an empty one
        SPGradient *gr_new = sp_gradient_get_private_normalized(doc, vector, type);

        // copy all the attributes to it
        Inkscape::XML::Node *repr_new = SP_OBJECT_REPR(gr_new);
        Inkscape::XML::Node *repr = SP_OBJECT_REPR(gr);
        repr_new->setAttribute("gradientUnits", repr->attribute("gradientUnits"));
        repr_new->setAttribute("gradientTransform", repr->attribute("gradientTransform"));
        repr_new->setAttribute("spreadMethod", repr->attribute("spreadMethod"));
        if (SP_IS_RADIALGRADIENT(gr)) {
            repr_new->setAttribute("cx", repr->attribute("cx"));
            repr_new->setAttribute("cy", repr->attribute("cy"));
            repr_new->setAttribute("fx", repr->attribute("fx"));
            repr_new->setAttribute("fy", repr->attribute("fy"));
            repr_new->setAttribute("r", repr->attribute("r"));
        } else {
            repr_new->setAttribute("x1", repr->attribute("x1"));
            repr_new->setAttribute("y1", repr->attribute("y1"));
            repr_new->setAttribute("x2", repr->attribute("x2"));
            repr_new->setAttribute("y2", repr->attribute("y2"));
        }

        return gr_new;
    } else {
        return gr;
    }
}

SPGradient *
sp_gradient_fork_vector_if_necessary (SPGradient *gr)
{
    if (SP_OBJECT_HREFCOUNT(gr) > 1) {
        SPDocument *document = SP_OBJECT_DOCUMENT(gr);

        Inkscape::XML::Node *repr = SP_OBJECT_REPR (gr)->duplicate();
        SP_OBJECT_REPR (SP_DOCUMENT_DEFS (document))->addChild(repr, NULL);
        SPGradient *gr_new = (SPGradient *) document->getObjectByRepr(repr);
        gr_new = sp_gradient_ensure_vector_normalized (gr_new);
        Inkscape::GC::release(repr);
        return gr_new;
    }
    return gr;
}

/**
 * Convert an item's gradient to userspace _without_ preserving coords, setting them to defaults
 * instead. No forking or reapplying is done because this is only called for newly created privates.
 * @return The new gradient.
 */
SPGradient *
sp_gradient_reset_to_userspace (SPGradient *gr, SPItem *item)
{
    Inkscape::XML::Node *repr = SP_OBJECT_REPR(gr);

    // calculate the bbox of the item
    sp_document_ensure_up_to_date(SP_OBJECT_DOCUMENT(item));
    NR::Rect const bbox = item->invokeBbox(NR::identity()); // we need "true" bbox without item_i2d_affine

    NR::Coord const width = bbox.dimensions()[NR::X];
    NR::Coord const height = bbox.dimensions()[NR::Y];
    g_assert(width > 0 && height > 0);

    NR::Point const center = bbox.midpoint();

    if (SP_IS_RADIALGRADIENT(gr)) {
        sp_repr_set_svg_double(repr, "cx", center[NR::X]);
        sp_repr_set_svg_double(repr, "cy", center[NR::Y]);
        sp_repr_set_svg_double(repr, "fx", center[NR::X]);
        sp_repr_set_svg_double(repr, "fy", center[NR::Y]);
        sp_repr_set_svg_double(repr, "r", width/2);

        // we want it to be elliptic, not circular
        NR::Matrix squeeze = NR::Matrix (NR::translate (-center)) *
            NR::Matrix (NR::scale(1, height/width)) *
            NR::Matrix (NR::translate (center));

        gr->gradientTransform = squeeze;
        {
            gchar c[256];
            if (sp_svg_transform_write(c, 256, gr->gradientTransform)) {
                SP_OBJECT_REPR(gr)->setAttribute("gradientTransform", c);
            } else {
                SP_OBJECT_REPR(gr)->setAttribute("gradientTransform", NULL);
            }
        }
    } else {
        sp_repr_set_svg_double(repr, "x1", (center - NR::Point(width/2, 0))[NR::X]);
        sp_repr_set_svg_double(repr, "y1", (center - NR::Point(width/2, 0))[NR::Y]);
        sp_repr_set_svg_double(repr, "x2", (center + NR::Point(width/2, 0))[NR::X]);
        sp_repr_set_svg_double(repr, "y2", (center + NR::Point(width/2, 0))[NR::Y]);
    }

    // set the gradientUnits
    repr->setAttribute("gradientUnits", "userSpaceOnUse");

    return gr;
}

/**
 * Convert an item's gradient to userspace if necessary, also fork it if necessary.
 * @return The new gradient.
 */
SPGradient *
sp_gradient_convert_to_userspace(SPGradient *gr, SPItem *item, gchar const *property)
{
    g_return_val_if_fail(SP_IS_GRADIENT(gr), NULL);

    // First, fork it if it is shared
    gr = sp_gradient_fork_private_if_necessary(gr, sp_gradient_get_vector(gr, FALSE),
                                               SP_IS_RADIALGRADIENT(gr) ? SP_GRADIENT_TYPE_RADIAL : SP_GRADIENT_TYPE_LINEAR, SP_OBJECT(item));

    if (gr->units == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX) {

        Inkscape::XML::Node *repr = SP_OBJECT_REPR(gr);

        // calculate the bbox of the item
        sp_document_ensure_up_to_date(SP_OBJECT_DOCUMENT(item));
        NR::Rect const bbox = item->invokeBbox(NR::identity()); // we need "true" bbox without item_i2d_affine
        NR::Matrix bbox2user(bbox.dimensions()[NR::X], 0,
                             0, bbox.dimensions()[NR::Y],
                             bbox.min()[NR::X], bbox.min()[NR::Y]);

        /* skew is the additional transform, defined by the proportions of the item, that we need
         * to apply to the gradient in order to work around this weird bit from SVG 1.1
         * (http://www.w3.org/TR/SVG11/pservers.html#LinearGradients):
         *
         *   When gradientUnits="objectBoundingBox" and gradientTransform is the identity
         *   matrix, the stripes of the linear gradient are perpendicular to the gradient
         *   vector in object bounding box space (i.e., the abstract coordinate system where
         *   (0,0) is at the top/left of the object bounding box and (1,1) is at the
         *   bottom/right of the object bounding box). When the object's bounding box is not
         *   square, the stripes that are conceptually perpendicular to the gradient vector
         *   within object bounding box space will render non-perpendicular relative to the
         *   gradient vector in user space due to application of the non-uniform scaling
         *   transformation from bounding box space to user space.
         */
        NR::Matrix skew = bbox2user;
        double exp = skew.expansion();
        skew[0] /= exp;
        skew[1] /= exp;
        skew[2] /= exp;
        skew[3] /= exp;
        skew[4] = 0;
        skew[5] = 0;

        // apply skew to the gradient
        gr->gradientTransform = skew;
        {
            gchar c[256];
            if (sp_svg_transform_write(c, 256, gr->gradientTransform)) {
                SP_OBJECT_REPR(gr)->setAttribute("gradientTransform", c);
            } else {
                SP_OBJECT_REPR(gr)->setAttribute("gradientTransform", NULL);
            }
        }

        // Matrix to convert points to userspace coords; postmultiply by inverse of skew so
        // as to cancel it out when it's applied to the gradient during rendering
        NR::Matrix point_convert = bbox2user * skew.inverse();

        if (SP_IS_RADIALGRADIENT(gr)) {
            SPRadialGradient *rg = SP_RADIALGRADIENT(gr);

            // original points in the bbox coords
            NR::Point c_b = NR::Point(rg->cx.computed, rg->cy.computed);
            NR::Point f_b = NR::Point(rg->fx.computed, rg->fy.computed);
            double r_b = rg->r.computed;

            // converted points in userspace coords
            NR::Point c_u = c_b * point_convert;
            NR::Point f_u = f_b * point_convert;
            double r_u = r_b * point_convert.expansion();

            sp_repr_set_svg_double(repr, "cx", c_u[NR::X]);
            sp_repr_set_svg_double(repr, "cy", c_u[NR::Y]);
            sp_repr_set_svg_double(repr, "fx", f_u[NR::X]);
            sp_repr_set_svg_double(repr, "fy", f_u[NR::Y]);
            sp_repr_set_svg_double(repr, "r", r_u);

        } else {
            SPLinearGradient *lg = SP_LINEARGRADIENT(gr);

            NR::Point p1_b = NR::Point(lg->x1.computed, lg->y1.computed);
            NR::Point p2_b = NR::Point(lg->x2.computed, lg->y2.computed);

            NR::Point p1_u = p1_b * point_convert;
            NR::Point p2_u = p2_b * point_convert;

            sp_repr_set_svg_double(repr, "x1", p1_u[NR::X]);
            sp_repr_set_svg_double(repr, "y1", p1_u[NR::Y]);
            sp_repr_set_svg_double(repr, "x2", p2_u[NR::X]);
            sp_repr_set_svg_double(repr, "y2", p2_u[NR::Y]);
        }

        // set the gradientUnits
        repr->setAttribute("gradientUnits", "userSpaceOnUse");
    }

    // apply the gradient to the item (may be necessary if we forked it); not recursive
    // generally because grouped items will be taken care of later (we're being called
    // from sp_item_adjust_paint_recursive); however text and all its children should all
    // refer to one gradient, hence the recursive call for text (because we can't/don't
    // want to access tspans and set gradients on them separately)
    if (SP_IS_TEXT(item))
        sp_style_set_property_url(SP_OBJECT(item), property, SP_OBJECT(gr), true);
    else
        sp_style_set_property_url(SP_OBJECT(item), property, SP_OBJECT(gr), false);

    return gr;
}

void
sp_gradient_transform_multiply(SPGradient *gradient, NR::Matrix postmul, bool set)
{
    if (set) {
        gradient->gradientTransform = postmul;
    } else {
        gradient->gradientTransform *= postmul; // fixme: get gradient transform by climbing to hrefs?
    }
    gradient->gradientTransform_set = TRUE;

    gchar c[256];
    if (sp_svg_transform_write(c, 256, gradient->gradientTransform)) {
        SP_OBJECT_REPR(gradient)->setAttribute("gradientTransform", c);
    } else {
        SP_OBJECT_REPR(gradient)->setAttribute("gradientTransform", NULL);
    }
}

SPGradient *
sp_item_gradient (SPItem *item, bool fill_or_stroke)
{
    SPStyle *style = SP_OBJECT_STYLE (item);
    SPGradient *gradient = NULL;

    if (fill_or_stroke) {
        if (style && (style->fill.type == SP_PAINT_TYPE_PAINTSERVER)) {
            SPObject *server = SP_OBJECT_STYLE_FILL_SERVER(item);
            if (SP_IS_GRADIENT (server)) {
                gradient = SP_GRADIENT (server);
            }
        }
    } else {
        if (style && (style->stroke.type == SP_PAINT_TYPE_PAINTSERVER)) {
            SPObject *server = SP_OBJECT_STYLE_STROKE_SERVER(item);
            if (SP_IS_GRADIENT (server)) {
                gradient = SP_GRADIENT (server);
            }
        }
    }

   return gradient;
}


SPStop*
sp_first_stop(SPGradient *gradient)
{
  for (SPObject *ochild = sp_object_first_child(gradient); ochild != NULL; ochild = SP_OBJECT_NEXT(ochild)) {
	if (SP_IS_STOP (ochild))
		return SP_STOP(ochild);
  }
  return NULL;
}

SPStop*
sp_prev_stop(SPStop *stop, SPGradient *gradient)
{
	if (sp_object_first_child(SP_OBJECT(gradient)) == SP_OBJECT(stop))
		return NULL;
	SPObject *found = NULL;
	for ( SPObject *ochild = sp_object_first_child(SP_OBJECT(gradient)) ; ochild != NULL ; ochild = SP_OBJECT_NEXT(ochild) ) {
		if (SP_IS_STOP (ochild)) {
			found = ochild;
		}
		if (SP_OBJECT_NEXT(ochild) == SP_OBJECT(stop) || SP_OBJECT(ochild) == SP_OBJECT(stop)) {
			break;
		}
	}
	return SP_STOP(found);
}

SPStop*
sp_next_stop(SPStop *stop)
{
  for (SPObject *ochild = SP_OBJECT_NEXT(stop); ochild != NULL; ochild = SP_OBJECT_NEXT(ochild)) {
	if (SP_IS_STOP (ochild))
		return SP_STOP(ochild);
  }
  return NULL;
}

SPStop*
sp_last_stop(SPGradient *gradient)
{
    for (SPStop *stop = sp_first_stop (gradient); stop != NULL; stop = sp_next_stop (stop)) {
        if (sp_next_stop (stop) == NULL)
		return stop;
  }
  return NULL;
}


void
sp_item_gradient_edit_stop (SPItem *item, guint point_num, bool fill_or_stroke)
{
    SPGradient *gradient = sp_item_gradient (item, fill_or_stroke);

    if (!gradient || !SP_IS_GRADIENT(gradient))
        return;

    SPGradient *vector = sp_gradient_get_vector (gradient, false);
    switch (point_num) {
        case POINT_LG_P1:
        case POINT_RG_CENTER:
        case POINT_RG_FOCUS:
        {
            GtkWidget *dialog = sp_gradient_vector_editor_new (vector, sp_first_stop (vector));
            gtk_widget_show (dialog);
        }
        break;

        case POINT_LG_P2:
        case POINT_RG_R1:
        case POINT_RG_R2:
        {
            GtkWidget *dialog = sp_gradient_vector_editor_new (vector, sp_last_stop (vector));
            gtk_widget_show (dialog);
        }
        break;
        default:
            break;
    }
}

guint32
sp_item_gradient_stop_query_style (SPItem *item, guint point_num, bool fill_or_stroke)
{
    SPGradient *gradient = sp_item_gradient (item, fill_or_stroke);

    if (!gradient || !SP_IS_GRADIENT(gradient))
        return 0;

    SPGradient *vector = sp_gradient_get_vector (gradient, false);

    if (!vector) // orphan!
        return 0; // what else to do?

    switch (point_num) {
        case POINT_LG_P1:
        case POINT_RG_CENTER:
        case POINT_RG_FOCUS:
        {
            SPStop *first = sp_first_stop (vector);
            if (first) {
                return sp_stop_get_rgba32(first);
            }
        }
        break;

        case POINT_LG_P2:
        case POINT_RG_R1:
        case POINT_RG_R2:
        {
            SPStop *last = sp_last_stop (vector);
            if (last) {
                return sp_stop_get_rgba32(last);
            }
        }
        break;
        default:
            break;
    }
    return 0;
}

void
sp_item_gradient_stop_set_style (SPItem *item, guint point_num, bool fill_or_stroke, SPCSSAttr *stop)
{
    SPGradient *gradient = sp_item_gradient (item, fill_or_stroke);

    if (!gradient || !SP_IS_GRADIENT(gradient))
        return;

    SPGradient *vector = sp_gradient_get_vector (gradient, false);

    if (!vector) // orphan!
        return;

    vector = sp_gradient_fork_vector_if_necessary (vector);
    if ( gradient != vector && gradient->ref->getObject() != vector ) {
        sp_gradient_repr_set_link(SP_OBJECT_REPR(gradient), vector);
    }

    switch (point_num) {
        case POINT_LG_P1:
        case POINT_RG_CENTER:
        case POINT_RG_FOCUS:
        {
            SPStop *first = sp_first_stop (vector);
            if (first) {
                sp_repr_css_change (SP_OBJECT_REPR (first), stop, "style");
            }
        }
        break;

        case POINT_LG_P2:
        case POINT_RG_R1:
        case POINT_RG_R2:
        {
            SPStop *last = sp_last_stop (vector);
            if (last) {
                sp_repr_css_change (SP_OBJECT_REPR (last), stop, "style");
            }
        }
        break;
        default:
            break;
    }
}

void
sp_item_gradient_reverse_vector (SPItem *item, bool fill_or_stroke)
{
    SPGradient *gradient = sp_item_gradient (item, fill_or_stroke);
    if (!gradient || !SP_IS_GRADIENT(gradient))
        return;

    SPGradient *vector = sp_gradient_get_vector (gradient, false);
    if (!vector) // orphan!
        return;

    vector = sp_gradient_fork_vector_if_necessary (vector);
    if ( gradient != vector && gradient->ref->getObject() != vector ) {
        sp_gradient_repr_set_link(SP_OBJECT_REPR(gradient), vector);
    }

    GSList *child_reprs = NULL;
    GSList *child_objects = NULL;
    std::vector<double> offsets;
    for (SPObject *child = sp_object_first_child(vector);
         child != NULL; child = SP_OBJECT_NEXT(child)) {
        child_reprs = g_slist_prepend (child_reprs, SP_OBJECT_REPR(child));
        child_objects = g_slist_prepend (child_objects, child);
        offsets.push_back(sp_repr_get_double_attribute(SP_OBJECT_REPR(child), "offset", 0));
    }

    GSList *child_copies = NULL;
    for (GSList *i = child_reprs; i != NULL; i = i->next) {
        Inkscape::XML::Node *repr = (Inkscape::XML::Node *) i->data;
        child_copies = g_slist_append (child_copies, repr->duplicate());
    }


    for (GSList *i = child_objects; i != NULL; i = i->next) {
        SPObject *child = SP_OBJECT (i->data);
        child->deleteObject();
    }

    std::vector<double>::iterator iter = offsets.end() - 1;
    for (GSList *i = child_copies; i != NULL; i = i->next) {
        Inkscape::XML::Node *copy = (Inkscape::XML::Node *) i->data;
        vector->appendChildRepr(copy);
        sp_repr_set_svg_double (copy, "offset", 1 - *iter);
        iter --;
        Inkscape::GC::release(copy);
    }

    g_slist_free (child_reprs);
    g_slist_free (child_copies);
    g_slist_free (child_objects);
}



/**
Set the position of point point_num of the gradient applied to item (either fill_or_stroke) to
p_w (in desktop coordinates). Write_repr if you want the change to become permanent.
*/
void
sp_item_gradient_set_coords (SPItem *item, guint point_num, NR::Point p_w, bool fill_or_stroke, bool write_repr, bool scale)
{
    SPGradient *gradient = sp_item_gradient (item, fill_or_stroke);

    if (!gradient || !SP_IS_GRADIENT(gradient))
        return;

    gradient = sp_gradient_convert_to_userspace (gradient, item, fill_or_stroke? "fill" : "stroke");

    NR::Matrix i2d = sp_item_i2d_affine (item);
    NR::Point p = p_w * i2d.inverse();
    p *= (gradient->gradientTransform).inverse();
    // now p is in gradient's original coordinates

    Inkscape::XML::Node *repr = SP_OBJECT_REPR(gradient);

    if (SP_IS_LINEARGRADIENT(gradient)) {
        SPLinearGradient *lg = SP_LINEARGRADIENT(gradient);
        switch (point_num) {
            case POINT_LG_P1:
                if (scale) {
                    lg->x2.computed += (lg->x1.computed - p[NR::X]);
                    lg->y2.computed += (lg->y1.computed - p[NR::Y]);
                }
                lg->x1.computed = p[NR::X];
                lg->y1.computed = p[NR::Y];
                if (write_repr) {
                    if (scale) {
                        sp_repr_set_svg_double(repr, "x2", lg->x2.computed);
                        sp_repr_set_svg_double(repr, "y2", lg->y2.computed);
                    }
                    sp_repr_set_svg_double(repr, "x1", lg->x1.computed);
                    sp_repr_set_svg_double(repr, "y1", lg->y1.computed);
                } else {
                    SP_OBJECT (gradient)->requestModified(SP_OBJECT_MODIFIED_FLAG);
                }
                break;
            case POINT_LG_P2:
                if (scale) {
                    lg->x1.computed += (lg->x2.computed - p[NR::X]);
                    lg->y1.computed += (lg->y2.computed - p[NR::Y]);
                }
                lg->x2.computed = p[NR::X];
                lg->y2.computed = p[NR::Y];
                if (write_repr) {
                    if (scale) {
                        sp_repr_set_svg_double(repr, "x1", lg->x1.computed);
                        sp_repr_set_svg_double(repr, "y1", lg->y1.computed);
                    }
                    sp_repr_set_svg_double(repr, "x2", lg->x2.computed);
                    sp_repr_set_svg_double(repr, "y2", lg->y2.computed);
                } else {
                    SP_OBJECT (gradient)->requestModified(SP_OBJECT_MODIFIED_FLAG);
                }
			break;
		default:
			break;
		}
	} else if (SP_IS_RADIALGRADIENT(gradient)) {

		SPRadialGradient *rg = SP_RADIALGRADIENT(gradient);
		NR::Point c (rg->cx.computed, rg->cy.computed);
		NR::Point c_w = c * gradient->gradientTransform * i2d; // now in desktop coords
           if ((point_num == POINT_RG_R1 || point_num == POINT_RG_R2) && NR::L2 (p_w - c_w) < 1e-3) {
               // prevent setting a radius too close to the center
               return;
           }
		NR::Matrix new_transform;
		bool transform_set = false;

		switch (point_num) {
		case POINT_RG_CENTER:
			rg->fx.computed = p[NR::X] + (rg->fx.computed - rg->cx.computed);
			rg->fy.computed = p[NR::Y] + (rg->fy.computed - rg->cy.computed);
			rg->cx.computed = p[NR::X];
			rg->cy.computed = p[NR::Y];
			if (write_repr) {
				sp_repr_set_svg_double(repr, "fx", rg->fx.computed);
				sp_repr_set_svg_double(repr, "fy", rg->fy.computed);
				sp_repr_set_svg_double(repr, "cx", rg->cx.computed);
				sp_repr_set_svg_double(repr, "cy", rg->cy.computed);
			} else {
				SP_OBJECT (gradient)->requestModified(SP_OBJECT_MODIFIED_FLAG);
			}
			break;
		case POINT_RG_FOCUS:
			rg->fx.computed = p[NR::X];
			rg->fy.computed = p[NR::Y];
			if (write_repr) {
				sp_repr_set_svg_double(repr, "fx", rg->fx.computed);
				sp_repr_set_svg_double(repr, "fy", rg->fy.computed);
			} else {
				SP_OBJECT (gradient)->requestModified(SP_OBJECT_MODIFIED_FLAG);
			}
			break;
		case POINT_RG_R1:
			{
				NR::Point r1_w = (c + NR::Point(rg->r.computed, 0)) * gradient->gradientTransform * i2d;
				double r1_angle = NR::atan2(r1_w - c_w);
				double move_angle = NR::atan2(p_w - c_w) - r1_angle;
				double move_stretch = NR::L2(p_w - c_w) / NR::L2(r1_w - c_w);

				NR::Matrix move = NR::Matrix (NR::translate (-c_w)) *
												 NR::Matrix (NR::rotate(-r1_angle)) *
												 NR::Matrix (NR::scale(move_stretch, scale? move_stretch : 1)) *
												 NR::Matrix (NR::rotate(r1_angle)) *
												 NR::Matrix (NR::rotate(move_angle)) *
												 NR::Matrix (NR::translate (c_w));

				new_transform = gradient->gradientTransform * i2d * move * i2d.inverse();
				transform_set = true;

				break;
			}
		case POINT_RG_R2:
			{
				NR::Point r2_w = (c + NR::Point(0, -rg->r.computed)) * gradient->gradientTransform * i2d;
				double r2_angle = NR::atan2(r2_w - c_w);
				double move_angle = NR::atan2(p_w - c_w) - r2_angle;
				double move_stretch = NR::L2(p_w - c_w) / NR::L2(r2_w - c_w);

				NR::Matrix move = NR::Matrix (NR::translate (-c_w)) *
												 NR::Matrix (NR::rotate(-r2_angle)) *
												 NR::Matrix (NR::scale(move_stretch, scale? move_stretch : 1)) *
												 NR::Matrix (NR::rotate(r2_angle)) *
												 NR::Matrix (NR::rotate(move_angle)) *
												 NR::Matrix (NR::translate (c_w));

				new_transform = gradient->gradientTransform * i2d * move * i2d.inverse();
				transform_set = true;

				break;
			}
		}

		if (transform_set) {
				gradient->gradientTransform = new_transform;
				gradient->gradientTransform_set = TRUE;
				if (write_repr) {
					gchar s[256];
					if (sp_svg_transform_write(s, 256, gradient->gradientTransform)) {
						SP_OBJECT_REPR(gradient)->setAttribute("gradientTransform", s);
					} else {
						SP_OBJECT_REPR(gradient)->setAttribute("gradientTransform", NULL);
					}
				} else {
					SP_OBJECT (gradient)->requestModified(SP_OBJECT_MODIFIED_FLAG);
				}
		}
	}
}

SPGradient *
sp_item_gradient_get_vector (SPItem *item, bool fill_or_stroke)
{
    SPGradient *gradient = sp_item_gradient (item, fill_or_stroke);

    if (gradient)
        return sp_gradient_get_vector (gradient, false);
    return NULL;
}

SPGradientSpread
sp_item_gradient_get_spread (SPItem *item, bool fill_or_stroke)
{
    SPGradient *gradient = sp_item_gradient (item, fill_or_stroke);

    if (gradient)
        return sp_gradient_get_spread (gradient);
    return SP_GRADIENT_SPREAD_PAD;
}


/**
Returns the position of point point_num of the gradient applied to item (either fill_or_stroke),
in desktop coordinates.
*/

NR::Point
sp_item_gradient_get_coords (SPItem *item, guint point_num, bool fill_or_stroke)
{
    SPGradient *gradient = sp_item_gradient (item, fill_or_stroke);

    NR::Point p (0, 0);

    if (!gradient)
        return p;

    if (SP_IS_LINEARGRADIENT(gradient)) {
        SPLinearGradient *lg = SP_LINEARGRADIENT(gradient);
        switch (point_num) {
            case POINT_LG_P1:
                p = NR::Point (lg->x1.computed, lg->y1.computed);
                break;
            case POINT_LG_P2:
                p = NR::Point (lg->x2.computed, lg->y2.computed);
                break;
        }
    } else     if (SP_IS_RADIALGRADIENT(gradient)) {
        SPRadialGradient *rg = SP_RADIALGRADIENT(gradient);
        switch (point_num) {
            case POINT_RG_CENTER:
                p = NR::Point (rg->cx.computed, rg->cy.computed);
                break;
            case POINT_RG_FOCUS:
                p = NR::Point (rg->fx.computed, rg->fy.computed);
                break;
            case POINT_RG_R1:
                p = NR::Point (rg->cx.computed + rg->r.computed, rg->cy.computed);
                break;
            case POINT_RG_R2:
                p = NR::Point (rg->cx.computed, rg->cy.computed - rg->r.computed);
                break;
        }
    }

    if (SP_GRADIENT(gradient)->units == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX) {
        sp_document_ensure_up_to_date(SP_OBJECT_DOCUMENT(item));
        NR::Rect const bbox = item->invokeBbox(NR::identity()); // we need "true" bbox without item_i2d_affine
        p *= NR::Matrix(bbox.dimensions()[NR::X], 0,
                        0, bbox.dimensions()[NR::Y],
                        bbox.min()[NR::X], bbox.min()[NR::Y]);
    }
    p *= NR::Matrix(gradient->gradientTransform) * sp_item_i2d_affine(item);
    return p;
}


/**
 * Sets item fill or stroke to the gradient of the specified type with given vector, creating
 * new private gradient, if needed.
 * gr has to be a normalized vector.
 */

SPGradient *
sp_item_set_gradient(SPItem *item, SPGradient *gr, SPGradientType type, bool is_fill)
{
    g_return_val_if_fail(item != NULL, NULL);
    g_return_val_if_fail(SP_IS_ITEM(item), NULL);
    g_return_val_if_fail(gr != NULL, NULL);
    g_return_val_if_fail(SP_IS_GRADIENT(gr), NULL);
    g_return_val_if_fail(gr->state == SP_GRADIENT_STATE_VECTOR, NULL);

    SPStyle *style = SP_OBJECT_STYLE(item);
    g_assert(style != NULL);

    guint style_type = is_fill? style->fill.type : style->stroke.type;
    SPPaintServer *ps = NULL;
    if (style_type == SP_PAINT_TYPE_PAINTSERVER)
        ps = is_fill? SP_STYLE_FILL_SERVER(style) : SP_STYLE_STROKE_SERVER(style);

    if (ps
        && ( (type == SP_GRADIENT_TYPE_LINEAR && SP_IS_LINEARGRADIENT(ps)) ||
             (type == SP_GRADIENT_TYPE_RADIAL && SP_IS_RADIALGRADIENT(ps))   ) )
    {

        /* Current fill style is the gradient of the required type */
        SPGradient *current = SP_GRADIENT(ps);

        //g_print("hrefcount %d   count %d\n", SP_OBJECT_HREFCOUNT(ig), count_gradient_hrefs(SP_OBJECT(item), ig));

        if (SP_OBJECT_HREFCOUNT(current) == 1 ||
            SP_OBJECT_HREFCOUNT(current) == count_gradient_hrefs(SP_OBJECT(item), current)) {

            // current is private and it's either used once, or all its uses are by children of item;
            // so just change its href to vector

            if ( current != gr && sp_gradient_get_vector(current, false) != gr ) {
                /* href is not the vector */
                sp_gradient_repr_set_link(SP_OBJECT_REPR(current), gr);
            }
            SP_OBJECT(item)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
            return current;

        } else {

            // the gradient is not private, or it is shared with someone else;
            // normalize it (this includes creating new private if necessary)
            SPGradient *normalized = sp_gradient_fork_private_if_necessary(current, gr, type, item);

            g_return_val_if_fail(normalized != NULL, NULL);

            if (normalized != current) {

                /* We have to change object style here; recursive because this is used from
                 * fill&stroke and must work for groups etc. */
                sp_style_set_property_url(SP_OBJECT(item), is_fill? "fill" : "stroke", SP_OBJECT(normalized), true);
            }
            SP_OBJECT(item)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
            return normalized;
        }

    } else {
        /* Current fill style is not a gradient or wrong type, so construct everything */
        SPGradient *constructed = sp_gradient_get_private_normalized(SP_OBJECT_DOCUMENT(item), gr, type);
        constructed = sp_gradient_reset_to_userspace(constructed, item);
        sp_style_set_property_url(SP_OBJECT(item), ( is_fill ? "fill" : "stroke" ), SP_OBJECT(constructed), true);
        SP_OBJECT(item)->requestDisplayUpdate(( SP_OBJECT_MODIFIED_FLAG |
                                                SP_OBJECT_STYLE_MODIFIED_FLAG ));
        return constructed;
    }
}

static void
sp_gradient_repr_set_link(Inkscape::XML::Node *repr, SPGradient *link)
{
    g_return_if_fail(repr != NULL);
    g_return_if_fail(link != NULL);
    g_return_if_fail(SP_IS_GRADIENT(link));

    gchar *ref;
    if (link) {
        gchar const *id = SP_OBJECT_ID(link);
        size_t const len = strlen(id);
        ref = (gchar*) alloca(len + 2);
        *ref = '#';
        memcpy(ref + 1, id, len + 1);
    } else {
        ref = NULL;
    }

    repr->setAttribute("xlink:href", ref);
}

/*
 * Get default normalized gradient vector of document, create if there is none
 */

SPGradient *
sp_document_default_gradient_vector(SPDocument *document, guint32 color)
{
    SPDefs *defs = (SPDefs *) SP_DOCUMENT_DEFS(document);

    Inkscape::XML::Node *repr = sp_repr_new("svg:linearGradient");

    repr->setAttribute("inkscape:collect", "always");
    // set here, but removed when it's edited in the gradient editor
    // to further reduce clutter, we could
    // (1) here, search gradients by color and return what is found without duplication
    // (2) in fill & stroke, show only one copy of each gradient in list

    Inkscape::XML::Node *stop = sp_repr_new("svg:stop");

    gchar b[64];
    sp_svg_write_color(b, 64, color);

    {
        gchar *t = g_strdup_printf("stop-color:%s;stop-opacity:1;", b);
        stop->setAttribute("style", t);
        g_free(t);
    }

    stop->setAttribute("offset", "0");

    repr->appendChild(stop);
    Inkscape::GC::release(stop);

    stop = sp_repr_new("svg:stop");

    {
        gchar *t = g_strdup_printf("stop-color:%s;stop-opacity:0;", b);
        stop->setAttribute("style", t);
        g_free(t);
    }

    stop->setAttribute("offset", "1");

    repr->appendChild(stop);
    Inkscape::GC::release(stop);

    SP_OBJECT_REPR(defs)->addChild(repr, NULL);
    Inkscape::GC::release(repr);

    /* fixme: This does not look like nice */
    SPGradient *gr;
    gr = (SPGradient *) document->getObjectByRepr(repr);
    g_assert(gr != NULL);
    g_assert(SP_IS_GRADIENT(gr));
    /* fixme: Maybe add extra sanity check here */
    gr->state = SP_GRADIENT_STATE_VECTOR;

    return gr;
}

/**
Return the preferred vector for \a o, made from (in order of preference) its current vector,
current fill or stroke color, or from desktop style if \a o is NULL or doesn't have style.
*/
SPGradient *
sp_gradient_vector_for_object(SPDocument *const doc, SPDesktop *const desktop,
                              SPObject *const o, bool const is_fill)
{
    guint32 rgba = 0;
    if (o == NULL || SP_OBJECT_STYLE(o) == NULL) {
        rgba = sp_desktop_get_color(desktop, is_fill);
    } else {
        // take the color of the object
        SPStyle const &style = *SP_OBJECT_STYLE(o);
        SPIPaint const &paint = ( is_fill
                                  ? style.fill
                                  : style.stroke );
        if (paint.type == SP_PAINT_TYPE_COLOR) {
            rgba = sp_color_get_rgba32_ualpha(&paint.value.color, 0xff);
        } else if (paint.type == SP_PAINT_TYPE_PAINTSERVER) {
            SPObject *server = is_fill? SP_OBJECT_STYLE_FILL_SERVER(o) : SP_OBJECT_STYLE_STROKE_SERVER(o);
            if (SP_IS_GRADIENT (server)) {
                return sp_gradient_get_vector(SP_GRADIENT (server), TRUE);
            } else {
                rgba = sp_desktop_get_color(desktop, is_fill);
            }
        } else {
            // if o doesn't use flat color, then take current color of the desktop.
            rgba = sp_desktop_get_color(desktop, is_fill);
        }
    }

    return sp_document_default_gradient_vector(doc, rgba);
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=99 :
