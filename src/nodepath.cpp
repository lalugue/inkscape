#define __SP_NODEPATH_C__

/*
 * Path handler in node edit mode
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * This code is in public domain
 */

#include "config.h"

#include <math.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include "svg/svg.h"
#include "helper/sp-canvas-util.h"
#include "helper/sp-ctrlline.h"
#include "helper/sodipodi-ctrl.h"
#include "helper/sp-intl.h"
#include "knot.h"
#include "inkscape.h"
#include "document.h"
#include "desktop.h"
#include "desktop-handles.h"
#include "desktop-affine.h"
#include "desktop-snap.h"
#include "node-context.h"
#include "nodepath.h"
#include "selection-chemistry.h"
#include "selection.h"
#include "selection.h"
#include "xml/repr.h"
#include "xml/repr-private.h"
#include "object-edit.h"
#include "prefs-utils.h"

#include <libnr/nr-matrix-ops.h>
#include <libnr/nr-point-fns.h>

/* fixme: Implement these via preferences */

#define NODE_FILL          0xafafaf00
#define NODE_STROKE        0x000000ff
#define NODE_FILL_HI       0xaf907000
#define NODE_STROKE_HI     0x000000ff
#define NODE_FILL_SEL      0xffbb0000
#define NODE_STROKE_SEL    0x000000ff
#define NODE_FILL_SEL_HI   0xffee0000
#define NODE_STROKE_SEL_HI 0x000000ff
#define KNOT_FILL          0x000000
#define KNOT_STROKE        0x000000ff
#define KNOT_FILL_HI       0xffee0000
#define KNOT_STROKE_HI     0x000000ff

static GMemChunk *nodechunk = NULL;

/* Creation from object */

static ArtBpath *subpath_from_bpath(SPNodePath *np, ArtBpath *b, gchar const *t);
static gchar *parse_nodetypes(gchar const *types, gint length);

/* Object updating */

static void stamp_repr(SPNodePath *np);
static SPCurve *create_curve(SPNodePath *np);
static gchar *create_typestr(SPNodePath *np);

static void sp_node_ensure_ctrls(SPPathNode *node);

static void sp_nodepath_node_select(SPPathNode *node, gboolean incremental, gboolean override);

static void sp_node_set_selected(SPPathNode *node, gboolean selected);

/* Control knot placement, if node or other knot is moved */

static void sp_node_adjust_knot(SPPathNode *node, gint which_adjust);
static void sp_node_adjust_knots(SPPathNode *node);

/* Knot event handlers */

static void node_clicked(SPKnot *knot, guint state, gpointer data);
static void node_grabbed(SPKnot *knot, guint state, gpointer data);
static void node_ungrabbed(SPKnot *knot, guint state, gpointer data);
static gboolean node_request(SPKnot *knot, NR::Point *p, guint state, gpointer data);
static void node_ctrl_clicked(SPKnot *knot, guint state, gpointer data);
static void node_ctrl_grabbed(SPKnot *knot, guint state, gpointer data);
static void node_ctrl_ungrabbed(SPKnot *knot, guint state, gpointer data);
static gboolean node_ctrl_request(SPKnot *knot, NR::Point *p, guint state, gpointer data);
static void node_ctrl_moved(SPKnot *knot, NR::Point *p, guint state, gpointer data);

/* Constructors and destructors */

static SPNodeSubPath *sp_nodepath_subpath_new(SPNodePath *nodepath);
static void sp_nodepath_subpath_destroy(SPNodeSubPath *subpath);
static void sp_nodepath_subpath_close(SPNodeSubPath *sp);
static void sp_nodepath_subpath_open(SPNodeSubPath *sp, SPPathNode *n);
static SPPathNode * sp_nodepath_node_new(SPNodeSubPath *sp, SPPathNode *next, SPPathNodeType type, ArtPathcode code,
					 NR::Point *ppos, NR::Point *pos, NR::Point *npos);
static void sp_nodepath_node_destroy(SPPathNode *node);

/* Helpers */

static SPPathNodeSide *sp_node_get_side(SPPathNode *node, gint which);
static SPPathNodeSide *sp_node_opposite_side(SPPathNode *node, SPPathNodeSide *me);
static ArtPathcode sp_node_path_code_from_side(SPPathNode *node, SPPathNodeSide *me);

// active_node indicates mouseover node
static SPPathNode *active_node = NULL;

/**
\brief Creates new nodepath from item 
*/
SPNodePath *sp_nodepath_new(SPDesktop *desktop, SPItem *item)
{
	SPRepr *repr = SP_OBJECT (item)->repr;

	if (!SP_IS_PATH (item))
        return NULL;
	SPPath *path = SP_PATH(item);
	SPCurve *curve = sp_shape_get_curve(SP_SHAPE(path));
	g_return_val_if_fail (curve != NULL, NULL);

	ArtBpath *bpath = sp_curve_first_bpath (curve);
	gint length = curve->end;
	if (length == 0)
        return NULL; // prevent crash for one-node paths

	const gchar *nodetypes = sp_repr_attr(repr, "sodipodi:nodetypes");
	gchar *typestr = parse_nodetypes(nodetypes, length);

	//Create new nodepath
	SPNodePath *np = g_new(SPNodePath, 1);
	if (!np)
		return NULL;

	// Set defaults
	np->desktop     = desktop;
	np->path        = path;
	np->subpaths    = NULL;
	np->selected    = NULL;
	np->nodeContext = NULL; //Let the context that makes this set it

	// we need to update item's transform from the repr here,
	// because they may be out of sync when we respond 
	// to a change in repr by regenerating nodepath     --bb
	sp_object_read_attr (SP_OBJECT (item), "transform");

	np->i2d  = sp_item_i2d_affine(SP_ITEM(path));
	np->d2i  = np->i2d.inverse();
	np->repr = repr;

	/* Now the bitchy part (lauris) */

	ArtBpath *b = bpath;

	while (b->code != ART_END) {
		b = subpath_from_bpath (np, b, typestr + (b - bpath));
	}

	g_free (typestr);
	sp_curve_unref (curve);

	return np;
}

void sp_nodepath_destroy(SPNodePath *np) {

	if (!np)  //soft fail, like delete
		return;

	while (np->subpaths) {
		sp_nodepath_subpath_destroy ((SPNodeSubPath *) np->subpaths->data);
	}

	//Inform the context that made me, if any, that I am gone.
	if (np->nodeContext)
		np->nodeContext->nodepath = NULL;

	g_assert (!np->selected);

	g_free (np);
}


/**
 *  Return the node count of a given NodeSubPath
 */
static gint sp_nodepath_subpath_get_node_count(SPNodeSubPath *subpath)
{
    if (!subpath)
        return 0;
    gint nodeCount = g_list_length(subpath->nodes);
    return nodeCount;
}

/**
 *  Return the node count of a given NodePath
 */
static gint sp_nodepath_get_node_count(SPNodePath *np)
{
    if (!np)
        return 0;
    gint nodeCount = 0;
    for (GList *item = np->subpaths ; item ; item=item->next) {
        SPNodeSubPath *subpath = (SPNodeSubPath *)item->data;
        nodeCount += g_list_length(subpath->nodes);
    }
    return nodeCount;
}


/**
 * Clean up a nodepath after editing.
 * Currently we are deleting trivial subpaths
 */
static void sp_nodepath_cleanup(SPNodePath *nodepath)
{
	GList *badSubPaths = NULL;

	//Check all subpaths to be >=2 nodes
	for (GList *l = nodepath->subpaths; l ; l=l->next) {
		SPNodeSubPath *sp = (SPNodeSubPath *)l->data;
		if (sp_nodepath_subpath_get_node_count(sp)<2)
			badSubPaths = g_list_append(badSubPaths, sp);
	}

	//Delete them.  This second step is because sp_nodepath_subpath_destroy()
	//also removes the subpath from nodepath->subpaths
	for (GList *l = badSubPaths; l ; l=l->next) {
		SPNodeSubPath *sp = (SPNodeSubPath *)l->data;
		sp_nodepath_subpath_destroy(sp);
	}

	g_list_free(badSubPaths);

}



/**
\brief Returns true if the argument nodepath and the d attribute in its repr do not match. 
 This may happen if repr was changed in e.g. XML editor or by undo.
*/
gboolean nodepath_repr_d_changed(SPNodePath *np, char const *newd)
{
	g_assert (np);

	SPCurve *curve = create_curve(np);

	gchar *svgpath = sp_svg_write_path(curve->bpath);

	char const *attr_d = ( newd
			       ? newd
			       : sp_repr_attr(SP_OBJECT(np->path)->repr, "d") );
	gboolean ret = strcmp(attr_d, svgpath);

	g_free (svgpath);
	sp_curve_unref (curve);

	return ret;
}

/**
\brief Returns true if the argument nodepath and the sodipodi:nodetypes attribute in its repr do not match. 
 This may happen if repr was changed in e.g. XML editor or by undo.
*/
gboolean nodepath_repr_typestr_changed(SPNodePath *np, char const *newtypestr)
{
	g_assert (np);
	gchar *typestr = create_typestr(np);
	char const *attr_typestr = ( newtypestr
				     ? newtypestr
				     : sp_repr_attr(SP_OBJECT(np->path)->repr, "sodipodi:nodetypes") );
	gboolean const ret = (attr_typestr && strcmp(attr_typestr, typestr));

	g_free (typestr);

	return ret;
}

static ArtBpath *subpath_from_bpath(SPNodePath *np, ArtBpath *b, gchar const *t)
// XXX: Fixme: t should be a proper type, rather than gchar
{
	NR::Point ppos, pos, npos;

	g_assert ((b->code == ART_MOVETO) || (b->code == ART_MOVETO_OPEN));

	SPNodeSubPath *sp = sp_nodepath_subpath_new (np);
	bool const closed = (b->code == ART_MOVETO);

	pos = NR::Point(b->x3, b->y3) * np->i2d;
	if (b[1].code == ART_CURVETO) {
		npos = NR::Point(b[1].x1, b[1].y1) * np->i2d;
	} else {
		npos = pos;
	}
	SPPathNode *n;
	n = sp_nodepath_node_new(sp, NULL, (SPPathNodeType) *t, ART_MOVETO, &pos, &pos, &npos);
	g_assert (sp->first == n);
	g_assert (sp->last  == n);

	b++;
	t++;
	while ((b->code == ART_CURVETO) || (b->code == ART_LINETO)) {
		pos = NR::Point(b->x3, b->y3) * np->i2d;
		if (b->code == ART_CURVETO) {
			ppos = NR::Point(b->x2, b->y2) * np->i2d;
		} else {
			ppos = pos;
		}
		if (b[1].code == ART_CURVETO) {
			npos = NR::Point(b[1].x1, b[1].y1) * np->i2d;
		} else {
			npos = pos;
		}
		n = sp_nodepath_node_new (sp, NULL, (SPPathNodeType)*t, b->code, &ppos, &pos, &npos);
		b++;
		t++;
	}

	if (closed) sp_nodepath_subpath_close (sp);

	return b;
}

static gchar *parse_nodetypes(gchar const *types, gint length)
{
	g_assert (length > 0);

	gchar *typestr = g_new(gchar, length + 1);

	gint pos = 0;

	if (types) {
		for (gint i = 0; types[i] && ( i < length ); i++) {
			while ((types[i] > '\0') && (types[i] <= ' ')) i++;
			if (types[i] != '\0') {
				switch (types[i]) {
				case 's':
					typestr[pos++] = SP_PATHNODE_SMOOTH;
					break;
				case 'z':
					typestr[pos++] = SP_PATHNODE_SYMM;
					break;
				case 'c':
				default:
					typestr[pos++] = SP_PATHNODE_CUSP;
					break;
				}
			}
		}
	}

	while (pos < length) typestr[pos++] = SP_PATHNODE_CUSP;

	return typestr;
}

