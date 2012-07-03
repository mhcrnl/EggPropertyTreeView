/* Copyright (C) 2011, 2012 Matthias Vogelgesang <matthias.vogelgesang@kit.edu>
   (Karlsruhe Institute of Technology)

   This library is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as published by the
   Free Software Foundation; either version 2.1 of the License, or (at your
   option) any later version.

   This library is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
   details.

   You should have received a copy of the GNU Lesser General Public License along
   with this library; if not, write to the Free Software Foundation, Inc., 51
   Franklin St, Fifth Floor, Boston, MA 02110, USA */

#include "egg-property-cell-renderer.h"

G_DEFINE_TYPE (EggPropertyCellRenderer, egg_property_cell_renderer, GTK_TYPE_CELL_RENDERER)

#define EGG_PROPERTY_CELL_RENDERER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), EGG_TYPE_PROPERTY_CELL_RENDERER, EggPropertyCellRendererPrivate))

struct _EggPropertyCellRendererPrivate
{
    GObject         *object;
    GtkListStore    *list_store;
    GtkCellRenderer *renderer;
    GtkCellRenderer *text_renderer;
    GtkCellRenderer *spin_renderer;
    GtkCellRenderer *toggle_renderer;
};

enum
{
    PROP_0,
    PROP_PROP_NAME,
    N_PROPERTIES
};

static GParamSpec *egg_property_cell_renderer_properties[N_PROPERTIES] = { NULL, };

GtkCellRenderer *
egg_property_cell_renderer_new (GObject         *object,
                                GtkListStore    *list_store)
{
    EggPropertyCellRenderer *renderer;

    renderer = EGG_PROPERTY_CELL_RENDERER (g_object_new (EGG_TYPE_PROPERTY_CELL_RENDERER, NULL));
    renderer->priv->object = object;
    renderer->priv->list_store = list_store;
    return GTK_CELL_RENDERER (renderer);
}

static void
egg_property_cell_renderer_set_renderer (EggPropertyCellRenderer    *renderer,
                                         const gchar                *prop_name)
{
    EggPropertyCellRendererPrivate *priv;
    GObjectClass *oclass;
    GParamSpec *pspec;
    gchar *text = NULL;

    priv = EGG_PROPERTY_CELL_RENDERER_GET_PRIVATE (renderer);
    oclass = G_OBJECT_GET_CLASS (priv->object);
    pspec = g_object_class_find_property (oclass, prop_name);

    /*
     * Set this renderers mode, so that any actions can be forwarded to our
     * child renderers.
     */
    switch (pspec->value_type) {
        /* spin renderers */
        case G_TYPE_BOOLEAN:
            g_object_set (renderer,
                    "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
                    NULL);
            priv->renderer = priv->toggle_renderer;
            break;

        case G_TYPE_FLOAT:
        case G_TYPE_DOUBLE:
        case G_TYPE_UINT:
        case G_TYPE_UINT64:
        case G_TYPE_INT:
        case G_TYPE_INT64:
            g_object_set (renderer,
                    "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
                    NULL);
            priv->renderer = priv->spin_renderer;
            break;

        case G_TYPE_POINTER:
        case G_TYPE_STRING:
            g_object_set (renderer,
                    "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
                    NULL);
            priv->renderer = priv->text_renderer;
            break;
    }

    /*
     * Set the content from the objects property.
     */
    switch (pspec->value_type) {
        case G_TYPE_BOOLEAN:
            {
                gboolean val;

                g_object_get (priv->object, prop_name, &val, NULL);
                g_object_set (priv->renderer,
                        "active", val,
                        "activatable", pspec->flags & G_PARAM_WRITABLE ? TRUE : FALSE,
                        NULL);
                break;
            }

        case G_TYPE_UINT:
        case G_TYPE_INT:
        case G_TYPE_INT64:
        case G_TYPE_FLOAT:
        case G_TYPE_DOUBLE:
            {
                GValue from = { 0 };
                GValue to_string = { 0 };
                GValue to_double = { 0 };

                g_value_init (&from, pspec->value_type);
                g_value_init (&to_string, G_TYPE_STRING);
                g_value_init (&to_double, G_TYPE_DOUBLE);
                g_object_get_property (priv->object, prop_name, &from);

                if (g_value_transform (&from, &to_string))
                    text = g_strdup (g_value_get_string (&to_string));
                else
                    g_warning ("Could not convert from %s gchar*\n", g_type_name (pspec->value_type));

                if (pspec->flags & G_PARAM_WRITABLE) {
                    GtkObject *adjustment;

                    g_object_get (priv->renderer,
                            "adjustment", &adjustment,
                            NULL);

                    if (adjustment)
                        g_object_unref (adjustment);

                    g_value_transform (&from, &to_double);
                    adjustment = gtk_adjustment_new (g_value_get_double (&to_double), 0, 1000, 1, 10, 0);

                    g_object_set (priv->renderer,
                            "editable", TRUE,
                            "adjustment", adjustment,
                            NULL);
                }
            }
            break;

        case G_TYPE_STRING:
            g_object_get (priv->object, prop_name, &text, NULL);
            break;

        case G_TYPE_POINTER:
            {
                gpointer val;

                g_object_get (priv->object, prop_name, &val, NULL);
                text = g_strdup_printf ("0x%x", GPOINTER_TO_INT (val));
            }
            break;

        default:
            break;
    }

    if (text != NULL) {
        g_object_set (priv->renderer, "text", text, NULL);
        g_free (text);
    }
}

