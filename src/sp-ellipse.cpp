/*
 * SVG <ellipse> and related implementations
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Mitsuru Oka
 *   bulia byak <buliabyak@users.sf.net>
 *   Abhishek Sharma
 *
 * Copyright (C) 1999-2002 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "svg/svg.h"
#include "svg/path-string.h"
#include "xml/repr.h"
#include "attributes.h"
#include "style.h"
#include "display/curve.h"
#include <glibmm/i18n.h>
#include <2geom/transforms.h>
#include <2geom/pathvector.h>
#include "document.h"
#include "sp-ellipse.h"
#include "preferences.h"
#include "snap-candidate.h"

/* Common parent class */

#define noELLIPSE_VERBOSE

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SP_2PI (2 * M_PI)

#if 1
/* Hmmm... shouldn't this also qualify */
/* Whether it is faster or not, well, nobody knows */
#define sp_round(v,m) (((v) < 0.0) ? ((ceil((v) / (m) - 0.5)) * (m)) : ((floor((v) / (m) + 0.5)) * (m)))
#else
/* we do not use C99 round(3) function yet */
static double sp_round(double x, double y)
{
    double remain;

    g_assert(y > 0.0);

    /* return round(x/y) * y; */

    remain = fmod(x, y);

    if (remain >= 0.5*y)
        return x - remain + y;
    else
        return x - remain;
}
#endif

static gboolean sp_arc_set_elliptical_path_attribute(SPArc *arc, Inkscape::XML::Node *repr);

G_DEFINE_TYPE(SPGenericEllipse, sp_genericellipse, G_TYPE_OBJECT);

static void sp_genericellipse_class_init(SPGenericEllipseClass *klass)
{
}

CGenericEllipse::CGenericEllipse(SPGenericEllipse* genericEllipse) : CShape(genericEllipse) {
	this->spgenericEllipse = genericEllipse;
}

CGenericEllipse::~CGenericEllipse() {
}

SPGenericEllipse::SPGenericEllipse() : SPShape() {
	SPGenericEllipse* ellipse = this;

	ellipse->cgenericEllipse = new CGenericEllipse(ellipse);
	ellipse->typeHierarchy.insert(typeid(SPGenericEllipse));

	delete ellipse->cshape;
	ellipse->cshape = ellipse->cgenericEllipse;
	ellipse->clpeitem = ellipse->cgenericEllipse;
	ellipse->citem = ellipse->cgenericEllipse;
	ellipse->cobject = ellipse->cgenericEllipse;

    ellipse->cx.unset();
    ellipse->cy.unset();
    ellipse->rx.unset();
    ellipse->ry.unset();

    ellipse->start = 0.0;
    ellipse->end = SP_2PI;
    ellipse->closed = TRUE;
}

static void
sp_genericellipse_init(SPGenericEllipse *ellipse)
{
	new (ellipse) SPGenericEllipse();
}

void CGenericEllipse::update(SPCtx *ctx, guint flags) {
	SPGenericEllipse* object = this->spgenericEllipse;

    if (flags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG | SP_OBJECT_VIEWPORT_MODIFIED_FLAG)) {
        SPGenericEllipse *ellipse = (SPGenericEllipse *) object;
        SPStyle const *style = object->style;
        Geom::Rect const &viewbox = ((SPItemCtx const *) ctx)->viewport;

        double const dx = viewbox.width();
        double const dy = viewbox.height();
        double const dr = sqrt(dx*dx + dy*dy)/sqrt(2);
        double const em = style->font_size.computed;
        double const ex = em * 0.5; // fixme: get from pango or libnrtype
        ellipse->cx.update(em, ex, dx);
        ellipse->cy.update(em, ex, dy);
        ellipse->rx.update(em, ex, dr);
        ellipse->ry.update(em, ex, dr);
        static_cast<SPShape *>(object)->setShape();
    }

    CShape::update(ctx, flags);
}