static void update_object(SPNodePath *np)
{
	g_assert(np);

	SPCurve *curve = create_curve(np);

	sp_shape_set_curve(SP_SHAPE(np->path), curve, TRUE);

	sp_curve_unref(curve);
}

static void update_repr_internal(SPNodePath *np)
{
	SPRepr *repr = SP_OBJECT(np->path)->repr;

	g_assert(np);

	SPCurve *curve = create_curve(np);
	gchar *typestr = create_typestr(np);
	gchar *svgpath = sp_svg_write_path(curve->bpath);

	sp_repr_set_attr (repr, "d", svgpath);
	sp_repr_set_attr (repr, "sodipodi:nodetypes", typestr);

	g_free (svgpath);
	g_free (typestr);
	sp_curve_unref (curve);
}

static void update_repr(SPNodePath *np)
{
	update_repr_internal(np);
	sp_document_done(SP_DT_DOCUMENT(np->desktop));
}

static void update_repr_keyed(SPNodePath *np, const gchar *key)
{
	update_repr_internal(np);
	sp_document_maybe_done(SP_DT_DOCUMENT(np->desktop), key);
}


static void stamp_repr(SPNodePath *np)
{
	g_assert (np);

	SPRepr *old_repr = SP_OBJECT(np->path)->repr;
	SPRepr *new_repr = sp_repr_duplicate(old_repr);
	
	SPCurve *curve = create_curve(np);
	gchar *typestr = create_typestr(np);

	gchar *svgpath = sp_svg_write_path (curve->bpath);

	sp_repr_set_attr (new_repr, "d", svgpath);
	sp_repr_set_attr (new_repr, "sodipodi:nodetypes", typestr);

	sp_document_add_repr(SP_DT_DOCUMENT (np->desktop),
			     new_repr);
	sp_document_done (SP_DT_DOCUMENT (np->desktop));

	sp_repr_unref (new_repr);
	g_free (svgpath);
	g_free (typestr);
	sp_curve_unref (curve);
}

static SPCurve *create_curve(SPNodePath *np)
{
	SPCurve *curve = sp_curve_new ();

	for (GList *spl = np->subpaths; spl != NULL; spl = spl->next) {
		SPNodeSubPath *sp = (SPNodeSubPath *) spl->data;
		sp_curve_moveto(curve,
				sp->first->pos * np->d2i);
		SPPathNode *n = sp->first->n.other;
		while (n) {
			NR::Point const end_pt = n->pos * np->d2i;
			switch (n->code) {
			case ART_LINETO:
				sp_curve_lineto(curve, end_pt);
				break;
			case ART_CURVETO:
				sp_curve_curveto(curve,
						 n->p.other->n.pos * np->d2i,
						 n->p.pos * np->d2i,
						 end_pt);
				break;
			default:
				g_assert_not_reached ();
				break;
			}
			if (n != sp->last) {
				n = n->n.other;
			} else {
				n = NULL;
			}
		}
		if (sp->closed) {
			sp_curve_closepath (curve);
		}
	}

	return curve;
}

static gchar *create_typestr(SPNodePath *np)
{
	gchar *typestr = g_new(gchar, 32);
	gint len = 32;
	gint pos = 0;

	for (GList *spl = np->subpaths; spl != NULL; spl = spl->next) {
		SPNodeSubPath *sp = (SPNodeSubPath *) spl->data;

		if (pos >= len) {
			typestr = g_renew (gchar, typestr, len + 32);
			len += 32;
		}

		typestr[pos++] = 'c';

		SPPathNode *n;
		n = sp->first->n.other;
		while (n) {
			gchar code;

			switch (n->type) {
			case SP_PATHNODE_CUSP:
				code = 'c';
				break;
			case SP_PATHNODE_SMOOTH:
				code = 's';
				break;
			case SP_PATHNODE_SYMM:
				code = 'z';
				break;
			default:
				g_assert_not_reached ();
				code = '\0';
				break;
			}

			if (pos >= len) {
				typestr = g_renew (gchar, typestr, len + 32);
				len += 32;
			}

			typestr[pos++] = code;

			if (n != sp->last) {
				n = n->n.other;
			} else {
				n = NULL;
			}
		}
	}

	if (pos >= len) {
		typestr = g_renew (gchar, typestr, len + 1);
		len += 1;
	}

	typestr[pos++] = '\0';

	return typestr;
}

static SPNodePath *sp_nodepath_current()
{
	if (!SP_ACTIVE_DESKTOP) return NULL;

	SPEventContext *event_context = (SP_ACTIVE_DESKTOP)->event_context;

	if (!SP_IS_NODE_CONTEXT (event_context)) return NULL;

	return SP_NODE_CONTEXT (event_context)->nodepath;
}

/**
 \brief Fills node and control positions for three nodes, splitting line
  marked by end at distance t
 */
static void sp_nodepath_line_midpoint(SPPathNode *new_path, SPPathNode *end, gdouble t)
{
	g_assert (new_path != NULL);
	g_assert (end      != NULL);

	g_assert (end->p.other == new_path);
	SPPathNode *start = new_path->p.other;
	g_assert (start);

	if (end->code == ART_LINETO) {
		new_path->type = SP_PATHNODE_CUSP;
		new_path->code = ART_LINETO;
		new_path->pos  = (t * start->pos + (1 - t) * end->pos);
	} else {
		new_path->type = SP_PATHNODE_SMOOTH;
		new_path->code = ART_CURVETO;
		gdouble s      = 1 - t;
		for(int dim = 0; dim < 2; dim++) {
			const NR::Coord f000 = start->pos[dim];
			const NR::Coord f001 = start->n.pos[dim];
			const NR::Coord f011 = end->p.pos[dim];
			const NR::Coord f111 = end->pos[dim];
			const NR::Coord f00t = s * f000 + t * f001;
			const NR::Coord f01t = s * f001 + t * f011;
			const NR::Coord f11t = s * f011 + t * f111;
			const NR::Coord f0tt = s * f00t + t * f01t;
			const NR::Coord f1tt = s * f01t + t * f11t;
			const NR::Coord fttt = s * f0tt + t * f1tt;
			start->n.pos[dim]    = f00t;
			new_path->p.pos[dim] = f0tt;
			new_path->pos[dim]   = fttt;
			new_path->n.pos[dim] = f1tt;
			end->p.pos[dim]      = f11t;
		}
	}
}

static SPPathNode *sp_nodepath_line_add_node(SPPathNode *end, gdouble t)
{
	g_assert (end);
	g_assert (end->subpath);
	g_assert (g_list_find (end->subpath->nodes, end));

	SPPathNode *start = end->p.other;
	g_assert( start->n.other == end );
	SPPathNode *newnode = sp_nodepath_node_new(end->subpath,
						   end,
						   SP_PATHNODE_SMOOTH,
						   (ArtPathcode)end->code,
						   &start->pos, &start->pos, &start->n.pos);
	sp_nodepath_line_midpoint(newnode, end, t);

	sp_node_ensure_ctrls (start);
	sp_node_ensure_ctrls (newnode);
	sp_node_ensure_ctrls (end);

	return newnode;
}

/**
\brief Break the path at the node: duplicate the argument node, start a new subpath with the duplicate, and copy all nodes after the argument node to it
*/
static SPPathNode *sp_nodepath_node_break(SPPathNode *node)
{
	g_assert (node);
	g_assert (node->subpath);
	g_assert (g_list_find (node->subpath->nodes, node));

	SPNodeSubPath *sp = node->subpath;
	SPNodePath *np    = sp->nodepath;

	if (sp->closed) {
		sp_nodepath_subpath_open (sp, node);
		return sp->first;
	} else {
		// no break for end nodes
		if (node == sp->first) return NULL;
		if (node == sp->last ) return NULL;

		// create a new subpath
		SPNodeSubPath *newsubpath = sp_nodepath_subpath_new(np);

		// duplicate the break node as start of the new subpath
		SPPathNode *newnode = sp_nodepath_node_new (newsubpath, NULL, (SPPathNodeType)node->type, ART_MOVETO, &node->pos, &node->pos, &node->n.pos);

		while (node->n.other) { // copy the remaining nodes into the new subpath
			SPPathNode *n  = node->n.other;
			SPPathNode *nn = sp_nodepath_node_new (newsubpath, NULL, (SPPathNodeType)n->type, (ArtPathcode)n->code, &n->p.pos, &n->pos, &n->n.pos);
			if (n->selected) {
				sp_nodepath_node_select (nn, TRUE, TRUE); //preserve selection
			}
			sp_nodepath_node_destroy (n); // remove the point on the original subpath
		}

		return newnode;
	}
}

static SPPathNode *sp_nodepath_node_duplicate(SPPathNode *node)
{
	g_assert (node);
	g_assert (node->subpath);
	g_assert (g_list_find (node->subpath->nodes, node));

	SPNodeSubPath *sp = node->subpath;

	ArtPathcode code = (ArtPathcode) node->code;
	if (code == ART_MOVETO) { // if node is the endnode,
		node->code = ART_LINETO; // new one is inserted before it, so change that to line
	}

	SPPathNode *newnode = sp_nodepath_node_new(sp, node, (SPPathNodeType)node->type, code,
						   &node->p.pos, &node->pos, &node->n.pos);

	return newnode;
}

static void sp_node_control_mirror_n_to_p(SPPathNode *node)
{
	node->p.pos = (node->pos + (node->pos - node->n.pos));
}

static void sp_node_control_mirror_p_to_n(SPPathNode *node)
{
	node->n.pos = (node->pos + (node->pos - node->p.pos));
}


static void sp_nodepath_set_line_type(SPPathNode *end, ArtPathcode code)
{
	g_assert (end);
	g_assert (end->subpath);
	g_assert (end->p.other);

	if (end->code == static_cast< guint > ( code ) )
		return;

	SPPathNode *start = end->p.other;

	end->code = code;

	if (code == ART_LINETO) {
		if (start->code == ART_LINETO) start->type = SP_PATHNODE_CUSP;
		if (end->n.other) {
			if (end->n.other->code == ART_LINETO) end->type = SP_PATHNODE_CUSP;
		}
		sp_node_adjust_knot (start, -1);
		sp_node_adjust_knot (end, 1);
	} else {
		NR::Point delta = end->pos - start->pos;
		start->n.pos = start->pos + delta / 3;
		end->p.pos = end->pos - delta / 3;
		sp_node_adjust_knot (start, 1);
		sp_node_adjust_knot (end, -1);
	}

	sp_node_ensure_ctrls (start);
	sp_node_ensure_ctrls (end);
}

static SPPathNode *sp_nodepath_set_node_type(SPPathNode *node, SPPathNodeType type)
{
	g_assert (node);
	g_assert (node->subpath);

	if (type == static_cast< SPPathNodeType> (static_cast< guint > (node->type) ) )
		return node;

	if ((node->p.other != NULL) && (node->n.other != NULL)) {
		if ((node->code == ART_LINETO) && (node->n.other->code == ART_LINETO)) {
			type = SP_PATHNODE_CUSP;
		}
	}

	node->type = type;

	if (node->type == SP_PATHNODE_CUSP) {
		g_object_set (G_OBJECT (node->knot), "shape", SP_KNOT_SHAPE_DIAMOND, "size", 9, NULL);
	} else {
		g_object_set (G_OBJECT (node->knot), "shape", SP_KNOT_SHAPE_SQUARE, "size", 7, NULL);
	}

	sp_node_adjust_knots (node);

	sp_nodepath_update_statusbar (node->subpath->nodepath);

	return node;
}

