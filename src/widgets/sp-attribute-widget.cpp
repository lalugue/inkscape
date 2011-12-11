/* Authors:
 *  Lauris Kaplinski <lauris@ximian.com>
 *   Abhishek Sharma
 *
 * Copyright (C) 2001 Ximian, Inc.
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <gtk/gtk.h>
#include "xml/repr.h"
#include "macros.h"
#include "document.h"
#include "sp-object.h"
#include <glibmm/i18n.h>

#include <sigc++/functors/ptr_fun.h>
#include <sigc++/adaptors/bind.h>

#include "sp-attribute-widget.h"

using Inkscape::DocumentUndo;

static void sp_attribute_widget_object_modified ( SPObject *object,
                                                  guint flags,
                                                  SPAttributeWidget *spaw );
static void sp_attribute_widget_object_release ( SPObject *object,
                                                 SPAttributeWidget *spaw );


SPAttributeWidget::SPAttributeWidget () : 
    blocked(0),
    hasobj(0),
    _attribute(),
    modified_connection(),
    release_connection()
{
    src.object = NULL;
}

SPAttributeWidget::~SPAttributeWidget ()
{
    if (hasobj)
    {
        if (src.object)
        {
            modified_connection.disconnect();
            release_connection.disconnect();
            src.object = NULL;
        }
    }
    else
    {
        if (src.repr)
        {
            src.repr = Inkscape::GC::release(src.repr);
        }
    }
}

void SPAttributeWidget::set_object(SPObject *object, const gchar *attribute)
{
    if (hasobj) {
        if (src.object) {
            modified_connection.disconnect();
            release_connection.disconnect();
            src.object = NULL;
        }
    } else {

        if (src.repr) {
            src.repr = Inkscape::GC::release(src.repr);
        }
    }

    hasobj = true;
    
    if (object) {
        const gchar *val;

        blocked = true;
        src.object = object;

        modified_connection = object->connectModified(sigc::bind<2>(sigc::ptr_fun(&sp_attribute_widget_object_modified), this));
        release_connection = object->connectRelease(sigc::bind<1>(sigc::ptr_fun(&sp_attribute_widget_object_release), this));

        _attribute = attribute;

        val = object->getRepr()->attribute(attribute);
        set_text (val ? val : (const gchar *) "");
        blocked = false;
    }
    gtk_widget_set_sensitive (GTK_WIDGET(this), (src.object != NULL));
}

void SPAttributeWidget::set_repr(Inkscape::XML::Node *repr, const gchar *attribute)
{
    if (hasobj) {
        if (src.object) {
            modified_connection.disconnect();
            release_connection.disconnect();
            src.object = NULL;
        }
    } else {

        if (src.repr) {
            src.repr = Inkscape::GC::release(src.repr);
        }
    }

    hasobj = false;
    
    if (repr) {
        const gchar *val;

        blocked = true;
        src.repr = Inkscape::GC::anchor(repr);
        attribute = g_strdup (attribute);

        val = repr->attribute(attribute);
        set_text (val ? val : (const gchar *) "");
        blocked = false;
    }
    gtk_widget_set_sensitive (GTK_WIDGET (this), (src.repr != NULL));
}

void SPAttributeWidget::on_changed (void)
{
    if (!blocked)
    {
        Glib::ustring text1;
        const gchar *text;
        blocked = TRUE;
        text1 = get_text ();
		text=text1.c_str();
        if (!*text)
            text = NULL;

        if (hasobj && src.object) {        
            src.object->getRepr()->setAttribute(_attribute.c_str(), text, false);
            DocumentUndo::done(src.object->document, SP_VERB_NONE,
                                _("Set attribute"));

        } else if (src.repr) {
            src.repr->setAttribute(_attribute.c_str(), text, false);
            /* TODO: Warning! Undo will not be flushed in given case */
        }
        blocked = false;
    }
}

static void sp_attribute_widget_object_modified ( SPObject */*object*/,
                                      guint flags,
                                      SPAttributeWidget *spaw )
{

    if (flags && SP_OBJECT_MODIFIED_FLAG) {

        const gchar *val, *text;
        val = spaw->src.object->getRepr()->attribute(spaw->get_attribute().c_str());
        text = gtk_entry_get_text (GTK_ENTRY (spaw));

        if (val || text) {

            if (!val || !text || strcmp (val, text)) {
                /* We are different */
                spaw->set_blocked(true);
                gtk_entry_set_text ( GTK_ENTRY (spaw),
                                     val ? val : (const gchar *) "");
                spaw->set_blocked(false);
            } // end of if()

        } // end of if()

    } //end of if()

} // end of sp_attribute_widget_object_modified()