void CGenericEllipse::update_patheffect(bool write) {
    SPShape *shape = this->spgenericEllipse;
    this->set_shape();

    if (write) {
        Inkscape::XML::Node *repr = shape->getRepr();
        if ( shape->_curve != NULL ) {
            gchar *str = sp_svg_write_path(shape->_curve->get_pathvector());
            repr->setAttribute("d", str);
            g_free(str);
        } else {
            repr->setAttribute("d", NULL);
        }
    }

    ((SPObject *)shape)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

/* fixme: Think (Lauris) */
/* Can't we use arcto in this method? */
void CGenericEllipse::set_shape() {
	SPGenericEllipse* shape = this->spgenericEllipse;

    if (sp_lpe_item_has_broken_path_effect(SP_LPE_ITEM(shape))) {
        g_warning ("The ellipse shape has unknown LPE on it! Convert to path to make it editable preserving the appearance; editing it as ellipse will remove the bad LPE");
        if (shape->getRepr()->attribute("d")) {
            // unconditionally read the curve from d, if any, to preserve appearance
            Geom::PathVector pv = sp_svg_read_pathv(shape->getRepr()->attribute("d"));
            SPCurve *cold = new SPCurve(pv);
            shape->setCurveInsync( cold, TRUE);
            cold->unref();
        }
        return;
    }

    double rx, ry, s, e;
    double x0, y0, x1, y1, x2, y2, x3, y3;
    double len;
    gint slice = FALSE;
 //   gint i;

    SPGenericEllipse *ellipse = (SPGenericEllipse *) shape;

    if ((ellipse->rx.computed < 1e-18) || (ellipse->ry.computed < 1e-18)) return;
    if (fabs(ellipse->end - ellipse->start) < 1e-9) return;

    sp_genericellipse_normalize(ellipse);

    rx = ellipse->rx.computed;
    ry = ellipse->ry.computed;

    // figure out if we have a slice, guarding against rounding errors
    len = fmod(ellipse->end - ellipse->start, SP_2PI);
    if (len < 0.0) len += SP_2PI;
    if (fabs(len) < 1e-8 || fabs(len - SP_2PI) < 1e-8) {
        slice = FALSE;
        ellipse->end = ellipse->start + SP_2PI;
    } else {
        slice = TRUE;
    }

    SPCurve * curve = new SPCurve();
    curve->moveto(cos(ellipse->start), sin(ellipse->start));

    for (s = ellipse->start; s < ellipse->end; s += M_PI_2) {
        e = s + M_PI_2;
        if (e > ellipse->end)
            e = ellipse->end;
        len = 4*tan((e - s)/4)/3;
        x0 = cos(s);
        y0 = sin(s);
        x1 = x0 + len * cos(s + M_PI_2);
        y1 = y0 + len * sin(s + M_PI_2);
        x3 = cos(e);
        y3 = sin(e);
        x2 = x3 + len * cos(e - M_PI_2);
        y2 = y3 + len * sin(e - M_PI_2);
#ifdef ELLIPSE_VERBOSE
        g_print("step %d s %f e %f coords %f %f %f %f %f %f\n",
                i, s, e, x1, y1, x2, y2, x3, y3);
#endif
        curve->curveto(x1,y1, x2,y2, x3,y3);
    }

    if (slice && ellipse->closed) {  // TODO: is this check for "ellipse->closed" necessary?
        curve->lineto(0., 0.);
    }
    if (ellipse->closed) {
        curve->closepath();
    }

    Geom::Affine aff = Geom::Scale(rx, ry) * Geom::Translate(ellipse->cx.computed, ellipse->cy.computed);
    curve->transform(aff);

    /* Reset the shape's curve to the "original_curve"
     * This is very important for LPEs to work properly! (the bbox might be recalculated depending on the curve in shape)*/
    shape->setCurveInsync( curve, TRUE);
    shape->setCurveBeforeLPE(curve);

    if (sp_lpe_item_has_path_effect(SP_LPE_ITEM(shape)) && sp_lpe_item_path_effects_enabled(SP_LPE_ITEM(shape))) {
        SPCurve *c_lpe = curve->copy();
        bool success = sp_lpe_item_perform_path_effect(SP_LPE_ITEM (shape), c_lpe);
        if (success) {
            shape->setCurveInsync( c_lpe, TRUE);
        }
        c_lpe->unref();
    }
    curve->unref();
}

void CGenericEllipse::snappoints(std::vector<Inkscape::SnapCandidatePoint> &p, Inkscape::SnapPreferences const *snapprefs) {
	SPGenericEllipse* item = this->spgenericEllipse;

    g_assert(item != NULL);
    g_assert(SP_IS_GENERICELLIPSE(item));

    SPGenericEllipse *ellipse = SP_GENERICELLIPSE(item);
    sp_genericellipse_normalize(ellipse);
    Geom::Affine const i2dt = item->i2dt_affine();

    // figure out if we have a slice, while guarding against rounding errors
    bool slice = false;
    double len = fmod(ellipse->end - ellipse->start, SP_2PI);
    if (len < 0.0) len += SP_2PI;
    if (fabs(len) < 1e-8 || fabs(len - SP_2PI) < 1e-8) {
        slice = false;
        ellipse->end = ellipse->start + SP_2PI;
    } else {
        slice = true;
    }

    double rx = ellipse->rx.computed;
    double ry = ellipse->ry.computed;
    double cx = ellipse->cx.computed;
    double cy = ellipse->cy.computed;

    Geom::Point pt;

    // Snap to the 4 quadrant points of the ellipse, but only if the arc
    // spans far enough to include them
    if (snapprefs->isTargetSnappable(Inkscape::SNAPTARGET_ELLIPSE_QUADRANT_POINT)) {
        double angle = 0;
        for (angle = 0; angle < SP_2PI; angle += M_PI_2) {
            if (angle >= ellipse->start && angle <= ellipse->end) {
                pt = Geom::Point(cx + cos(angle)*rx, cy + sin(angle)*ry) * i2dt;
                p.push_back(Inkscape::SnapCandidatePoint(pt, Inkscape::SNAPSOURCE_ELLIPSE_QUADRANT_POINT, Inkscape::SNAPTARGET_ELLIPSE_QUADRANT_POINT));
            }
        }
    }

    // Add the centre, if we have a closed slice or when explicitly asked for
    bool c1 = snapprefs->isTargetSnappable(Inkscape::SNAPTARGET_NODE_CUSP) && slice && ellipse->closed;
    bool c2 = snapprefs->isTargetSnappable(Inkscape::SNAPTARGET_OBJECT_MIDPOINT);
    if (c1 || c2) {
        pt = Geom::Point(cx, cy) * i2dt;
        if (c1) {
            p.push_back(Inkscape::SnapCandidatePoint(pt, Inkscape::SNAPSOURCE_NODE_CUSP, Inkscape::SNAPTARGET_NODE_CUSP));
        }
        if (c2) {
            p.push_back(Inkscape::SnapCandidatePoint(pt, Inkscape::SNAPSOURCE_OBJECT_MIDPOINT, Inkscape::SNAPTARGET_OBJECT_MIDPOINT));
        }
    }

    // And if we have a slice, also snap to the endpoints
    if (snapprefs->isTargetSnappable(Inkscape::SNAPTARGET_NODE_CUSP) && slice) {
        // Add the start point, if it's not coincident with a quadrant point
        if (fmod(ellipse->start, M_PI_2) != 0.0 ) {
            pt = Geom::Point(cx + cos(ellipse->start)*rx, cy + sin(ellipse->start)*ry) * i2dt;
            p.push_back(Inkscape::SnapCandidatePoint(pt, Inkscape::SNAPSOURCE_NODE_CUSP, Inkscape::SNAPTARGET_NODE_CUSP));
        }
        // Add the end point, if it's not coincident with a quadrant point
        if (fmod(ellipse->end, M_PI_2) != 0.0 ) {
            pt = Geom::Point(cx + cos(ellipse->end)*rx, cy + sin(ellipse->end)*ry) * i2dt;
            p.push_back(Inkscape::SnapCandidatePoint(pt, Inkscape::SNAPSOURCE_NODE_CUSP, Inkscape::SNAPTARGET_NODE_CUSP));
        }
    }
}

void
sp_genericellipse_normalize(SPGenericEllipse *ellipse)
{
    ellipse->start = fmod(ellipse->start, SP_2PI);
    ellipse->end = fmod(ellipse->end, SP_2PI);

    if (ellipse->start < 0.0)
        ellipse->start += SP_2PI;
    double diff = ellipse->start - ellipse->end;
    if (diff >= 0.0)
        ellipse->end += diff - fmod(diff, SP_2PI) + SP_2PI;

    /* Now we keep: 0 <= start < end <= 2*PI */
}

Inkscape::XML::Node* CGenericEllipse::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    SPGenericEllipse *ellipse = this->spgenericEllipse;
    SPGenericEllipse* object = ellipse;

    if (flags & SP_OBJECT_WRITE_EXT) {
        if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
            repr = xml_doc->createElement("svg:path");
        }

        sp_repr_set_svg_double(repr, "sodipodi:cx", ellipse->cx.computed);
        sp_repr_set_svg_double(repr, "sodipodi:cy", ellipse->cy.computed);
        sp_repr_set_svg_double(repr, "sodipodi:rx", ellipse->rx.computed);
        sp_repr_set_svg_double(repr, "sodipodi:ry", ellipse->ry.computed);

        if (SP_IS_ARC(ellipse)) {
            sp_arc_set_elliptical_path_attribute(SP_ARC(object), object->getRepr());
        }
    }
    this->set_shape(); // evaluate SPCurve

    CShape::write(xml_doc, repr, flags);

    return repr;
}