static void sp_node_moveto(SPPathNode *node, NR::Point p)
{
	NR::Point delta = p - node->pos;
	node->pos = p;

	node->p.pos += delta;
	node->n.pos += delta;

	if (node->p.other) {
		if (node->code == ART_LINETO) {
			sp_node_adjust_knot (node, 1);
			sp_node_adjust_knot (node->p.other, -1);
		}
	}
	if (node->n.other) {
		if (node->n.other->code == ART_LINETO) {
			sp_node_adjust_knot (node, -1);
			sp_node_adjust_knot (node->n.other, 1);
		}
	}

	sp_node_ensure_ctrls (node);
}

static void sp_nodepath_selected_nodes_move(SPNodePath *nodepath, gdouble dx, gdouble dy)
{
	gdouble dist, best[2];

	best[0] = best[1] = 1e18;
	NR::Point delta(dx, dy);
	NR::Point best_pt = delta;

	for (GList *l = nodepath->selected; l != NULL; l = l->next) {
		SPPathNode *n = (SPPathNode *) l->data;
		NR::Point p = n->pos + delta;
		for(int dim = 0; dim < 2; dim++) {
			dist = sp_desktop_dim_snap (nodepath->desktop, p, dim);
			if (dist < best[dim]) {
				g_message("Snapping %d", dim);
				best[dim] = dist;
				best_pt[dim] = p[dim] - n->pos[dim];
			}
		}
	}

	for (GList *l = nodepath->selected; l != NULL; l = l->next) {
		SPPathNode *n = (SPPathNode *) l->data;
		sp_node_moveto (n, n->pos + best_pt);
	}

	update_object (nodepath);
}

void
sp_node_selected_move (gdouble dx, gdouble dy)
{
	SPNodePath *nodepath = sp_nodepath_current ();
	if (!nodepath) return;

	sp_nodepath_selected_nodes_move (nodepath, dx, dy);

	if (dx == 0) {
		update_repr_keyed (nodepath, "node:move:vertical");
	} else if (dy == 0) {
		update_repr_keyed (nodepath, "node:move:horizontal");
	} else 
		update_repr (nodepath);
}

void
sp_node_selected_move_screen (gdouble dx, gdouble dy)
{
	// borrowed from sp_selection_move_screen in selection-chemistry.c
	// we find out the current zoom factor and divide deltas by it
	SPDesktop *desktop = SP_ACTIVE_DESKTOP;
	g_return_if_fail(SP_IS_DESKTOP (desktop));

	gdouble zoom = SP_DESKTOP_ZOOM (desktop);
	gdouble zdx = dx / zoom;
	gdouble zdy = dy / zoom;

	SPNodePath *nodepath = sp_nodepath_current ();
	if (!nodepath) return;

	sp_nodepath_selected_nodes_move (nodepath, zdx, zdy);

	if (dx == 0) {
		update_repr_keyed (nodepath, "node:move:vertical");
	} else if (dy == 0) {
		update_repr_keyed (nodepath, "node:move:horizontal");
	} else 
		update_repr (nodepath);
}

static void sp_node_ensure_knot(SPPathNode *node, gint which, gboolean show_knot)
{
	g_assert (node != NULL);

	SPPathNodeSide *side = sp_node_get_side (node, which);
	ArtPathcode code = sp_node_path_code_from_side (node, side);

	show_knot = show_knot && (code == ART_CURVETO);

	if (show_knot) {
		if (!SP_KNOT_IS_VISIBLE (side->knot)) {
			sp_knot_show (side->knot);
		}

		sp_knot_set_position (side->knot, &side->pos, 0);
		sp_canvas_item_show (side->line);

	} else {
		if (SP_KNOT_IS_VISIBLE (side->knot)) {
			sp_knot_hide (side->knot);
		}
		sp_canvas_item_hide (side->line);
	}
}

static void sp_node_ensure_ctrls(SPPathNode *node)
{
	g_assert (node != NULL);

	if (!SP_KNOT_IS_VISIBLE (node->knot)) {
		sp_knot_show (node->knot);
	}

	sp_knot_set_position (node->knot, &node->pos, 0);

	gboolean show_knots = node->selected;
	if (node->p.other != NULL) {
		if (node->p.other->selected) show_knots = TRUE;
	}
	if (node->n.other != NULL) {
		if (node->n.other->selected) show_knots = TRUE;
	}

	sp_node_ensure_knot (node, -1, show_knots);
	sp_node_ensure_knot (node, 1, show_knots);
}

static void sp_nodepath_subpath_ensure_ctrls(SPNodeSubPath *subpath)
{
	g_assert (subpath != NULL);

	for (GList *l = subpath->nodes; l != NULL; l = l->next) {
		sp_node_ensure_ctrls ((SPPathNode *) l->data);
	}
}

static void sp_nodepath_ensure_ctrls(SPNodePath *nodepath)
{
	g_assert (nodepath != NULL);

	for (GList *l = nodepath->subpaths; l != NULL; l = l->next)
		sp_nodepath_subpath_ensure_ctrls ((SPNodeSubPath *) l->data);
}

void
sp_node_selected_add_node (void)
{
	SPNodePath *nodepath = sp_nodepath_current ();
	if (!nodepath) return;

	GList *nl = NULL;

	for (GList *l = nodepath->selected; l != NULL; l = l->next) {
		SPPathNode *t = (SPPathNode *) l->data;
		g_assert (t->selected);
		if (t->p.other && t->p.other->selected) nl = g_list_prepend (nl, t);
	}

	while (nl) {
		SPPathNode *t = (SPPathNode *) nl->data;
		SPPathNode *n = sp_nodepath_line_add_node(t, 0.5);
		sp_nodepath_node_select (n, TRUE, FALSE);
		nl = g_list_remove (nl, t);
	}

	/* fixme: adjust ? */
	sp_nodepath_ensure_ctrls (nodepath);

	update_repr (nodepath);

	sp_nodepath_update_statusbar (nodepath);
}




void sp_node_selected_break()
{
	SPNodePath *nodepath = sp_nodepath_current ();
	if (!nodepath) return;

	GList *temp = NULL;
	for (GList *l = nodepath->selected; l != NULL; l = l->next) {
		SPPathNode *n = (SPPathNode *) l->data;
		SPPathNode *nn = sp_nodepath_node_break(n);
		if (nn == NULL) continue; // no break, no new node 
		temp = g_list_prepend (temp, nn);
	}

	if (temp) sp_nodepath_deselect (nodepath);
	for (GList *l = temp; l != NULL; l = l->next) {
		sp_nodepath_node_select ((SPPathNode *) l->data, TRUE, TRUE);
	}

	sp_nodepath_ensure_ctrls (nodepath);

	update_repr (nodepath);
}




/**
\brief duplicate selected nodes
*/
void sp_node_selected_duplicate()
{
	SPNodePath *nodepath = sp_nodepath_current ();
	if (!nodepath) return;

	GList *temp = NULL;
	for (GList *l = nodepath->selected; l != NULL; l = l->next) {
		SPPathNode *n = (SPPathNode *) l->data;
		SPPathNode *nn = sp_nodepath_node_duplicate(n);
		if (nn == NULL) continue; // could not duplicate
		temp = g_list_prepend(temp, nn);
	}

	if (temp) sp_nodepath_deselect (nodepath);
	for (GList *l = temp; l != NULL; l = l->next) {
		sp_nodepath_node_select ((SPPathNode *) l->data, TRUE, TRUE);
	}

	sp_nodepath_ensure_ctrls (nodepath);

	update_repr (nodepath);
}



void sp_node_selected_join()
{
	SPNodePath *nodepath = sp_nodepath_current ();
	if (!nodepath) return; // there's no nodepath when editing rects, stars, spirals or ellipses

	if (g_list_length (nodepath->selected) != 2) {
		sp_view_set_statusf_error (SP_VIEW(nodepath->desktop), "To join, you must have two endnodes selected.");
		return;
	}

	SPPathNode *a = (SPPathNode *) nodepath->selected->data;
	SPPathNode *b = (SPPathNode *) nodepath->selected->next->data;

	g_assert (a != b);
	g_assert (a->p.other || a->n.other);
	g_assert (b->p.other || b->n.other);

	if (((a->subpath->closed) || (b->subpath->closed)) || (a->p.other && a->n.other) || (b->p.other && b->n.other)) {
		sp_view_set_statusf_error (SP_VIEW(nodepath->desktop), "To join, you must have two endnodes selected.");
		return;
	}

	/* a and b are endpoints */

	NR::Point c = (a->pos + b->pos) / 2;

	if (a->subpath == b->subpath) {
		SPNodeSubPath *sp = a->subpath;
		sp_nodepath_subpath_close (sp);

		sp_nodepath_ensure_ctrls (sp->nodepath);

		update_repr (nodepath);

		return;
	}

	/* a and b are separate subpaths */
	SPNodeSubPath *sa = a->subpath;
	SPNodeSubPath *sb = b->subpath;
	NR::Point p;
	SPPathNode *n;
	ArtPathcode code;
	if (a == sa->first) {
		p = sa->first->n.pos;
		code = (ArtPathcode)sa->first->n.other->code;
		SPNodeSubPath *t = sp_nodepath_subpath_new (sa->nodepath);
		n = sa->last;
		sp_nodepath_node_new (t, NULL, SP_PATHNODE_CUSP, ART_MOVETO, &n->n.pos, &n->pos, &n->p.pos);
		n = n->p.other;
		while (n) {
			sp_nodepath_node_new (t, NULL, (SPPathNodeType)n->type, (ArtPathcode)n->n.other->code, &n->n.pos, &n->pos, &n->p.pos);
			n = n->p.other;
			if (n == sa->first) n = NULL;
		}
		sp_nodepath_subpath_destroy (sa);
		sa = t;
	} else if (a == sa->last) {
		p = sa->last->p.pos;
		code = (ArtPathcode)sa->last->code;
		sp_nodepath_node_destroy (sa->last);
	} else {
		code = ART_END;
		g_assert_not_reached ();
	}

	if (b == sb->first) {
		sp_nodepath_node_new (sa, NULL, SP_PATHNODE_CUSP, code, &p, &c, &sb->first->n.pos);
		for (n = sb->first->n.other; n != NULL; n = n->n.other) {
			sp_nodepath_node_new (sa, NULL, (SPPathNodeType)n->type, (ArtPathcode)n->code, &n->p.pos, &n->pos, &n->n.pos);
		}
	} else if (b == sb->last) {
		sp_nodepath_node_new (sa, NULL, SP_PATHNODE_CUSP, code, &p, &c, &sb->last->p.pos);
		for (n = sb->last->p.other; n != NULL; n = n->p.other) {
			sp_nodepath_node_new (sa, NULL, (SPPathNodeType)n->type, (ArtPathcode)n->n.other->code, &n->n.pos, &n->pos, &n->p.pos);
		}
	} else {
		g_assert_not_reached ();
	}
	/* and now destroy sb */

	sp_nodepath_subpath_destroy (sb);

	sp_nodepath_ensure_ctrls (sa->nodepath);

	update_repr (nodepath);

	sp_nodepath_update_statusbar (nodepath);
}