static void
egg_property_cell_renderer_toggle_cb (GtkCellRendererToggle *renderer,
                                      gchar                 *path,
                                      gpointer               user_data)
{
    GtkTreeIter iter;
    EggPropertyCellRendererPrivate *priv;

    priv = (EggPropertyCellRendererPrivate *) user_data;

    if (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (priv->list_store), &iter, path)) {
        gchar *prop_name;
        gboolean activated;

        gtk_tree_model_get (GTK_TREE_MODEL (priv->list_store), &iter,
                0, &prop_name,
                -1);

        g_object_get (priv->object,
                prop_name, &activated,
                NULL);

        g_object_set (priv->object,
                prop_name, !activated,
                NULL);

        g_free (prop_name);
    }
}

static void
egg_property_cell_renderer_text_edited_cb (GtkCellRendererText  *renderer,
                                           gchar                *path,
                                           gchar                *new_text,
                                           gpointer              user_data)
{
    g_print ("text edited with path=%s and new_text=%s!\n", path, new_text);
}

static void
egg_property_cell_renderer_spin_edited_cb (GtkCellRendererText  *renderer,
                                           gchar                *path,
                                           gchar                *new_text,
                                           gpointer              user_data)
{
    GtkTreeIter iter;
    EggPropertyCellRendererPrivate *priv;

    priv = (EggPropertyCellRendererPrivate *) user_data;

    if (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (priv->list_store), &iter, path)) {
        gchar *prop_name;
        GObjectClass *oclass;
        GParamSpec *pspec;
        GValue from = { 0 };
        GValue to = { 0 };

        gtk_tree_model_get (GTK_TREE_MODEL (priv->list_store), &iter,
                0, &prop_name,
                -1);

        oclass = G_OBJECT_GET_CLASS (priv->object);
        pspec = g_object_class_find_property (oclass, prop_name);

        g_value_init (&from, G_TYPE_DOUBLE);
        g_value_init (&to, pspec->value_type);
        g_value_set_double (&from, g_ascii_strtod (new_text, NULL));

        if (g_value_transform (&from, &to))
            g_object_set_property (priv->object, prop_name, &to);
        else
            g_warning ("Could not transform %s to %s\n",
                    g_value_get_string (&from), g_type_name (pspec->value_type));

        g_free (prop_name);
    }
}

static void
egg_property_cell_renderer_get_size (GtkCellRenderer    *cell,
                                     GtkWidget          *widget,
                                     GdkRectangle       *cell_area,
                                     gint               *x_offset,
                                     gint               *y_offset,
                                     gint               *width,
                                     gint               *height)
{

    EggPropertyCellRendererPrivate *priv = EGG_PROPERTY_CELL_RENDERER_GET_PRIVATE (cell);

    gtk_cell_renderer_get_size (priv->renderer, widget, cell_area, x_offset, y_offset, width, height);
}

static void
egg_property_cell_renderer_render (GtkCellRenderer      *cell,
                                   GdkDrawable          *window,
                                   GtkWidget            *widget,
                                   GdkRectangle         *background_area,
                                   GdkRectangle         *cell_area,
                                   GdkRectangle         *expose_area,
                                   GtkCellRendererState  flags)
{
    EggPropertyCellRendererPrivate *priv = EGG_PROPERTY_CELL_RENDERER_GET_PRIVATE (cell);
    gtk_cell_renderer_render (priv->renderer, window, widget, background_area, cell_area, expose_area, flags);
}