/* SVG <ellipse> element */

G_DEFINE_TYPE(SPEllipse, sp_ellipse, G_TYPE_OBJECT);

static void sp_ellipse_class_init(SPEllipseClass *klass)
{
}

CEllipse::CEllipse(SPEllipse* ellipse) : CGenericEllipse(ellipse) {
	this->spellipse = ellipse;
}

CEllipse::~CEllipse() {
}

SPEllipse::SPEllipse() : SPGenericEllipse() {
	SPEllipse* ellipse = this;

	ellipse->cellipse = new CEllipse(ellipse);
	ellipse->typeHierarchy.insert(typeid(SPEllipse));

	delete ellipse->cgenericEllipse;
	ellipse->cgenericEllipse = ellipse->cellipse;
	ellipse->cshape = ellipse->cellipse;
	ellipse->clpeitem = ellipse->cellipse;
	ellipse->citem = ellipse->cellipse;
	ellipse->cobject = ellipse->cellipse;
}

static void
sp_ellipse_init(SPEllipse *ellipse)
{
	new (ellipse) SPEllipse();
}

void CEllipse::build(SPDocument *document, Inkscape::XML::Node *repr) {
	CGenericEllipse::build(document, repr);

	SPEllipse* object = this->spellipse;
    object->readAttr( "cx" );
    object->readAttr( "cy" );
    object->readAttr( "rx" );
    object->readAttr( "ry" );
}