void sp_node_selected_join_segment()
{
	SPNodePath *nodepath = sp_nodepath_current ();
	if (!nodepath) return; // there's no nodepath when editing rects, stars, spirals or ellipses

	if (g_list_length (nodepath->selected) != 2) {
		sp_view_set_statusf_error (SP_VIEW(nodepath->desktop), "To join, you must have two endnodes selected.");
		return;
	}

	SPPathNode *a = (SPPathNode *) nodepath->selected->data;
	SPPathNode *b = (SPPathNode *) nodepath->selected->next->data;

	g_assert (a != b);
	g_assert (a->p.other || a->n.other);
	g_assert (b->p.other || b->n.other);

	if (((a->subpath->closed) || (b->subpath->closed)) || (a->p.other && a->n.other) || (b->p.other && b->n.other)) {
		sp_view_set_statusf_error (SP_VIEW(nodepath->desktop), "To join, you must have two endnodes selected.");
		return;
	}

	if (a->subpath == b->subpath) {
		SPNodeSubPath *sp = a->subpath;

		/*similar to sp_nodepath_subpath_close (sp), without the node destruction*/
		sp->closed = TRUE;

		sp->first->p.other = sp->last;
		sp->last->n.other  = sp->first;
		
		sp_node_control_mirror_p_to_n (sp->last);
		sp_node_control_mirror_n_to_p (sp->first);

		sp->first->code = sp->last->code;
		sp->first       = sp->last;

		sp_nodepath_ensure_ctrls (sp->nodepath);

		update_repr (nodepath);

		return;
	}

	/* a and b are separate subpaths */
	SPNodeSubPath *sa = a->subpath;
	SPNodeSubPath *sb = b->subpath;

	SPPathNode *n;
	NR::Point p;
	ArtPathcode code;
	if (a == sa->first) {
		code = (ArtPathcode) sa->first->n.other->code;
		SPNodeSubPath *t = sp_nodepath_subpath_new (sa->nodepath);
		n = sa->last;
		sp_nodepath_node_new (t, NULL, SP_PATHNODE_CUSP, ART_MOVETO, &n->n.pos, &n->pos, &n->p.pos);
		for (n = n->p.other; n != NULL; n = n->p.other) {
			sp_nodepath_node_new (t, NULL, (SPPathNodeType)n->type, (ArtPathcode)n->n.other->code, &n->n.pos, &n->pos, &n->p.pos);
		}
		sp_nodepath_subpath_destroy (sa);
		sa = t;
	} else if (a == sa->last) {
		code = (ArtPathcode)sa->last->code;
	} else {
		code = ART_END;
		g_assert_not_reached ();
	}

	if (b == sb->first) {
		n = sb->first;
		sp_node_control_mirror_p_to_n (sa->last);
		sp_nodepath_node_new (sa, NULL, SP_PATHNODE_CUSP, code, &n->p.pos, &n->pos, &n->n.pos);
		sp_node_control_mirror_n_to_p (sa->last);
		for (n = n->n.other; n != NULL; n = n->n.other) {
			sp_nodepath_node_new (sa, NULL, (SPPathNodeType)n->type, (ArtPathcode)n->code, &n->p.pos, &n->pos, &n->n.pos);
		}
	} else if (b == sb->last) {
		n = sb->last;
		sp_node_control_mirror_p_to_n (sa->last);
		sp_nodepath_node_new (sa, NULL, SP_PATHNODE_CUSP, code, &p, &n->pos, &n->p.pos);
		sp_node_control_mirror_n_to_p (sa->last);
		for (n = n->p.other; n != NULL; n = n->p.other) {
 			sp_nodepath_node_new (sa, NULL, (SPPathNodeType)n->type, (ArtPathcode)n->n.other->code, &n->n.pos, &n->pos, &n->p.pos);
		}
	} else {
		g_assert_not_reached ();
	}
	/* and now destroy sb */

	sp_nodepath_subpath_destroy (sb);

	sp_nodepath_ensure_ctrls (sa->nodepath);

	update_repr (nodepath);
}

void sp_node_selected_delete()
{
	SPNodePath *nodepath = sp_nodepath_current();
	if (!nodepath) return;
	if (!nodepath->selected) return;

	/* fixme: do it the right way */
	while (nodepath->selected) {
		SPPathNode *node = (SPPathNode *) nodepath->selected->data;
		sp_nodepath_node_destroy (node);
	}


	//clean up the nodepath (such as for trivial subpaths)
	sp_nodepath_cleanup(nodepath);

	sp_nodepath_ensure_ctrls (nodepath);

	update_repr (nodepath);

	// if the entire nodepath is removed, delete the selected object.
	if (nodepath->subpaths == NULL ||
		sp_nodepath_get_node_count(nodepath) < 2) {
			sp_nodepath_destroy (nodepath);
			sp_selection_delete (NULL, NULL);
			return;
	}

	sp_nodepath_update_statusbar (nodepath);
}



/**
 * This is the code for 'split'
 */
void
sp_node_selected_delete_segment (void)
{
	SPPathNode *start, *end;     //Start , end nodes.  not inclusive
	SPPathNode *curr, *next;     //Iterators

	SPNodePath *nodepath = sp_nodepath_current ();
	if (!nodepath) return; // there's no nodepath when editing rects, stars, spirals or ellipses

	if (g_list_length (nodepath->selected) != 2) {
		sp_view_set_statusf_error (SP_VIEW(nodepath->desktop),
                "You must select two non-endpoint nodes on a path between which to delete segments.");
		return;
	}
	
    //Selected nodes, not inclusive
	SPPathNode *a = (SPPathNode *) nodepath->selected->data;
	SPPathNode *b = (SPPathNode *) nodepath->selected->next->data;

	if (     ( a==b)                       ||  //same node
             (a->subpath  != b->subpath )  ||  //not the same path
             (!a->p.other || !a->n.other)  ||  //one of a's sides does not have a segment
             (!b->p.other || !b->n.other) )    //one of b's sides does not have a segment
		{
		sp_view_set_statusf_error (SP_VIEW(nodepath->desktop),
		"You must select two non-endpoint nodes on a path between which to delete segments.");
		return;
		}

	//###########################################
	//# BEGIN EDITS
	//###########################################
	//##################################
	//# CLOSED PATH
	//##################################
	if (a->subpath->closed) {


        gboolean reversed = FALSE;

		//Since we can go in a circle, we need to find the shorter distance.
		//  a->b or b->a
		start = end = NULL;
		int distance    = 0;
		int minDistance = 0;
		for (curr = a->n.other ; curr && curr!=a ; curr=curr->n.other) {
			if (curr==b) {
				//printf("a to b:%d\n", distance);
				start = a;//go from a to b
				end   = b;
				minDistance = distance;
				//printf("A to B :\n");
				break;
			}
			distance++;
		}

		//try again, the other direction
		distance = 0;
		for (curr = b->n.other ; curr && curr!=b ; curr=curr->n.other) {
			if (curr==a) {
				//printf("b to a:%d\n", distance);
				if (distance < minDistance) {
					start    = b;  //we go from b to a
					end      = a;
                    reversed = TRUE;
					//printf("B to A\n");
				}
				break;
			}
			distance++;
		}

		
		//Copy everything from 'end' to 'start' to a new subpath
		SPNodeSubPath *t = sp_nodepath_subpath_new (nodepath);
		for (curr=end ; curr ; curr=curr->n.other) {
            ArtPathcode code = (ArtPathcode) curr->code;
            if (curr == end)
                code = ART_MOVETO;
			sp_nodepath_node_new (t, NULL, 
                (SPPathNodeType)curr->type, code,
			    &curr->p.pos, &curr->pos, &curr->n.pos);
			if (curr == start)
				break;
		}
		sp_nodepath_subpath_destroy(a->subpath);


	}



	//##################################
	//# OPEN PATH
	//##################################
	else {

		//We need to get the direction of the list between A and B
		//Can we walk from a to b?
		start = end = NULL;
		for (curr = a->n.other ; curr && curr!=a ; curr=curr->n.other) {
			if (curr==b) {
				start = a;  //did it!  we go from a to b
				end   = b;
				//printf("A to B\n");
				break;
			}
		}
		if (!start) {//didn't work?  let's try the other direction
			for (curr = b->n.other ; curr && curr!=b ; curr=curr->n.other) {
				if (curr==a) {
					start = b;  //did it!  we go from b to a
					end   = a;
					//printf("B to A\n");
					break;
				}
			}
		}
		if (!start) {
			sp_view_set_statusf_error (SP_VIEW(nodepath->desktop),
			"Cannot find path between nodes.");
			return;
		}



		//Copy everything after 'end' to a new subpath
		SPNodeSubPath *t = sp_nodepath_subpath_new (nodepath);
		for (curr=end ; curr ; curr=curr->n.other) {
			sp_nodepath_node_new (t, NULL, (SPPathNodeType)curr->type, (ArtPathcode)curr->code,
			&curr->p.pos, &curr->pos, &curr->n.pos);
		}

		//Now let us do our deletion.  Since the tail has been saved, go all the way to the end of the list
		for (curr = start->n.other ; curr  ; curr=next) {
			next = curr->n.other;
			sp_nodepath_node_destroy (curr);
		}

	}
	//###########################################
	//# END EDITS
	//###########################################

	//clean up the nodepath (such as for trivial subpaths)
	sp_nodepath_cleanup(nodepath);

	sp_nodepath_ensure_ctrls (nodepath);

	update_repr (nodepath);

	// if the entire nodepath is removed, delete the selected object.
	if (nodepath->subpaths == NULL ||
		sp_nodepath_get_node_count(nodepath) < 2) {
			sp_nodepath_destroy (nodepath);
			sp_selection_delete (NULL, NULL);
			return;
	}

	sp_nodepath_update_statusbar (nodepath);
}


void
sp_node_selected_set_line_type (ArtPathcode code)
{
	SPNodePath *nodepath = sp_nodepath_current();
	if (nodepath == NULL) return;

	for (GList *l = nodepath->selected; l != NULL; l = l->next) {
		SPPathNode *n = (SPPathNode *) l->data;
		g_assert (n->selected);
		if (n->p.other && n->p.other->selected) {
			sp_nodepath_set_line_type (n, code);
		}
	}

	update_repr (nodepath);
}

void
sp_node_selected_set_type (SPPathNodeType type)
{
	/* fixme: do it the right way */
    /* What is the right way?  njh */
	SPNodePath *nodepath = sp_nodepath_current ();
	if (nodepath == NULL) return;

	for (GList *l = nodepath->selected; l != NULL; l = l->next) {
		sp_nodepath_set_node_type ((SPPathNode *) l->data, type);
	}

	update_repr (nodepath);
}

static void sp_node_set_selected(SPPathNode *node, gboolean selected)
{
	node->selected = selected;

	if (selected) {
		g_object_set (G_OBJECT (node->knot),
			      "fill", NODE_FILL_SEL,
			      "fill_mouseover", NODE_FILL_SEL_HI,
			      "stroke", NODE_STROKE_SEL,
			      "stroke_mouseover", NODE_STROKE_SEL_HI,
			      NULL);
	} else {
		g_object_set (G_OBJECT (node->knot),
			      "fill", NODE_FILL,
			      "fill_mouseover", NODE_FILL_HI,
			      "stroke", NODE_STROKE,
			      "stroke_mouseover", NODE_STROKE_HI,
			      NULL);
	}

	sp_node_ensure_ctrls (node);
	if (node->n.other) sp_node_ensure_ctrls (node->n.other);
	if (node->p.other) sp_node_ensure_ctrls (node->p.other);
}