static gboolean
egg_property_cell_renderer_activate (GtkCellRenderer        *cell,
                                     GdkEvent               *event,
                                     GtkWidget              *widget,
                                     const gchar            *path,
                                     GdkRectangle           *background_area,
                                     GdkRectangle           *cell_area,
                                     GtkCellRendererState    flags)
{
    EggPropertyCellRendererPrivate *priv = EGG_PROPERTY_CELL_RENDERER_GET_PRIVATE (cell);
    return gtk_cell_renderer_activate (priv->renderer, event, widget, path, background_area, cell_area, flags);
}

static GtkCellEditable *
egg_property_cell_renderer_start_editing (GtkCellRenderer        *cell,
                                          GdkEvent               *event,
                                          GtkWidget              *widget,
                                          const gchar            *path,
                                          GdkRectangle           *background_area,
                                          GdkRectangle           *cell_area,
                                          GtkCellRendererState    flags)
{
    EggPropertyCellRendererPrivate *priv = EGG_PROPERTY_CELL_RENDERER_GET_PRIVATE (cell);
    return gtk_cell_renderer_start_editing (priv->renderer, event, widget, path, background_area, cell_area, flags);
}

static void
egg_property_cell_renderer_set_property (GObject        *object,
                                         guint           property_id,
                                         const GValue   *value,
                                         GParamSpec     *pspec)
{
    g_return_if_fail (EGG_IS_PROPERTY_CELL_RENDERER (object));
    EggPropertyCellRenderer *renderer = EGG_PROPERTY_CELL_RENDERER (object);

    switch (property_id) {
        case PROP_PROP_NAME:
            egg_property_cell_renderer_set_renderer (renderer, g_value_get_string (value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            return;
    }
}

static void
egg_property_cell_renderer_get_property (GObject    *object,
                                         guint       property_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
    g_return_if_fail (EGG_IS_PROPERTY_CELL_RENDERER (object));

    switch (property_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            return;
    }
}

static void
egg_property_cell_renderer_class_init (EggPropertyCellRendererClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkCellRendererClass *cellrenderer_class = GTK_CELL_RENDERER_CLASS (klass);

    gobject_class->set_property = egg_property_cell_renderer_set_property;
    gobject_class->get_property = egg_property_cell_renderer_get_property;

    cellrenderer_class->render = egg_property_cell_renderer_render;
    cellrenderer_class->get_size = egg_property_cell_renderer_get_size;
    cellrenderer_class->activate = egg_property_cell_renderer_activate;
    cellrenderer_class->start_editing = egg_property_cell_renderer_start_editing;

    egg_property_cell_renderer_properties[PROP_PROP_NAME] =
            g_param_spec_string("prop-name",
                                "Property name", "Property name", "",
                                G_PARAM_READWRITE);

    g_object_class_install_property(gobject_class, PROP_PROP_NAME, egg_property_cell_renderer_properties[PROP_PROP_NAME]);

    g_type_class_add_private (klass, sizeof (EggPropertyCellRendererPrivate));
}

static void
egg_property_cell_renderer_init (EggPropertyCellRenderer *renderer)
{
    EggPropertyCellRendererPrivate *priv;

    renderer->priv = priv = EGG_PROPERTY_CELL_RENDERER_GET_PRIVATE (renderer);

    priv->text_renderer = gtk_cell_renderer_text_new ();
    priv->spin_renderer = gtk_cell_renderer_spin_new ();
    priv->toggle_renderer = gtk_cell_renderer_toggle_new ();
    priv->renderer = priv->text_renderer;

    g_object_set (priv->text_renderer,
            "editable", TRUE,
            NULL);

    g_object_set (priv->spin_renderer,
            "editable", TRUE,
            NULL);

    g_object_set (priv->toggle_renderer,
            "xalign", 0.0f,
            "activatable", TRUE,
            "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
            NULL);

    g_signal_connect (priv->spin_renderer, "edited",
            G_CALLBACK (egg_property_cell_renderer_spin_edited_cb), priv);

    g_signal_connect (priv->text_renderer, "edited",
            G_CALLBACK (egg_property_cell_renderer_text_edited_cb), NULL);

    g_signal_connect (priv->toggle_renderer, "toggled",
            G_CALLBACK (egg_property_cell_renderer_toggle_cb), priv);
}