Inkscape::XML::Node* CEllipse::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
	SPGenericEllipse *ellipse = this->spellipse;

    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = xml_doc->createElement("svg:ellipse");
    }

    sp_repr_set_svg_double(repr, "cx", ellipse->cx.computed);
    sp_repr_set_svg_double(repr, "cy", ellipse->cy.computed);
    sp_repr_set_svg_double(repr, "rx", ellipse->rx.computed);
    sp_repr_set_svg_double(repr, "ry", ellipse->ry.computed);

    CGenericEllipse::write(xml_doc, repr, flags);

    return repr;
}


void CEllipse::set(unsigned int key, gchar const* value) {
    SPEllipse *ellipse = this->spellipse;
    SPEllipse* object = (SPEllipse*)ellipse;

    switch (key) {
        case SP_ATTR_CX:
            ellipse->cx.readOrUnset(value);
            object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_CY:
            ellipse->cy.readOrUnset(value);
            object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_RX:
            if (!ellipse->rx.read(value) || (ellipse->rx.value <= 0.0)) {
                ellipse->rx.unset();
            }
            object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_RY:
            if (!ellipse->ry.read(value) || (ellipse->ry.value <= 0.0)) {
                ellipse->ry.unset();
            }
            object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        default:
            CGenericEllipse::set(key, value);
            break;
    }
}