/**
\brief select a node
\param node     the node to select
\param incremental   if true, add to selection, otherwise deselect others
\param override   if true, always select this node, otherwise toggle selected status
*/
static void sp_nodepath_node_select(SPPathNode *node, gboolean incremental, gboolean override)
{
	SPNodePath *nodepath = node->subpath->nodepath;

	if (incremental) {
		if (override) {
			if (!g_list_find (nodepath->selected, node)) {
				nodepath->selected = g_list_append (nodepath->selected, node);
			}
			sp_node_set_selected (node, TRUE);
		} else { // toggle
			if (node->selected) {
				g_assert (g_list_find (nodepath->selected, node));
				nodepath->selected = g_list_remove (nodepath->selected, node);
			} else {
				g_assert (!g_list_find (nodepath->selected, node));
				nodepath->selected = g_list_append (nodepath->selected, node);
			}
			sp_node_set_selected (node, !node->selected);
		}
	} else {
		sp_nodepath_deselect (nodepath);
		nodepath->selected = g_list_append (nodepath->selected, node);
		sp_node_set_selected (node, TRUE);
	}

	sp_nodepath_update_statusbar (nodepath);
}


/**
\brief deselect all nodes in the nodepath
*/
void
sp_nodepath_deselect (SPNodePath *nodepath)
{
	if (!nodepath) return; // there's no nodepath when editing rects, stars, spirals or ellipses

	while (nodepath->selected) {
		sp_node_set_selected ((SPPathNode *) nodepath->selected->data, FALSE);
		nodepath->selected = g_list_remove (nodepath->selected, nodepath->selected->data);
	}
	sp_nodepath_update_statusbar (nodepath);
}

/**
\brief select all nodes in the nodepath
*/
void
sp_nodepath_select_all (SPNodePath *nodepath)
{
	if (!nodepath) return;

	for (GList *spl = nodepath->subpaths; spl != NULL; spl = spl->next) {
		SPNodeSubPath *subpath = (SPNodeSubPath *) spl->data;
		for (GList *nl = subpath->nodes; nl != NULL; nl = nl->next) {
			SPPathNode *node = (SPPathNode *) nl->data;
			sp_nodepath_node_select (node, TRUE, TRUE);
		}
	}
}

/**
\brief select the node after the last selected; if none is selected, select the first within path
*/
void sp_nodepath_select_next(SPNodePath *nodepath)
{
	if (!nodepath) return; // there's no nodepath when editing rects, stars, spirals or ellipses

	SPPathNode *last = NULL;
	if (nodepath->selected) {
		for (GList *spl = nodepath->subpaths; spl != NULL; spl = spl->next) {
			SPNodeSubPath *subpath, *subpath_next;
			subpath = (SPNodeSubPath *) spl->data;
			for (GList *nl = subpath->nodes; nl != NULL; nl = nl->next) {
				SPPathNode *node = (SPPathNode *) nl->data;
				if (node->selected) {
					if (node->n.other == (SPPathNode *) subpath->last) {
						if (node->n.other == (SPPathNode *) subpath->first) { // closed subpath 
							if (spl->next) { // there's a next subpath
								subpath_next = (SPNodeSubPath *) spl->next->data;
								last = subpath_next->first;
							} else if (spl->prev) { // there's a previous subpath
								last = NULL; // to be set later to the first node of first subpath
							} else {
								last = node->n.other;
							}
						} else {
							last = node->n.other;
						}
					} else {
						if (node->n.other) {
							last = node->n.other;
						} else {
							if (spl->next) { // there's a next subpath
								subpath_next = (SPNodeSubPath *) spl->next->data;
								last = subpath_next->first;
							} else if (spl->prev) { // there's a previous subpath
								last = NULL; // to be set later to the first node of first subpath
							} else {
								last = (SPPathNode *) subpath->first;
							}
						}
					}
				}
			}
		}
		sp_nodepath_deselect (nodepath);
	}

	if (last) { // there's at least one more node after selected
		sp_nodepath_node_select ((SPPathNode *) last, TRUE, TRUE);
	} else { // no more nodes, select the first one in first subpath
		SPNodeSubPath *subpath = (SPNodeSubPath *) nodepath->subpaths->data;
		sp_nodepath_node_select ((SPPathNode *) subpath->first, TRUE, TRUE);
	}
}

/**
\brief select the node before the first selected; if none is selected, select the last within path
*/
void sp_nodepath_select_prev(SPNodePath *nodepath)
{
	if (!nodepath) return; // there's no nodepath when editing rects, stars, spirals or ellipses

	SPPathNode *last = NULL;
	if (nodepath->selected) {
		for (GList *spl = g_list_last(nodepath->subpaths); spl != NULL; spl = spl->prev) {
			SPNodeSubPath *subpath = (SPNodeSubPath *) spl->data;
			for (GList *nl = g_list_last(subpath->nodes); nl != NULL; nl = nl->prev) {
				SPPathNode *node = (SPPathNode *) nl->data;
				if (node->selected) {
					if (node->p.other == (SPPathNode *) subpath->first) {
						if (node->p.other == (SPPathNode *) subpath->last) { // closed subpath 
							if (spl->prev) { // there's a prev subpath
								SPNodeSubPath *subpath_prev = (SPNodeSubPath *) spl->prev->data;
								last = subpath_prev->last;
							} else if (spl->next) { // there's a next subpath
								last = NULL; // to be set later to the last node of last subpath
							} else {
								last = node->p.other;
							}
						} else {
							last = node->p.other;
						}
					} else {
						if (node->p.other) {
							last = node->p.other;
						} else {
							if (spl->prev) { // there's a prev subpath
								SPNodeSubPath *subpath_prev = (SPNodeSubPath *) spl->prev->data;
								last = subpath_prev->last;
							} else if (spl->next) { // there's a next subpath
								last = NULL; // to be set later to the last node of last subpath
							} else {
								last = (SPPathNode *) subpath->last;
							}
						}
					}
				}
			}
		}
		sp_nodepath_deselect (nodepath);
	}

	if (last) { // there's at least one more node before selected
		sp_nodepath_node_select ((SPPathNode *) last, TRUE, TRUE);
	} else { // no more nodes, select the last one in last subpath
		GList *spl = g_list_last(nodepath->subpaths);
		SPNodeSubPath *subpath = (SPNodeSubPath *) spl->data;
		sp_nodepath_node_select ((SPPathNode *) subpath->last, TRUE, TRUE);
	}
}

/**
\brief select all nodes that are within the rectangle
*/
void sp_nodepath_select_rect(SPNodePath *nodepath, NRRect *b, gboolean incremental)
{
	if (!incremental) {
		sp_nodepath_deselect (nodepath);
	}

	for (GList *spl = nodepath->subpaths; spl != NULL; spl = spl->next) {
		SPNodeSubPath *subpath = (SPNodeSubPath *) spl->data;
		for (GList *nl = subpath->nodes; nl != NULL; nl = nl->next) {
			SPPathNode *node = (SPPathNode *) nl->data;

			NR::Point p = node->pos;

			if ((p[NR::X] > b->x0) && (p[NR::X] < b->x1) && (p[NR::Y] > b->y0) && (p[NR::Y] < b->y1)) {
				sp_nodepath_node_select (node, TRUE, FALSE);
			}
		}
	}
}

/**
\brief  Saves selected nodes in a nodepath into a list containing integer positions of all selected nodes
*/
GList *save_nodepath_selection (SPNodePath *nodepath)
{
	if (!nodepath->selected) {
		return NULL;
	}

	GList *r = NULL;
	guint i = 0;
	for (GList *spl = nodepath->subpaths; spl != NULL; spl = spl->next) {
		SPNodeSubPath *subpath = (SPNodeSubPath *) spl->data;
		for (GList *nl = subpath->nodes; nl != NULL; nl = nl->next) {
			SPPathNode *node = (SPPathNode *) nl->data;
			i++;
			if (node->selected) {
				r = g_list_append (r, GINT_TO_POINTER (i));
			}
		}
	}
	return r;
}

/**
\brief  Restores selection by selecting nodes whose positions are in the list
*/
void restore_nodepath_selection (SPNodePath *nodepath, GList *r)
{
	sp_nodepath_deselect (nodepath);

	guint i = 0;
	for (GList *spl = nodepath->subpaths; spl != NULL; spl = spl->next) {
		SPNodeSubPath *subpath = (SPNodeSubPath *) spl->data;
		for (GList *nl = subpath->nodes; nl != NULL; nl = nl->next) {
			SPPathNode *node = (SPPathNode *) nl->data;
			i++;
			if (g_list_find (r, GINT_TO_POINTER (i))) {
				sp_nodepath_node_select (node, TRUE, TRUE);
			}
		}
	}

}

/**
\brief adjusts control point according to node type and line code
*/
static void sp_node_adjust_knot(SPPathNode *node, gint which_adjust)
{
	double len, otherlen, linelen;

	g_assert (node);

	SPPathNodeSide *me = sp_node_get_side (node, which_adjust);
	SPPathNodeSide *other = sp_node_opposite_side (node, me);

	/* fixme: */
	if (me->other == NULL) return;
	if (other->other == NULL) return;

	/* I have line */

	ArtPathcode mecode, ocode;
	if (which_adjust == 1) {
		mecode = (ArtPathcode)me->other->code;
		ocode = (ArtPathcode)node->code;
	} else {
		mecode = (ArtPathcode)node->code;
		ocode = (ArtPathcode)other->other->code;
	}

	if (mecode == ART_LINETO) return;

	/* I am curve */

	if (other->other == NULL) return;

	/* Other has line */

	if (node->type == SP_PATHNODE_CUSP) return;
	
	NR::Point delta;
	if (ocode == ART_LINETO) {
		/* other is lineto, we are either smooth or symm */
		SPPathNode *othernode = other->other;
		len = NR::L2(me->pos - node->pos);
		delta = node->pos - othernode->pos;
		linelen = NR::L2(delta);
		if (linelen < 1e-18) return;

		me->pos = node->pos + (len / linelen)*delta;
		sp_knot_set_position (me->knot, &me->pos, 0);

		sp_node_ensure_ctrls (node);
		return;
	}

	if (node->type == SP_PATHNODE_SYMM) {

		me->pos = 2 * node->pos - other->pos;
		sp_knot_set_position (me->knot, &me->pos, 0);

		sp_node_ensure_ctrls (node);
		return;
	}

	/* We are smooth */

	len = NR::L2 (me->pos - node->pos);
	delta = other->pos - node->pos;
	otherlen = NR::L2 (delta);
	if (otherlen < 1e-18) return;

	me->pos = node->pos - (len / otherlen) * delta;
	sp_knot_set_position (me->knot, &me->pos, 0);

	sp_node_ensure_ctrls (node);
}

/**
 \brief Adjusts control point according to node type and line code
 */