static void sp_attribute_widget_object_release ( SPObject */*object*/,
                                     SPAttributeWidget * spaw )
{
	spaw->set_object (NULL, NULL);
}



/* SPAttributeTable */

static void sp_attribute_table_class_init (SPAttributeTableClass *klass);
static void sp_attribute_table_init (SPAttributeTable *widget);
static void sp_attribute_table_destroy (GtkObject *object);

static void sp_attribute_table_object_modified (SPObject *object, guint flags, SPAttributeTable *spaw);
static void sp_attribute_table_object_release (SPObject *object, SPAttributeTable *spaw);
static void sp_attribute_table_entry_changed (GtkEditable *editable, SPAttributeTable *spat);

static GtkVBoxClass *table_parent_class;




GType sp_attribute_table_get_type(void)
{
    static GType type = 0;
    if (!type) {
        GTypeInfo info = {
            sizeof(SPAttributeTableClass),
            0, // base_init
            0, // base_finalize
            (GClassInitFunc)sp_attribute_table_class_init,
            0, // class_finalize
            0, // class_data
            sizeof(SPAttributeTable),
            0, // n_preallocs
            (GInstanceInitFunc)sp_attribute_table_init,
            0 // value_table
        };
        type = g_type_register_static(GTK_TYPE_VBOX, "SPAttributeTable", &info, static_cast<GTypeFlags>(0));
    }
    return type;
} // end of sp_attribute_table_get_type()



static void sp_attribute_table_class_init (SPAttributeTableClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

    table_parent_class = (GtkVBoxClass*)g_type_class_peek_parent (klass);

    object_class->destroy = sp_attribute_table_destroy;

} // end of sp_attribute_table_class_init()



static void sp_attribute_table_init ( SPAttributeTable *spat )
{
    spat->blocked = FALSE;
    spat->hasobj = FALSE;
    spat->table = NULL;
    spat->src.object = NULL;
    spat->num_attr = 0;
    spat->attributes = NULL;
    spat->entries = NULL;

    new (&spat->modified_connection) sigc::connection();
    new (&spat->release_connection) sigc::connection();
}

static void sp_attribute_table_destroy ( GtkObject *object )
{
    SPAttributeTable *spat;

    spat = SP_ATTRIBUTE_TABLE (object);

    if (spat->attributes) {
        gint i;
        for (i = 0; i < spat->num_attr; i++) {
            g_free (spat->attributes[i]);
        }
        g_free (spat->attributes);
        spat->attributes = NULL;
    }

    if (spat->hasobj) {

        if (spat->src.object) {
            spat->modified_connection.disconnect();
            spat->release_connection.disconnect();
            spat->src.object = NULL;
        }
    } else {
        if (spat->src.repr) {
            spat->src.repr = Inkscape::GC::release(spat->src.repr);
        }
    } // end of if()

    spat->modified_connection.~connection();
    spat->release_connection.~connection();

    if (spat->entries) {
        g_free (spat->entries);
        spat->entries = NULL;
    }

    spat->table = NULL;

    if (((GtkObjectClass *) table_parent_class)->destroy) {
        (* ((GtkObjectClass *) table_parent_class)->destroy) (object);
    }

} // end of sp_attribute_table_destroy()


GtkWidget * sp_attribute_table_new ( SPObject *object,
                         gint num_attr,
                         const gchar **labels,
                         const gchar **attributes )
{
    SPAttributeTable *spat;

    g_return_val_if_fail (!object || SP_IS_OBJECT (object), NULL);
    g_return_val_if_fail (!object || (num_attr > 0), NULL);
    g_return_val_if_fail (!num_attr || (labels && attributes), NULL);

    spat = (SPAttributeTable*)g_object_new (SP_TYPE_ATTRIBUTE_TABLE, NULL);

    sp_attribute_table_set_object (spat, object, num_attr, labels, attributes);

    return GTK_WIDGET (spat);

} // end of sp_attribute_table_new()



GtkWidget *sp_attribute_table_new_repr ( Inkscape::XML::Node *repr,
                              gint num_attr,
                              const gchar **labels,
                              const gchar **attributes )
{
    SPAttributeTable *spat;

    g_return_val_if_fail (!num_attr || (labels && attributes), NULL);

    spat = (SPAttributeTable*)g_object_new (SP_TYPE_ATTRIBUTE_TABLE, NULL);

    sp_attribute_table_set_repr (spat, repr, num_attr, labels, attributes);

    return GTK_WIDGET (spat);

} // end of sp_attribute_table_new_repr()