gchar* CEllipse::description() {
	return g_strdup(_("<b>Ellipse</b>"));
}


void
sp_ellipse_position_set(SPEllipse *ellipse, gdouble x, gdouble y, gdouble rx, gdouble ry)
{
    SPGenericEllipse *ge;

    g_return_if_fail(ellipse != NULL);
    g_return_if_fail(SP_IS_ELLIPSE(ellipse));

    ge = SP_GENERICELLIPSE(ellipse);

    ge->cx.computed = x;
    ge->cy.computed = y;
    ge->rx.computed = rx;
    ge->ry.computed = ry;

    ((SPObject *)ge)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

/* SVG <circle> element */

G_DEFINE_TYPE(SPCircle, sp_circle, G_TYPE_OBJECT);

static void
sp_circle_class_init(SPCircleClass *klass)
{
}

CCircle::CCircle(SPCircle* circle) : CGenericEllipse(circle) {
	this->spcircle = circle;
}

CCircle::~CCircle() {
}

SPCircle::SPCircle() : SPGenericEllipse() {
	SPCircle* circle = this;

	circle->ccircle = new CCircle(circle);
	circle->typeHierarchy.insert(typeid(SPCircle));

	delete circle->cgenericEllipse;
	circle->cgenericEllipse = circle->ccircle;
	circle->cshape = circle->ccircle;
	circle->clpeitem = circle->ccircle;
	circle->citem = circle->ccircle;
	circle->cobject = circle->ccircle;
}

static void
sp_circle_init(SPCircle *circle)
{
	new (circle) SPCircle();
}

void CCircle::build(SPDocument *document, Inkscape::XML::Node *repr) {
	SPCircle* object = this->spcircle;

    CGenericEllipse::build(document, repr);

    object->readAttr( "cx" );
    object->readAttr( "cy" );
    object->readAttr( "r" );
}


Inkscape::XML::Node* CCircle::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    SPGenericEllipse *ellipse = this->spcircle;

    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = xml_doc->createElement("svg:circle");
    }

    sp_repr_set_svg_double(repr, "cx", ellipse->cx.computed);
    sp_repr_set_svg_double(repr, "cy", ellipse->cy.computed);
    sp_repr_set_svg_double(repr, "r", ellipse->rx.computed);

    CGenericEllipse::write(xml_doc, repr, flags);

    return repr;
}


void CCircle::set(unsigned int key, gchar const* value) {
    SPGenericEllipse *ge = this->spcircle;
    SPCircle* object = (SPCircle*)ge;

    switch (key) {
        case SP_ATTR_CX:
            ge->cx.readOrUnset(value);
            object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_CY:
            ge->cy.readOrUnset(value);
            object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_R:
            if (!ge->rx.read(value) || ge->rx.value <= 0.0) {
                ge->rx.unset();
            }
            ge->ry = ge->rx;
            object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        default:
            CGenericEllipse::set(key, value);
            break;
    }
}

gchar* CCircle::description() {
	return g_strdup(_("<b>Circle</b>"));
}

/* <path sodipodi:type="arc"> element */