static void sp_node_adjust_knots(SPPathNode *node)
{
	g_assert (node);

	if (node->type == SP_PATHNODE_CUSP) return;

	/* we are either smooth or symm */

	if (node->p.other == NULL) return;

	if (node->n.other == NULL) return;

	if (node->code == ART_LINETO) {
		if (node->n.other->code == ART_LINETO) return;
		sp_node_adjust_knot (node, 1);
		sp_node_ensure_ctrls (node);
		return;
	}

	if (node->n.other->code == ART_LINETO) {
		if (node->code == ART_LINETO) return;
		sp_node_adjust_knot (node, -1);
		sp_node_ensure_ctrls (node);
		return;
	}

	/* both are curves */

	const NR::Point delta  = node->n.pos - node->p.pos;

	if (node->type == SP_PATHNODE_SYMM) {
		node->p.pos = node->pos - delta / 2;
		node->n.pos = node->pos + delta / 2;
		sp_node_ensure_ctrls (node);
		return;
	}

	/* We are smooth */

	double plen = NR::L2(node->p.pos - node->pos);
	if (plen < 1e-18) return;
	double nlen = NR::L2(node->n.pos - node->pos);
	if (nlen < 1e-18) return;
	node->p.pos = node->pos - (plen / (plen + nlen)) * delta;
	node->n.pos = node->pos + (nlen / (plen + nlen)) * delta;
	sp_node_ensure_ctrls (node);
}

/*
 * Knot events
 */