#define XPAD 4
#define YPAD 0

void sp_attribute_table_set_object ( SPAttributeTable *spat,
                                SPObject *object,
                                gint num_attr,
                                const gchar **labels,
                                const gchar **attributes )
{

    g_return_if_fail (spat != NULL);
    g_return_if_fail (SP_IS_ATTRIBUTE_TABLE (spat));
    g_return_if_fail (!object || SP_IS_OBJECT (object));
    g_return_if_fail (!object || (num_attr > 0));
    g_return_if_fail (!num_attr || (labels && attributes));

    if (spat->table) {
        gtk_widget_destroy (spat->table);
        spat->table = NULL;
    }

    if (spat->attributes) {
        gint i;
        for (i = 0; i < spat->num_attr; i++) {
            g_free (spat->attributes[i]);
        }
        g_free (spat->attributes);
        spat->attributes = NULL;
    }

    if (spat->entries) {
        g_free (spat->entries);
        spat->entries = NULL;
    }

    if (spat->hasobj) {
        if (spat->src.object) {
            spat->modified_connection.disconnect();
            spat->release_connection.disconnect();
            spat->src.object = NULL;
        }
    } else {
        if (spat->src.repr) {
            spat->src.repr = Inkscape::GC::release(spat->src.repr);
        }
    }

    spat->hasobj = TRUE;

    if (object) {
        gint i;

        spat->blocked = TRUE;

        /* Set up object */
        spat->src.object = object;
        spat->num_attr = num_attr;

        spat->modified_connection = object->connectModified(sigc::bind<2>(sigc::ptr_fun(&sp_attribute_table_object_modified), spat));
        spat->release_connection = object->connectRelease(sigc::bind<1>(sigc::ptr_fun(&sp_attribute_table_object_release), spat));

        /* Create table */
        spat->table = gtk_table_new (num_attr, 2, FALSE);
        gtk_container_add (GTK_CONTAINER (spat), spat->table);
        /* Arrays */
        spat->attributes = g_new0 (gchar *, num_attr);
        spat->entries = g_new0 (GtkWidget *, num_attr);
        /* Fill rows */
        for (i = 0; i < num_attr; i++) {
            GtkWidget *w;
            const gchar *val;

            spat->attributes[i] = g_strdup (attributes[i]);
            w = gtk_label_new (_(labels[i]));
            gtk_widget_show (w);
            gtk_misc_set_alignment (GTK_MISC (w), 1.0, 0.5);
            gtk_table_attach ( GTK_TABLE (spat->table), w, 0, 1, i, i + 1,
                               GTK_FILL,
                               (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                               XPAD, YPAD );
            w = gtk_entry_new ();
            gtk_widget_show (w);
            val = object->getRepr()->attribute(attributes[i]);
            gtk_entry_set_text (GTK_ENTRY (w), val ? val : (const gchar *) "");
            gtk_table_attach ( GTK_TABLE (spat->table), w, 1, 2, i, i + 1,
                               (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                               (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                               XPAD, YPAD );
            spat->entries[i] = w;
            g_signal_connect ( G_OBJECT (w), "changed",
                               G_CALLBACK (sp_attribute_table_entry_changed),
                               spat );
        }
        /* Show table */
        gtk_widget_show (spat->table);

        spat->blocked = FALSE;
    }

    gtk_widget_set_sensitive ( GTK_WIDGET (spat),
                               (spat->src.object != NULL) );

} // end of sp_attribute_table_set_object()



void sp_attribute_table_set_repr ( SPAttributeTable *spat,
                              Inkscape::XML::Node *repr,
                              gint num_attr,
                              const gchar **labels,
                              const gchar **attributes )
{
    g_return_if_fail (spat != NULL);
    g_return_if_fail (SP_IS_ATTRIBUTE_TABLE (spat));
    g_return_if_fail (!num_attr || (labels && attributes));

    if (spat->table) {
        gtk_widget_destroy (spat->table);
        spat->table = NULL;
    }

    if (spat->attributes) {
        gint i;
        for (i = 0; i < spat->num_attr; i++) {
            g_free (spat->attributes[i]);
        }
        g_free (spat->attributes);
        spat->attributes = NULL;
    }

    if (spat->entries) {
        g_free (spat->entries);
        spat->entries = NULL;
    }

    if (spat->hasobj) {
        if (spat->src.object) {
            spat->modified_connection.disconnect();
            spat->release_connection.disconnect();
            spat->src.object = NULL;
        }
    } else {
        if (spat->src.repr) {
            spat->src.repr = Inkscape::GC::release(spat->src.repr);
        }
    }

    spat->hasobj = FALSE;

    if (repr) {
        gint i;

        spat->blocked = TRUE;

        /* Set up repr */
        spat->src.repr = Inkscape::GC::anchor(repr);
        spat->num_attr = num_attr;
        /* Create table */
        spat->table = gtk_table_new (num_attr, 2, FALSE);
        gtk_container_add (GTK_CONTAINER (spat), spat->table);
        /* Arrays */
        spat->attributes = g_new0 (gchar *, num_attr);
        spat->entries = g_new0 (GtkWidget *, num_attr);

        /* Fill rows */
        for (i = 0; i < num_attr; i++) {
            GtkWidget *w;
            const gchar *val;

            spat->attributes[i] = g_strdup (attributes[i]);
            w = gtk_label_new (labels[i]);
            gtk_widget_show (w);
            gtk_misc_set_alignment (GTK_MISC (w), 1.0, 0.5);
            gtk_table_attach ( GTK_TABLE (spat->table), w, 0, 1, i, i + 1,
                               GTK_FILL,
                               (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                               XPAD, YPAD );
            w = gtk_entry_new ();
            gtk_widget_show (w);
            val = repr->attribute(attributes[i]);
            gtk_entry_set_text (GTK_ENTRY (w), val ? val : (const gchar *) "");
            gtk_table_attach ( GTK_TABLE (spat->table), w, 1, 2, i, i + 1,
                               (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                               (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                               XPAD, YPAD );
            spat->entries[i] = w;
            g_signal_connect ( G_OBJECT (w), "changed",
                               G_CALLBACK (sp_attribute_table_entry_changed),
                               spat );
        }
        /* Show table */
        gtk_widget_show (spat->table);

        spat->blocked = FALSE;
    }

    gtk_widget_set_sensitive (GTK_WIDGET (spat), (spat->src.repr != NULL));

} // end of sp_attribute_table_set_repr()



static void sp_attribute_table_object_modified ( SPObject */*object*/,
                                     guint flags,
                                     SPAttributeTable *spat )
{
    if (flags && SP_OBJECT_MODIFIED_FLAG)
    {
        gint i;
        for (i = 0; i < spat->num_attr; i++) {
            const gchar *val, *text;
            val = spat->src.object->getRepr()->attribute(spat->attributes[i]);
            text = gtk_entry_get_text (GTK_ENTRY (spat->entries[i]));
            if (val || text) {
                if (!val || !text || strcmp (val, text)) {
                    /* We are different */
                    spat->blocked = TRUE;
                    gtk_entry_set_text ( GTK_ENTRY (spat->entries[i]),
                                         val ? val : (const gchar *) "");
                    spat->blocked = FALSE;
                }
            }
        }
    } // end of if()

} // end of sp_attribute_table_object_modified()



static void sp_attribute_table_object_release (SPObject */*object*/, SPAttributeTable *spat)
{
    sp_attribute_table_set_object (spat, NULL, 0, NULL, NULL);
}



static void sp_attribute_table_entry_changed ( GtkEditable *editable,
                                   SPAttributeTable *spat )
{
    if (!spat->blocked)
    {
        gint i;
        for (i = 0; i < spat->num_attr; i++) {

            if (GTK_WIDGET (editable) == spat->entries[i]) {
                const gchar *text;
                spat->blocked = TRUE;
                text = gtk_entry_get_text (GTK_ENTRY (spat->entries[i]));

                if (!*text)
                    text = NULL;

                if (spat->hasobj && spat->src.object) {
                    spat->src.object->getRepr()->setAttribute(spat->attributes[i], text, false);
                    DocumentUndo::done(spat->src.object->document, SP_VERB_NONE,
                                       _("Set attribute"));

                } else if (spat->src.repr) {

                    spat->src.repr->setAttribute(spat->attributes[i], text, false);
                    /* TODO: Warning! Undo will not be flushed in given case */
                }
                spat->blocked = FALSE;
                return;
            }
        }
        g_warning ("file %s: line %d: Entry signalled change, but there is no such entry", __FILE__, __LINE__);
    } // end of if()

} // end of sp_attribute_table_entry_changed()

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