G_DEFINE_TYPE(SPArc, sp_arc, G_TYPE_OBJECT);

static void
sp_arc_class_init(SPArcClass *klass)
{
}

CArc::CArc(SPArc* arc) : CGenericEllipse(arc) {
	this->sparc = arc;
}

CArc::~CArc() {
}

SPArc::SPArc() : SPGenericEllipse() {
	SPArc* arc = this;

	arc->carc = new CArc(arc);
	arc->typeHierarchy.insert(typeid(SPArc));

	delete arc->cgenericEllipse;
	arc->cgenericEllipse = arc->carc;
	arc->cshape = arc->carc;
	arc->clpeitem = arc->carc;
	arc->citem = arc->carc;
	arc->cobject = arc->carc;
}

static void
sp_arc_init(SPArc *arc)
{
	new (arc) SPArc();
}

void CArc::build(SPDocument *document, Inkscape::XML::Node *repr) {
	SPArc* object = this->sparc;

	CGenericEllipse::build(document, repr);

    object->readAttr( "sodipodi:cx" );
    object->readAttr( "sodipodi:cy" );
    object->readAttr( "sodipodi:rx" );
    object->readAttr( "sodipodi:ry" );

    object->readAttr( "sodipodi:start" );
    object->readAttr( "sodipodi:end" );
    object->readAttr( "sodipodi:open" );
}

/*
 * sp_arc_set_elliptical_path_attribute:
 *
 * Convert center to endpoint parameterization and set it to repr.
 *
 * See SVG 1.0 Specification W3C Recommendation
 * ``F.6 Ellptical arc implementation notes'' for more detail.
 */
static gboolean
sp_arc_set_elliptical_path_attribute(SPArc *arc, Inkscape::XML::Node *repr)
{
    SPGenericEllipse *ge = SP_GENERICELLIPSE(arc);

    Inkscape::SVG::PathString str;

    Geom::Point p1 = sp_arc_get_xy(arc, ge->start);
    Geom::Point p2 = sp_arc_get_xy(arc, ge->end);
    double rx = ge->rx.computed;
    double ry = ge->ry.computed;

    str.moveTo(p1);

    double dt = fmod(ge->end - ge->start, SP_2PI);
    if (fabs(dt) < 1e-6) {
        Geom::Point ph = sp_arc_get_xy(arc, (ge->start + ge->end) / 2.0);
        str.arcTo(rx, ry, 0, true, true, ph)
           .arcTo(rx, ry, 0, true, true, p2)
           .closePath();
    } else {
        bool fa = (fabs(dt) > M_PI);
        bool fs = (dt > 0);
        str.arcTo(rx, ry, 0, fa, fs, p2);
        if (ge->closed) {
            Geom::Point center = Geom::Point(ge->cx.computed, ge->cy.computed);
            str.lineTo(center).closePath();
        }
    }

    repr->setAttribute("d", str.c_str());
    return true;
}

Inkscape::XML::Node* CArc::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
	SPArc* object = this->sparc;
    SPGenericEllipse *ge = object;
    SPArc *arc = object;

    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = xml_doc->createElement("svg:path");
    }

    if (flags & SP_OBJECT_WRITE_EXT) {
        repr->setAttribute("sodipodi:type", "arc");
        sp_repr_set_svg_double(repr, "sodipodi:cx", ge->cx.computed);
        sp_repr_set_svg_double(repr, "sodipodi:cy", ge->cy.computed);
        sp_repr_set_svg_double(repr, "sodipodi:rx", ge->rx.computed);
        sp_repr_set_svg_double(repr, "sodipodi:ry", ge->ry.computed);

        // write start and end only if they are non-trivial; otherwise remove
        gdouble len = fmod(ge->end - ge->start, SP_2PI);
        if (len < 0.0) len += SP_2PI;
        if (!(fabs(len) < 1e-8 || fabs(len - SP_2PI) < 1e-8)) {
            sp_repr_set_svg_double(repr, "sodipodi:start", ge->start);
            sp_repr_set_svg_double(repr, "sodipodi:end", ge->end);
            repr->setAttribute("sodipodi:open", (!ge->closed) ? "true" : NULL);
        } else {
            repr->setAttribute("sodipodi:end", NULL);
            repr->setAttribute("sodipodi:start", NULL);
            repr->setAttribute("sodipodi:open", NULL);
        }
    }

    // write d=
    sp_arc_set_elliptical_path_attribute(arc, repr);

    CGenericEllipse::write(xml_doc, repr, flags);

    return repr;
}