static gboolean node_event(SPKnot *knot, GdkEvent *event, SPPathNode *n)
{
	gboolean ret = FALSE;
	switch (event->type) {
	case GDK_ENTER_NOTIFY:
		active_node = n;
		break;
	case GDK_LEAVE_NOTIFY:
		active_node = NULL;
		break;
	case GDK_KEY_PRESS:
		switch (event->key.keyval) {
		case GDK_space:
			if (event->key.state & GDK_BUTTON1_MASK) {
				SPNodePath *nodepath = n->subpath->nodepath;
				stamp_repr(nodepath);
				ret = TRUE;
			}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return ret;
}

gboolean node_key(GdkEvent *event)
{
	SPNodePath *np;

	// there is no way to verify nodes so set active_node to nil when deleting!!
	if (active_node == NULL) return FALSE;

	if ((event->type == GDK_KEY_PRESS) && !(event->key.state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK))) {
		gint ret = FALSE;
		switch (event->key.keyval) {
		case GDK_BackSpace:
			np = active_node->subpath->nodepath;
			sp_nodepath_node_destroy (active_node);
			update_repr (np);
			active_node = NULL;
			ret = TRUE;
			break;
		case GDK_c:
			sp_nodepath_set_node_type (active_node, SP_PATHNODE_CUSP);
			ret = TRUE;
			break;
		case GDK_s:
			sp_nodepath_set_node_type (active_node, SP_PATHNODE_SMOOTH);
			ret = TRUE;
			break;
		case GDK_y:
			sp_nodepath_set_node_type (active_node, SP_PATHNODE_SYMM);
			ret = TRUE;
			break;
		case GDK_b:
			sp_nodepath_node_break (active_node);
			ret = TRUE;
			break;
		}
		return ret;
	}
	return FALSE;
}

static void node_clicked(SPKnot *knot, guint state, gpointer data)
{
	SPPathNode *n = (SPPathNode *) data;

	if (state & GDK_CONTROL_MASK) {
		if (!(state & GDK_MOD1_MASK)) { // ctrl+click: toggle node type
			if (n->type == SP_PATHNODE_CUSP) {
				sp_nodepath_set_node_type (n, SP_PATHNODE_SMOOTH);
			} else if (n->type == SP_PATHNODE_SMOOTH) {
				sp_nodepath_set_node_type (n, SP_PATHNODE_SYMM);
			} else {
				sp_nodepath_set_node_type (n, SP_PATHNODE_CUSP);
			}
		} else { //ctrl+alt+click: delete node
			SPNodePath *nodepath = n->subpath->nodepath;
			sp_nodepath_node_destroy (n);
			if (nodepath->subpaths == NULL) { // if the entire nodepath is removed, delete the selected object.
				sp_nodepath_destroy (nodepath);
				sp_selection_delete (NULL, NULL);
			} else {
				sp_nodepath_ensure_ctrls (nodepath);
				update_repr (nodepath);
				sp_nodepath_update_statusbar (nodepath);
			}
		}
	} else {
		sp_nodepath_node_select (n, (state & GDK_SHIFT_MASK), FALSE);
	}
}

static void node_grabbed(SPKnot *knot, guint state, gpointer data)
{
	SPPathNode *n = (SPPathNode *) data;

	n->origin = knot->pos;

	if (!n->selected) {
		sp_nodepath_node_select (n, (state & GDK_SHIFT_MASK), FALSE);
	}
}

static void node_ungrabbed(SPKnot *knot, guint state, gpointer data)
{
	SPPathNode *n = (SPPathNode *) data;

	update_repr (n->subpath->nodepath);
}

Radial::Radial(NR::Point const &p)
{
	r = NR::L2(p);
	if (r > 0) {
		a = NR::atan2 (p);
	} else {
		a = HUGE_VAL; //undefined
	}
}

Radial::operator NR::Point() const
{
	if (a == HUGE_VAL) {
		return NR::Point(0,0);
	} else {
		return r*NR::Point(cos(a), sin(a));
	}
}

/**
\brief The point on a line, given by its angle, closest to the given point
\param p   point
\param a   angle of the line; it is assumed to go through coordinate origin
\param closest   pointer to the point struct where the result is stored
*/
// FIXME: use dot product perhaps?
static void point_line_closest(NR::Point *p, double a, NR::Point *closest)
{
	if (a == HUGE_VAL) { // vertical
		*closest = NR::Point(0, (*p)[NR::Y]);
	} else {
		//		*closest = NR::Point(( ( a * (*p)[NR::Y] + (*p)[NR::X]) / (a*a + 1) ),
		//				     a * (*closest)[NR::X]);
		(*closest)[NR::X] = ( a * (*p)[NR::Y] + (*p)[NR::X]) / (a*a + 1);
		(*closest)[NR::Y] = a * (*closest)[NR::X];
	}
}

/**
\brief Distance from the point to a line given by its angle
\param p   point
\param a   angle of the line; it is assumed to go through coordinate origin
*/
static double point_line_distance(NR::Point *p, double a)
{
	NR::Point c;
	point_line_closest (p, a, &c);
	return sqrt (((*p)[NR::X] - c[NR::X])*((*p)[NR::X] - c[NR::X]) + ((*p)[NR::Y] - c[NR::Y])*((*p)[NR::Y] - c[NR::Y]));
}


/* fixme: This goes to "moved" event? (lauris) */
static gboolean
node_request (SPKnot *knot, NR::Point *p, guint state, gpointer data)
{
	double yn, xn, yp, xp;
	double an, ap, na, pa;
	double d_an, d_ap, d_na, d_pa;
	gboolean collinear = FALSE;
	NR::Point c;
	NR::Point pr;

	SPPathNode *n = (SPPathNode *) data;

	if (state & GDK_CONTROL_MASK) { // constrained motion 

		// calculate relative distances of control points
		yn = n->n.pos[NR::Y] - n->pos[NR::Y]; 
		xn = n->n.pos[NR::X] - n->pos[NR::X];
		if (xn < 0) { xn = -xn; yn = -yn; } // limit the handle angle to between 0 and pi
		if (yn < 0) { xn = -xn; yn = -yn; } 

		yp = n->p.pos[NR::Y] - n->pos[NR::Y];
		xp = n->p.pos[NR::X] - n->pos[NR::X];
		if (xp < 0) { xp = -xp; yp = -yp; } // limit the handle angle to between 0 and pi
		if (yp < 0) { xp = -xp; yp = -yp; } 

		if (state & GDK_MOD1_MASK && !(xn == 0 && xp == 0)) { 
			// sliding on handles, only if at least one of the handles is non-vertical

			// calculate angles of the control handles
			if (xn == 0) {
				if (yn == 0) { // no handle, consider it the continuation of the other one
					an = 0; 
					collinear = TRUE;
				} 
				else an = 0; // vertical; set the angle to horizontal
			} else an = yn/xn;

			if (xp == 0) {
				if (yp == 0) { // no handle, consider it the continuation of the other one
					ap = an; 
				}
				else ap = 0; // vertical; set the angle to horizontal
			} else  ap = yp/xp; 

			if (collinear) an = ap;

			// angles of the perpendiculars; HUGE_VAL means vertical
			if (an == 0) na = HUGE_VAL; else na = -1/an;
			if (ap == 0) pa = HUGE_VAL; else pa = -1/ap;

			//	g_print("an %g    ap %g\n", an, ap);

			// mouse point relative to the node's original pos
			pr = (*p) - n->origin;

			// distances to the four lines (two handles and two perpendiculars)
			d_an = point_line_distance(&pr, an);
			d_na = point_line_distance(&pr, na);
			d_ap = point_line_distance(&pr, ap);
			d_pa = point_line_distance(&pr, pa);

			// find out which line is the closest, save its closest point in c
			if (d_an <= d_na && d_an <= d_ap && d_an <= d_pa) {
				point_line_closest(&pr, an, &c);
			} else if (d_ap <= d_an && d_ap <= d_na && d_ap <= d_pa) {
				point_line_closest(&pr, ap, &c);
			} else if (d_na <= d_an && d_na <= d_ap && d_na <= d_pa) {
				point_line_closest(&pr, na, &c);
			} else if (d_pa <= d_an && d_pa <= d_ap && d_pa <= d_na) {
				point_line_closest(&pr, pa, &c);
			}

			// move the node to the closest point
			sp_nodepath_selected_nodes_move (n->subpath->nodepath, n->origin[NR::X] + c[NR::X] - n->pos[NR::X], n->origin[NR::Y] + c[NR::Y] - n->pos[NR::Y]);

		} else {  // constraining to hor/vert

			if (fabs((*p)[NR::X] - n->origin[NR::X]) > fabs((*p)[NR::Y] - n->origin[NR::Y])) { // snap to hor
				sp_nodepath_selected_nodes_move (n->subpath->nodepath, (*p)[NR::X] - n->pos[NR::X], n->origin[NR::Y] - n->pos[NR::Y]);
			} else { // snap to vert
				sp_nodepath_selected_nodes_move (n->subpath->nodepath, n->origin[NR::X] - n->pos[NR::X], (*p)[NR::Y] - n->pos[NR::Y]);
			}
		}
	} else { // move freely
		sp_nodepath_selected_nodes_move (n->subpath->nodepath, (*p)[NR::X] - n->pos[NR::X], (*p)[NR::Y] - n->pos[NR::Y]);
	}

	sp_desktop_scroll_to_point (n->subpath->nodepath->desktop, p);

	return TRUE;
}

static void node_ctrl_clicked(SPKnot *knot, guint state, gpointer data)
{
	SPPathNode *n = (SPPathNode *) data;

	sp_nodepath_node_select (n, (state & GDK_SHIFT_MASK), FALSE);
}

static void node_ctrl_grabbed(SPKnot *knot, guint state, gpointer data)
{
	SPPathNode *n = (SPPathNode *) data;

	if (!n->selected) {
		sp_nodepath_node_select (n, (state & GDK_SHIFT_MASK), FALSE);
	}

	// remember the origin of the control
	if (n->p.knot == knot) {
		n->p.origin = Radial(n->p.pos - n->pos);
	} else if (n->n.knot == knot) {
		n->n.origin = Radial(n->n.pos - n->pos);
	} else {
		g_assert_not_reached ();
	}

}

static void node_ctrl_ungrabbed(SPKnot *knot, guint state, gpointer data)
{
	SPPathNode *n = (SPPathNode *) data;

	// forget origin and set knot position once more (because it can be wrong now due to restrictions)
	if (n->p.knot == knot) {
		n->p.origin.a = 0;
		sp_knot_set_position (knot, &n->p.pos, state);
	} else if (n->n.knot == knot) {
		n->n.origin.a = 0;
		sp_knot_set_position (knot, &n->n.pos, state);
	} else {
		g_assert_not_reached ();
	}

	update_repr (n->subpath->nodepath);
}

static gboolean node_ctrl_request(SPKnot *knot, NR::Point *p, guint state, gpointer data)
{
	SPPathNode *n = (SPPathNode *) data;

	SPPathNodeSide *me, *opposite;
	gint which;
	if (n->p.knot == knot) {
		me = &n->p;
		opposite = &n->n;
		which = -1;
	} else if (n->n.knot == knot) {
		me = &n->n;
		opposite = &n->p;
		which = 1;
	} else {
		me = opposite = NULL;
		which = 0;
		g_assert_not_reached ();
	}

	ArtPathcode othercode = sp_node_path_code_from_side (n, opposite);

	if (opposite->other && (n->type != SP_PATHNODE_CUSP) && (othercode == ART_LINETO)) {
		gdouble len, linelen, scal;
		/* We are smooth node adjacent with line */
		NR::Point delta = *p - n->pos;
		len = NR::L2(delta);
		SPPathNode *othernode = opposite->other;
		NR::Point ndelta = n->pos - othernode->pos;
		linelen = NR::L2(ndelta);
		if ((len > 1e-18) && (linelen > 1e-18)) {
			scal = dot(delta, ndelta) / linelen;
			(*p) = n->pos + (scal / linelen) * ndelta;
		}
		sp_desktop_vector_snap (n->subpath->nodepath->desktop, *p, ndelta);
	} else {
		sp_desktop_free_snap (n->subpath->nodepath->desktop, *p);
	}

	sp_node_adjust_knot (n, -which);

	return FALSE;
}

static void node_ctrl_moved (SPKnot *knot, NR::Point *p, guint state, gpointer data)
{
	SPPathNode *n = (SPPathNode *) data;

	SPPathNodeSide *me;
	SPPathNodeSide *other;
	if (n->p.knot == knot) {
		me = &n->p;
		other = &n->n;
	} else if (n->n.knot == knot) {
		me = &n->n;
		other = &n->p;
	} else {
		me = NULL;
		other = NULL;
		g_assert_not_reached ();
	}

	// calculate radial coordinates of the grabbed control, other control, and the mouse point
	Radial rme(me->pos - n->pos);
	Radial rother(other->pos - n->pos);
	Radial rnew(*p - n->pos);

	if (state & GDK_CONTROL_MASK && rnew.a != HUGE_VAL) { 
		double a_snapped, a_ortho;

		int snaps = prefs_get_int_attribute ("options.rotationsnapsperpi", "value", 12);
		/* 0 interpreted as "no snapping". */

		// the closest PI/snaps angle, starting from zero
		a_snapped = floor (rnew.a/(M_PI/snaps) + 0.5) * (M_PI/snaps);
		// the closest PI/2 angle, starting from original angle (i.e. snapping to original, its opposite and perpendiculars)
		a_ortho = me->origin.a + floor ((rnew.a - me->origin.a)/(M_PI/2) + 0.5) * (M_PI/2);

		// snap to the closest
		if (fabs (a_snapped - rnew.a) < fabs (a_ortho - rnew.a))
			rnew.a = a_snapped;
		else 
			rnew.a = a_ortho;
	} 

	if (state & GDK_MOD1_MASK) { 
		// lock handle length
		rnew.r = me->origin.r;
	} 

	if ((state & GDK_SHIFT_MASK) && rme.a != HUGE_VAL && rnew.a != HUGE_VAL) { 
		// rotate the other handle correspondingly, if both old and new angles exist
		rother.a += rnew.a - rme.a;
		other->pos = NR::Point(rother) + n->pos;
		sp_ctrlline_set_coords (SP_CTRLLINE (other->line), n->pos, other->pos);
		sp_knot_set_position (other->knot, &other->pos, 0);
	} 

	me->pos = NR::Point(rnew) + n->pos;
	sp_ctrlline_set_coords (SP_CTRLLINE (me->line), n->pos, me->pos);

	// this is what sp_knot_set_position does, but without emitting the signal:
	// we cannot emit a "moved" signal because we're now processing it
	if (me->knot->item) sp_ctrl_moveto (SP_CTRL (me->knot->item), me->pos[NR::X], me->pos[NR::Y]);

	sp_desktop_set_coordinate_status (knot->desktop, me->pos[NR::X], me->pos[NR::Y], 0);

	update_object (n->subpath->nodepath);
}

static gboolean node_ctrl_event(SPKnot *knot, GdkEvent *event, SPPathNode *n)
{
	gboolean ret = FALSE;
	switch (event->type) {
	case GDK_KEY_PRESS:
		switch (event->key.keyval) {
		case GDK_space:
			if (event->key.state & GDK_BUTTON1_MASK) {
				SPNodePath *nodepath = n->subpath->nodepath;
				stamp_repr(nodepath);
				ret = TRUE;
			}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return ret;
}

static void node_rotate_internal(SPPathNode *n, gdouble angle, Radial &rme, Radial &rother, gboolean both)
{
	rme.a += angle; 
	if (both || n->type == SP_PATHNODE_SMOOTH || n->type == SP_PATHNODE_SYMM) 
		rother.a += angle;
}

static void node_rotate_internal_screen(SPPathNode *n, gdouble const angle, Radial &rme, Radial &rother, gboolean both)
{
	gdouble r;

	gdouble const norm_angle = angle / SP_DESKTOP_ZOOM (n->subpath->nodepath->desktop);

	if (both || n->type == SP_PATHNODE_SMOOTH || n->type == SP_PATHNODE_SYMM) 
		r = MAX (rme.r, rother.r);
	else 
		r = rme.r;

	gdouble const weird_angle = atan2 (norm_angle, r);
/* Bulia says norm_angle is just the visible distance that the
 * object's end must travel on the screen.  Left as 'angle' for want of
 * a better name.*/

	rme.a += weird_angle; 
	if (both || n->type == SP_PATHNODE_SMOOTH || n->type == SP_PATHNODE_SYMM)  
		rother.a += weird_angle;
}

static void node_rotate_common(SPPathNode *n, gdouble angle, int which, gboolean screen)
{
	SPPathNodeSide *me, *other;
	gboolean both = FALSE;

	if (which > 0) {
		me = &(n->n);
		other = &(n->p);
	} else if (which < 0){
		me = &(n->p);
		other = &(n->n);
	} else {
		me = &(n->n);
		other = &(n->p);
		both = TRUE;
	}

	Radial rme(me->pos - n->pos);
	Radial rother(other->pos - n->pos);

	if (screen) {
		node_rotate_internal_screen (n, angle, rme, rother, both);
	} else {
		node_rotate_internal (n, angle, rme, rother, both);
	}

	me->pos += NR::Point(rme);

	if (both || n->type == SP_PATHNODE_SMOOTH || n->type == SP_PATHNODE_SYMM) {
		other->pos = NR::Point(rother) + n->pos;
	}

	sp_node_ensure_ctrls (n);
}

void sp_nodepath_selected_nodes_rotate(SPNodePath *nodepath, gdouble angle, int which)
{
	if (!nodepath) return;

	for (GList *l = nodepath->selected; l != NULL; l = l->next) {
		SPPathNode *n = (SPPathNode *) l->data;
		node_rotate_common (n, angle, which, FALSE);
	}

	update_object (nodepath);
	// fixme: use _keyed
	update_repr (nodepath);
}

void sp_nodepath_selected_nodes_rotate_screen(SPNodePath *nodepath, gdouble angle, int which)
{
	if (!nodepath) return;

	for (GList *l = nodepath->selected; l != NULL; l = l->next) {
		SPPathNode *n = (SPPathNode *) l->data;
		node_rotate_common (n, angle, which, TRUE);
	}

	update_object (nodepath);
	// fixme: use _keyed
	update_repr (nodepath);
}

static void node_scale(SPPathNode *n, gdouble grow, int which)
{
	bool both = false;
	SPPathNodeSide *me, *other;
	if (which > 0) {
		me = &(n->n);
		other = &(n->p);
	} else if (which < 0){
		me = &(n->p);
		other = &(n->n);
	} else {
		me = &(n->n);
		other = &(n->p);
		both = true;
	}

	Radial rme(me->pos - n->pos);
	Radial rother(other->pos - n->pos);

	rme.r += grow; 
	if (rme.r < 0) rme.r = 1e-6; // not 0, so that direction is not lost
	if (rme.a == HUGE_VAL) {
		rme.a = 0; // if direction is unknown, initialize to 0
		sp_node_selected_set_line_type (ART_CURVETO);
	}
	if (both || n->type == SP_PATHNODE_SYMM) {
		rother.r += grow;
		if (rother.r < 0) rother.r = 1e-6;
		if (rother.r == HUGE_VAL) {
			rother.a = 0;
			sp_node_selected_set_line_type (ART_CURVETO);
		}
	}

	me->pos = NR::Point(rme) + n->pos;

	if (both || n->type == SP_PATHNODE_SYMM) {
		other->pos = NR::Point(rother) + n->pos;
	}

	sp_node_ensure_ctrls (n);
}

static void node_scale_screen(SPPathNode *n, gdouble const grow, int which)
{
	node_scale (n, grow / SP_DESKTOP_ZOOM (n->subpath->nodepath->desktop), which);
}

void sp_nodepath_selected_nodes_scale(SPNodePath *nodepath, gdouble const grow, int which)
{
	if (!nodepath) return;

	for (GList *l = nodepath->selected; l != NULL; l = l->next) {
		SPPathNode *n = (SPPathNode *) l->data;
		node_scale (n, grow, which);
	}

	update_object (nodepath);
	// fixme: use _keyed
	update_repr (nodepath);
}

void sp_nodepath_selected_nodes_scale_screen (SPNodePath *nodepath, gdouble const grow, int which)
{
	if (!nodepath) return;

	for (GList *l = nodepath->selected; l != NULL; l = l->next) {
		SPPathNode *n = (SPPathNode *) l->data;
		node_scale_screen (n, grow, which);
	}

	update_object (nodepath);
	// fixme: use _keyed
	update_repr (nodepath);
}


/*
 * Constructors and destructors
 */

static SPNodeSubPath *sp_nodepath_subpath_new(SPNodePath *nodepath)
{
	g_assert (nodepath);
	g_assert (nodepath->desktop);

	SPNodeSubPath *s = g_new (SPNodeSubPath, 1);

	s->nodepath = nodepath;
	s->closed = FALSE;
	s->nodes = NULL;
	s->first = NULL;
	s->last = NULL;

	nodepath->subpaths = g_list_prepend (nodepath->subpaths, s);

	return s;
}

static void sp_nodepath_subpath_destroy(SPNodeSubPath *subpath)
{
	g_assert (subpath);
	g_assert (subpath->nodepath);
	g_assert (g_list_find (subpath->nodepath->subpaths, subpath));

	while (subpath->nodes) {
		sp_nodepath_node_destroy ((SPPathNode *) subpath->nodes->data);
	}

	subpath->nodepath->subpaths = g_list_remove (subpath->nodepath->subpaths, subpath);

	g_free (subpath);
}

static void sp_nodepath_subpath_close(SPNodeSubPath *sp)
{
	g_assert (!sp->closed);
	g_assert (sp->last != sp->first);
	g_assert (sp->first->code == ART_MOVETO);

	sp->closed = TRUE;

    //Link the head to the tail
	sp->first->p.other = sp->last;
	sp->last->n.other  = sp->first;
	sp->last->n.pos    = sp->first->n.pos;
	sp->first          = sp->last;

    //Remove the extra end node
	sp_nodepath_node_destroy (sp->last->n.other);
}

static void sp_nodepath_subpath_open(SPNodeSubPath *sp, SPPathNode *n)
{
	g_assert (sp->closed);
	g_assert (n->subpath == sp);
	g_assert (sp->first == sp->last);

	/* We create new startpoint, current node will become last one */

	SPPathNode *new_path = sp_nodepath_node_new(sp, n->n.other, SP_PATHNODE_CUSP, ART_MOVETO,
						    &n->pos, &n->pos, &n->n.pos);


	sp->closed        = FALSE;

    //Unlink to make a head and tail
	sp->first         = new_path;
	sp->last          = n;
	n->n.other        = NULL;
	new_path->p.other = NULL;
}

SPPathNode *
sp_nodepath_node_new (SPNodeSubPath *sp, SPPathNode *next, SPPathNodeType type, ArtPathcode code,
		      NR::Point *ppos, NR::Point *pos, NR::Point *npos)
{
	g_assert (sp);
	g_assert (sp->nodepath);
	g_assert (sp->nodepath->desktop);

	if (nodechunk == NULL)
		nodechunk = g_mem_chunk_create (SPPathNode, 32, G_ALLOC_AND_FREE);

	SPPathNode *n = (SPPathNode*)g_mem_chunk_alloc (nodechunk);

	n->subpath  = sp;
	n->type     = type;
	n->code     = code;
	n->selected = FALSE;
	n->pos      = *pos;
	n->p.pos    = *ppos;
	n->n.pos    = *npos;

	SPPathNode *prev;
	if (next) {
		g_assert (g_list_find (sp->nodes, next));
		prev = next->p.other;
	} else {
		prev = sp->last;
	}

	if (prev)
		prev->n.other = n;
	else
		sp->first = n;

	if (next)
		next->p.other = n;
	else
		sp->last = n;

	n->p.other = prev;
	n->n.other = next;

	n->knot = sp_knot_new (sp->nodepath->desktop);
	sp_knot_set_position (n->knot, pos, 0);
	g_object_set (G_OBJECT (n->knot),
		      "anchor", GTK_ANCHOR_CENTER,
		      "fill", NODE_FILL,
		      "fill_mouseover", NODE_FILL_HI,
		      "stroke", NODE_STROKE,
		      "stroke_mouseover", NODE_STROKE_HI,
		      NULL);
	if (n->type == SP_PATHNODE_CUSP)
		g_object_set (G_OBJECT (n->knot), "shape", SP_KNOT_SHAPE_DIAMOND, "size", 9, NULL);
	else
		g_object_set (G_OBJECT (n->knot), "shape", SP_KNOT_SHAPE_SQUARE, "size", 7, NULL);

	g_signal_connect (G_OBJECT (n->knot), "event", G_CALLBACK (node_event), n);
	g_signal_connect (G_OBJECT (n->knot), "clicked", G_CALLBACK (node_clicked), n);
	g_signal_connect (G_OBJECT (n->knot), "grabbed", G_CALLBACK (node_grabbed), n);
	g_signal_connect (G_OBJECT (n->knot), "ungrabbed", G_CALLBACK (node_ungrabbed), n);
	g_signal_connect (G_OBJECT (n->knot), "request", G_CALLBACK (node_request), n);
	sp_knot_show (n->knot);

	n->p.knot = sp_knot_new (sp->nodepath->desktop);
	sp_knot_set_position (n->p.knot, ppos, 0);
	g_object_set (G_OBJECT (n->p.knot),
		      "shape", SP_KNOT_SHAPE_CIRCLE,
		      "size", 7,
		      "anchor", GTK_ANCHOR_CENTER,
		      "fill", KNOT_FILL,
		      "fill_mouseover", KNOT_FILL_HI,
		      "stroke", KNOT_STROKE,
		      "stroke_mouseover", KNOT_STROKE_HI,
		      NULL);
	g_signal_connect (G_OBJECT (n->p.knot), "clicked", G_CALLBACK (node_ctrl_clicked), n);
	g_signal_connect (G_OBJECT (n->p.knot), "grabbed", G_CALLBACK (node_ctrl_grabbed), n);
	g_signal_connect (G_OBJECT (n->p.knot), "ungrabbed", G_CALLBACK (node_ctrl_ungrabbed), n);
	g_signal_connect (G_OBJECT (n->p.knot), "request", G_CALLBACK (node_ctrl_request), n);
	g_signal_connect (G_OBJECT (n->p.knot), "moved", G_CALLBACK (node_ctrl_moved), n);
	g_signal_connect (G_OBJECT (n->p.knot), "event", G_CALLBACK (node_ctrl_event), n);
	
	sp_knot_hide (n->p.knot);
	n->p.line = sp_canvas_item_new (SP_DT_CONTROLS (n->subpath->nodepath->desktop),
					       SP_TYPE_CTRLLINE, NULL);
	sp_canvas_item_hide (n->p.line);

	n->n.knot = sp_knot_new (sp->nodepath->desktop);
	sp_knot_set_position (n->n.knot, npos, 0);
	g_object_set (G_OBJECT (n->n.knot),
		      "shape", SP_KNOT_SHAPE_CIRCLE,
		      "size", 7,
		      "anchor", GTK_ANCHOR_CENTER,
		      "fill", KNOT_FILL,
		      "fill_mouseover", KNOT_FILL_HI,
		      "stroke", KNOT_STROKE,
		      "stroke_mouseover", KNOT_STROKE_HI,
		      NULL);
	g_signal_connect (G_OBJECT (n->n.knot), "clicked", G_CALLBACK (node_ctrl_clicked), n);
	g_signal_connect (G_OBJECT (n->n.knot), "grabbed", G_CALLBACK (node_ctrl_grabbed), n);
	g_signal_connect (G_OBJECT (n->n.knot), "ungrabbed", G_CALLBACK (node_ctrl_ungrabbed), n);
	g_signal_connect (G_OBJECT (n->n.knot), "request", G_CALLBACK (node_ctrl_request), n);
	g_signal_connect (G_OBJECT (n->n.knot), "moved", G_CALLBACK (node_ctrl_moved), n);
	g_signal_connect (G_OBJECT (n->n.knot), "event", G_CALLBACK (node_ctrl_event), n);
	sp_knot_hide (n->n.knot);
	n->n.line = sp_canvas_item_new (SP_DT_CONTROLS (n->subpath->nodepath->desktop),
					       SP_TYPE_CTRLLINE, NULL);
	sp_canvas_item_hide (n->n.line);

	sp->nodes = g_list_prepend (sp->nodes, n);

	return n;
}

static void sp_nodepath_node_destroy (SPPathNode *node)
{
	g_assert (node);
	g_assert (node->subpath);
	g_assert (SP_IS_KNOT (node->knot));
	g_assert (SP_IS_KNOT (node->p.knot));
	g_assert (SP_IS_KNOT (node->n.knot));
	g_assert (g_list_find (node->subpath->nodes, node));

	SPNodeSubPath *sp = node->subpath;

	if (node->selected) { // first, deselect
		g_assert (g_list_find (node->subpath->nodepath->selected, node));
		node->subpath->nodepath->selected = g_list_remove (node->subpath->nodepath->selected, node);
	}

	node->subpath->nodes = g_list_remove (node->subpath->nodes, node);

	g_object_unref (G_OBJECT (node->knot));
	g_object_unref (G_OBJECT (node->p.knot));
	g_object_unref (G_OBJECT (node->n.knot));

	gtk_object_destroy (GTK_OBJECT (node->p.line));
	gtk_object_destroy (GTK_OBJECT (node->n.line));
	
	if (sp->nodes) { // there are others nodes on the subpath
		if (sp->closed) {
			if (sp->first == node) {
				g_assert (sp->last == node);
				sp->first = node->n.other;
				sp->last = sp->first;
			}
			node->p.other->n.other = node->n.other;
			node->n.other->p.other = node->p.other;
		} else {
			if (sp->first == node) {
				sp->first = node->n.other;
				sp->first->code = ART_MOVETO;
			}
			if (sp->last == node) sp->last = node->p.other;
			if (node->p.other) node->p.other->n.other = node->n.other;
			if (node->n.other) node->n.other->p.other = node->p.other;
		}
	} else { // this was the last node on subpath
		sp->nodepath->subpaths = g_list_remove (sp->nodepath->subpaths, sp);
	}

	g_mem_chunk_free (nodechunk, node);
}

/*
 * Helpers
 */

static SPPathNodeSide *sp_node_get_side(SPPathNode *node, gint which)
{
	g_assert (node);

	switch (which) {
	case -1:
		return &node->p;
	case 1:
		return &node->n;
	default:
		break;
	}

	g_assert_not_reached ();

	return NULL;
}

static SPPathNodeSide *sp_node_opposite_side(SPPathNode *node, SPPathNodeSide *me)
{
	g_assert (node);

	if (me == &node->p) return &node->n;
	if (me == &node->n) return &node->p;

	g_assert_not_reached ();

	return NULL;
}

static ArtPathcode sp_node_path_code_from_side(SPPathNode *node, SPPathNodeSide *me)
{
	g_assert (node);

	if (me == &node->p) {
		if (node->p.other) return (ArtPathcode)node->code;
		return ART_MOVETO;
	}

	if (me == &node->n) {
		if (node->n.other) return (ArtPathcode)node->n.other->code;
		return ART_MOVETO;
	}

	g_assert_not_reached ();

	return ART_END;
}

static gchar const *sp_node_type_description(SPPathNode *n)
{
	switch (n->type) {
	case SP_PATHNODE_CUSP:
		return _("cusp");
	case SP_PATHNODE_SMOOTH:
		return _("smooth");
	case SP_PATHNODE_SYMM:
		return _("symmetric");
	}
	return NULL;
}

void
sp_nodepath_update_statusbar (SPNodePath *nodepath)
{
	if (!nodepath) return;

	const gchar* when_selected = _("Drag nodes or control points to edit the path");

	gint total = 0;

	for (GList *spl = nodepath->subpaths; spl != NULL; spl = spl->next) {
		SPNodeSubPath *subpath = (SPNodeSubPath *) spl->data;
		total += g_list_length (subpath->nodes);
	}

	gint selected = g_list_length (nodepath->selected);

	if (selected == 0) {
		SPSelection *sel = nodepath->desktop->selection;
		if (!sel || !sel->items)
			sp_view_set_statusf (SP_VIEW(nodepath->desktop), _("Select one path object with selector first, then switch back to node tool."));
		else 
			sp_view_set_statusf (SP_VIEW(nodepath->desktop), _("0 out of %i nodes selected. Click, Shift+click, drag around nodes to select."), total);
	} else if (selected == 1) {
		sp_view_set_statusf (SP_VIEW(nodepath->desktop), _("%i of %i nodes selected; %s. %s."), selected, total, sp_node_type_description ((SPPathNode *) nodepath->selected->data), when_selected);
	} else {
		sp_view_set_statusf (SP_VIEW(nodepath->desktop), _("%i of %i nodes selected. %s."), selected, total, when_selected);
	}
}
