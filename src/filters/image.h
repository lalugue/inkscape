/** @file
 * @brief SVG image filter effect
 *//*
 * Authors:
 *   Felipe Corrêa da Silva Sanches <juca@members.fsf.org>
 *   Hugo Rodrigues <haa.rodrigues@gmail.com>
 *
 * Copyright (C) 2006 Hugo Rodrigues
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifndef SP_FEIMAGE_H_SEEN
#define SP_FEIMAGE_H_SEEN

#include "sp-filter-primitive.h"
#include "svg/svg-length.h"
#include "sp-item.h"
#include "uri-references.h"

#define SP_TYPE_FEIMAGE (sp_feImage_get_type())
#define SP_FEIMAGE(obj) ((SPFeImage*)obj)
#define SP_IS_FEIMAGE(obj) (obj != NULL && static_cast<const SPObject*>(obj)->typeHierarchy.count(typeid(SPFeImage)))

class CFeImage;

class SPFeImage : public SPFilterPrimitive {
public:
	SPFeImage();
	CFeImage* cfeimage;

    gchar *href;

    /* preserveAspectRatio */
    unsigned int aspect_align : 4;
    unsigned int aspect_clip : 1;

    SPDocument *document;
    bool from_element;
    SPItem* SVGElem;
    Inkscape::URIReference* SVGElemRef;
    sigc::connection _image_modified_connection;
    sigc::connection _href_modified_connection;
};

struct SPFeImageClass {
    SPFilterPrimitiveClass parent_class;
};

class CFeImage : public CFilterPrimitive {
public:
	CFeImage(SPFeImage* image);
	virtual ~CFeImage();

	virtual void build(SPDocument* doc, Inkscape::XML::Node* repr);
	virtual void release();

	virtual void set(unsigned int key, const gchar* value);

	virtual void update(SPCtx* ctx, unsigned int flags);

	virtual Inkscape::XML::Node* write(Inkscape::XML::Document* doc, Inkscape::XML::Node* repr, guint flags);

	virtual void build_renderer(Inkscape::Filters::Filter* filter);

private:
	SPFeImage* spfeimage;
};

GType sp_feImage_get_type();

#endif /* !SP_FEIMAGE_H_SEEN */

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