void CArc::set(unsigned int key, gchar const* value) {
	SPArc* object = this->sparc;
    SPGenericEllipse *ge = object;

    switch (key) {
        case SP_ATTR_SODIPODI_CX:
            ge->cx.readOrUnset(value);
            object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_SODIPODI_CY:
            ge->cy.readOrUnset(value);
            object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_SODIPODI_RX:
            if (!ge->rx.read(value) || ge->rx.computed <= 0.0) {
                ge->rx.unset();
            }
            object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_SODIPODI_RY:
            if (!ge->ry.read(value) || ge->ry.computed <= 0.0) {
                ge->ry.unset();
            }
            object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_SODIPODI_START:
            if (value) {
                sp_svg_number_read_d(value, &ge->start);
            } else {
                ge->start = 0;
            }
            object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_SODIPODI_END:
            if (value) {
                sp_svg_number_read_d(value, &ge->end);
            } else {
                ge->end = 2 * M_PI;
            }
            object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_SODIPODI_OPEN:
            ge->closed = (!value);
            object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        default:
            CGenericEllipse::set(key, value);
            break;
    }
}

void CArc::modified(guint flags) {
	SPArc* object = this->sparc;

    if (flags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG | SP_OBJECT_VIEWPORT_MODIFIED_FLAG)) {
        ((SPShape *) object)->setShape();
    }

    CGenericEllipse::modified(flags);
}


gchar* CArc::description() {
	SPArc* item = this->sparc;
    SPGenericEllipse *ge = item;

    gdouble len = fmod(ge->end - ge->start, SP_2PI);
    if (len < 0.0) len += SP_2PI;
    if (!(fabs(len) < 1e-8 || fabs(len - SP_2PI) < 1e-8)) {
        if (ge->closed) {
            return g_strdup(_("<b>Segment</b>"));
        } else {
            return g_strdup(_("<b>Arc</b>"));
        }
    } else {
        return g_strdup(_("<b>Ellipse</b>"));
    }
}

void
sp_arc_position_set(SPArc *arc, gdouble x, gdouble y, gdouble rx, gdouble ry)
{
    g_return_if_fail(arc != NULL);
    g_return_if_fail(SP_IS_ARC(arc));

    SPGenericEllipse *ge = SP_GENERICELLIPSE(arc);

    ge->cx.computed = x;
    ge->cy.computed = y;
    ge->rx.computed = rx;
    ge->ry.computed = ry;
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    // those pref values are in degrees, while we want radians
    if (prefs->getDouble("/tools/shapes/arc/start", 0.0) != 0)
        ge->start = prefs->getDouble("/tools/shapes/arc/start", 0.0) * M_PI / 180;
    if (prefs->getDouble("/tools/shapes/arc/end", 0.0) != 0)
        ge->end = prefs->getDouble("/tools/shapes/arc/end", 0.0) * M_PI / 180;
    if (!prefs->getBool("/tools/shapes/arc/open"))
        ge->closed = 1;
    else
        ge->closed = 0;

    ((SPObject *)arc)->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

Geom::Point sp_arc_get_xy(SPArc *arc, gdouble arg)
{
    SPGenericEllipse *ge = SP_GENERICELLIPSE(arc);

    return Geom::Point(ge->rx.computed * cos(arg) + ge->cx.computed,
                     ge->ry.computed * sin(arg) + ge->cy.computed);
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
