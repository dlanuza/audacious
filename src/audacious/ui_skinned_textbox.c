/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007  Audacious development team.
 *
 * Based on:
 * BMP - Cross-platform multimedia player
 * Copyright (C) 2003-2004  BMP development team.
 * XMMS:
 * Copyright (C) 1998-2003  XMMS development team.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "widgets/widgetcore.h"
#include "ui_skinned_textbox.h"
#include "main.h"
#include "util.h"
#include "strings.h"
#include <string.h>
#include <ctype.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmarshal.h>
#include <gtk/gtkimage.h>

#define UI_TYPE_SKINNED_TEXTBOX           (ui_skinned_textbox_get_type())
#define UI_SKINNED_TEXTBOX_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), UI_TYPE_SKINNED_TEXTBOX, UiSkinnedTextboxPrivate))
typedef struct _UiSkinnedTextboxPrivate UiSkinnedTextboxPrivate;

static GMutex *mutex = NULL;

#define TEXTBOX_SCROLL_SMOOTH_TIMEOUT  30
#define TEXTBOX_SCROLL_WAIT            80

enum {
    CLICKED,
    DOUBLE_CLICKED,
    RIGHT_CLICKED,
    DOUBLED,
    REDRAW,
    LAST_SIGNAL
};

struct _UiSkinnedTextboxPrivate {
    GdkPixmap        *img;
    SkinPixmapId     skin_index;
    GtkWidget        *fixed;
    gboolean         double_size;
    gboolean         scroll_back;
    gint             nominal_y, nominal_height;
    gint             scroll_timeout;
    gint             font_ascent, font_descent;
    PangoFontDescription *font;
    gchar            *fontname;
    gchar            *pixmap_text;
    gint             skin_id;
    gint             drag_x, drag_off, offset;
    gboolean         is_scrollable, is_dragging;
    gint             pixmap_width;
    GdkPixmap        *pixmap;
    gboolean         scroll_allowed, scroll_enabled;
    gint             scroll_dummy;
    gint             resize_width, resize_height;
    gint             move_x, move_y;
};

static void ui_skinned_textbox_class_init         (UiSkinnedTextboxClass *klass);
static void ui_skinned_textbox_init               (UiSkinnedTextbox *textbox);
static void ui_skinned_textbox_destroy            (GtkObject *object);
static void ui_skinned_textbox_realize            (GtkWidget *widget);
static void ui_skinned_textbox_size_request       (GtkWidget *widget, GtkRequisition *requisition);
static void ui_skinned_textbox_size_allocate      (GtkWidget *widget, GtkAllocation *allocation);
static gboolean ui_skinned_textbox_expose         (GtkWidget *widget, GdkEventExpose *event);
static gboolean ui_skinned_textbox_button_press   (GtkWidget *widget, GdkEventButton *event);
static gboolean ui_skinned_textbox_button_release (GtkWidget *widget, GdkEventButton *event);
static gboolean ui_skinned_textbox_motion_notify  (GtkWidget *widget, GdkEventMotion *event);
static void ui_skinned_textbox_toggle_doublesize  (UiSkinnedTextbox *textbox);
static void ui_skinned_textbox_redraw             (UiSkinnedTextbox *textbox);
static gboolean ui_skinned_textbox_should_scroll  (UiSkinnedTextbox *textbox);
static void textbox_generate_xfont_pixmap         (UiSkinnedTextbox *textbox, const gchar *pixmaptext);
static gboolean textbox_scroll                    (gpointer data);
static void textbox_generate_pixmap               (UiSkinnedTextbox *textbox);
static void textbox_handle_special_char           (gchar c, gint * x, gint * y);

static GtkWidgetClass *parent_class = NULL;
static guint textbox_signals[LAST_SIGNAL] = { 0 };

GType ui_skinned_textbox_get_type() {
    static GType textbox_type = 0;
    if (!textbox_type) {
        static const GTypeInfo textbox_info = {
            sizeof (UiSkinnedTextboxClass),
            NULL,
            NULL,
            (GClassInitFunc) ui_skinned_textbox_class_init,
            NULL,
            NULL,
            sizeof (UiSkinnedTextbox),
            0,
            (GInstanceInitFunc) ui_skinned_textbox_init,
        };
        textbox_type = g_type_register_static (GTK_TYPE_WIDGET, "UiSkinnedTextbox", &textbox_info, 0);
    }

    return textbox_type;
}

static void ui_skinned_textbox_class_init(UiSkinnedTextboxClass *klass) {
    GObjectClass *gobject_class;
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    gobject_class = G_OBJECT_CLASS(klass);
    object_class = (GtkObjectClass*) klass;
    widget_class = (GtkWidgetClass*) klass;
    parent_class = gtk_type_class (gtk_widget_get_type ());

    object_class->destroy = ui_skinned_textbox_destroy;

    widget_class->realize = ui_skinned_textbox_realize;
    widget_class->expose_event = ui_skinned_textbox_expose;
    widget_class->size_request = ui_skinned_textbox_size_request;
    widget_class->size_allocate = ui_skinned_textbox_size_allocate;
    widget_class->button_press_event = ui_skinned_textbox_button_press;
    widget_class->button_release_event = ui_skinned_textbox_button_release;
    widget_class->motion_notify_event = ui_skinned_textbox_motion_notify;

    klass->clicked = NULL;
    klass->double_clicked = NULL;
    klass->right_clicked = NULL;
    klass->doubled = ui_skinned_textbox_toggle_doublesize;
    klass->redraw = ui_skinned_textbox_redraw;

    textbox_signals[CLICKED] = 
        g_signal_new ("clicked", G_OBJECT_CLASS_TYPE (object_class), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (UiSkinnedTextboxClass, clicked), NULL, NULL,
                      gtk_marshal_VOID__VOID, G_TYPE_NONE, 0);

    textbox_signals[DOUBLE_CLICKED] = 
        g_signal_new ("double-clicked", G_OBJECT_CLASS_TYPE (object_class), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (UiSkinnedTextboxClass, double_clicked), NULL, NULL,
                      gtk_marshal_VOID__VOID, G_TYPE_NONE, 0);

    textbox_signals[RIGHT_CLICKED] = 
        g_signal_new ("right-clicked", G_OBJECT_CLASS_TYPE (object_class), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (UiSkinnedTextboxClass, right_clicked), NULL, NULL,
                      gtk_marshal_VOID__VOID, G_TYPE_NONE, 0);

    textbox_signals[DOUBLED] = 
        g_signal_new ("toggle-double-size", G_OBJECT_CLASS_TYPE (object_class), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (UiSkinnedTextboxClass, doubled), NULL, NULL,
                      gtk_marshal_VOID__VOID, G_TYPE_NONE, 0);

    textbox_signals[REDRAW] = 
        g_signal_new ("redraw", G_OBJECT_CLASS_TYPE (object_class), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (UiSkinnedTextboxClass, redraw), NULL, NULL,
                      gtk_marshal_VOID__VOID, G_TYPE_NONE, 0);

    g_type_class_add_private (gobject_class, sizeof (UiSkinnedTextboxPrivate));
}

static void ui_skinned_textbox_init(UiSkinnedTextbox *textbox) {
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);
    mutex = g_mutex_new();
    priv->resize_width = 0;
    priv->resize_height = 0;
    priv->move_x = 0;
    priv->move_y = 0;
    priv->img = NULL;
}

GtkWidget* ui_skinned_textbox_new(GtkWidget *fixed, gint x, gint y, gint w, gboolean allow_scroll, SkinPixmapId si) {
    UiSkinnedTextbox *textbox = g_object_new (ui_skinned_textbox_get_type (), NULL);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    textbox->height = bmp_active_skin->properties.textbox_bitmap_font_height;
    textbox->x = x;
    textbox->y = y;
    textbox->text = g_strdup("");
    textbox->width = w;
    priv->scroll_allowed = allow_scroll;
    priv->scroll_enabled = TRUE;
    priv->skin_index = si;
    priv->nominal_y = y;
    priv->nominal_height = textbox->height;
    priv->scroll_timeout = 0;
    priv->scroll_dummy = 0;

    priv->fixed = fixed;
    priv->double_size = FALSE;

    gtk_fixed_put(GTK_FIXED(priv->fixed), GTK_WIDGET(textbox), textbox->x, textbox->y);

    return GTK_WIDGET(textbox);
}

static void ui_skinned_textbox_destroy(GtkObject *object) {
    UiSkinnedTextbox *textbox;

    g_return_if_fail (object != NULL);
    g_return_if_fail (UI_SKINNED_IS_TEXTBOX (object));

    textbox = UI_SKINNED_TEXTBOX (object);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void ui_skinned_textbox_realize(GtkWidget *widget) {
    UiSkinnedTextbox *textbox;
    GdkWindowAttr attributes;
    gint attributes_mask;

    g_return_if_fail (widget != NULL);
    g_return_if_fail (UI_SKINNED_IS_TEXTBOX(widget));

    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
    textbox = UI_SKINNED_TEXTBOX(widget);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events(widget);
    attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | 
                             GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
                             GDK_POINTER_MOTION_HINT_MASK;
    attributes.visual = gtk_widget_get_visual(widget);
    attributes.colormap = gtk_widget_get_colormap(widget);

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new(widget->parent->window, &attributes, attributes_mask);

    widget->style = gtk_style_attach(widget->style, widget->window);

    gdk_window_set_user_data(widget->window, widget);
}

static void ui_skinned_textbox_size_request(GtkWidget *widget, GtkRequisition *requisition) {
    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX(widget);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    requisition->width = textbox->width*(1+priv->double_size);
    requisition->height = textbox->height*(1+priv->double_size);
}

static void ui_skinned_textbox_size_allocate(GtkWidget *widget, GtkAllocation *allocation) {
    g_mutex_lock(mutex);
    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX (widget);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    widget->allocation = *allocation;
    if (GTK_WIDGET_REALIZED (widget))
        gdk_window_move_resize(widget->window, allocation->x, allocation->y, allocation->width, allocation->height);

    textbox->x = widget->allocation.x/(priv->double_size ? 2 : 1);
    textbox->y = widget->allocation.y/(priv->double_size ? 2 : 1);
    priv->move_x = 0;
    priv->move_y = 0;

    if (textbox->width != widget->allocation.width/(priv->double_size ? 2 : 1)) {
        textbox->width = widget->allocation.width/(priv->double_size ? 2 : 1);
        priv->resize_width = 0;
        priv->resize_height = 0;
        if (priv->pixmap_text) g_free(priv->pixmap_text);
        priv->pixmap_text = NULL;
        priv->offset = 0;
        gtk_widget_queue_draw(GTK_WIDGET(textbox));
    }
    g_mutex_unlock(mutex);
}

static gboolean ui_skinned_textbox_expose(GtkWidget *widget, GdkEventExpose *event) {
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (UI_SKINNED_IS_TEXTBOX (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX (widget);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    GdkPixmap *obj = NULL;
    GdkGC *gc;
    gint cw;

    if (textbox->text && (!priv->pixmap_text || strcmp(textbox->text, priv->pixmap_text)))
        textbox_generate_pixmap(textbox);

    if (priv->pixmap) {
        if (skin_get_id() != priv->skin_id) {
            priv->skin_id = skin_get_id();
            textbox_generate_pixmap(textbox);
        }
        obj = gdk_pixmap_new(NULL, textbox->width, textbox->height, gdk_rgb_get_visual()->depth);
        gc = gdk_gc_new(obj);

        if (cfg.twoway_scroll) { // twoway scroll
            cw = priv->pixmap_width - priv->offset;
            if (cw > textbox->width)
                cw = textbox->width;
            gdk_draw_drawable(obj, gc, priv->pixmap, priv->offset, 0, 0, 0, cw, textbox->height);
            if (cw < textbox->width)
                gdk_draw_drawable(obj, gc, priv->pixmap, 0, 0,
                                  textbox->x + cw, textbox->y,
                                  textbox->width - cw, textbox->height);
        } else { // oneway scroll
            int cw1, cw2;

            if (priv->offset >= priv->pixmap_width)
                priv->offset = 0;

            if (priv->pixmap_width - priv->offset > textbox->width) { // case1
                cw1 = textbox->width;
                gdk_draw_drawable(obj, gc, priv->pixmap, priv->offset, 0,
                                  0, 0, cw1, textbox->height);
            } else { // case 2
                cw1 = priv->pixmap_width - priv->offset;
                gdk_draw_drawable(obj, gc, priv->pixmap, priv->offset, 0,
                                  0, 0, cw1, textbox->height);
                cw2 = textbox->width - cw1;
                gdk_draw_drawable(obj, gc, priv->pixmap, 0, 0, cw1, 0, cw2, textbox->height);
            }

        }
    if (priv->img)
        g_object_unref(priv->img);
    priv->img = gdk_pixmap_new(NULL, textbox->width*(1+priv->double_size),
                               textbox->height*(1+priv->double_size),
                               gdk_rgb_get_visual()->depth);

    if (priv->double_size) {
        GdkImage *img, *img2x;
        img = gdk_drawable_get_image(obj, 0, 0, textbox->width, textbox->height);
        img2x = create_dblsize_image(img);
        gdk_draw_image (priv->img, gc, img2x, 0, 0, 0, 0, textbox->width*2, textbox->height*2);
        g_object_unref(img2x);
        g_object_unref(img);
    } else
        gdk_draw_drawable (priv->img, gc, obj, 0, 0, 0, 0, textbox->width, textbox->height);


        g_object_unref(obj);

        gdk_draw_drawable (widget->window, gc, priv->img, 0, 0, 0, 0,
                           textbox->width*(1+priv->double_size), textbox->height*(1+priv->double_size));
        g_object_unref(gc);
    }

    return FALSE;
}

static gboolean ui_skinned_textbox_button_press(GtkWidget *widget, GdkEventButton *event) {
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (UI_SKINNED_IS_TEXTBOX (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX (widget);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    if (event->type == GDK_BUTTON_PRESS) {
        textbox = UI_SKINNED_TEXTBOX(widget);
        if (event->button == 1) {
            if (priv->scroll_allowed) {
                if ((priv->pixmap_width > textbox->width) && priv->is_scrollable) {
                    priv->is_dragging = TRUE;
                    priv->drag_off = priv->offset;
                    priv->drag_x = event->x;
                }
            } else
                g_signal_emit(widget, textbox_signals[CLICKED], 0);

        }
    } else if (event->type == GDK_2BUTTON_PRESS) {
        if (event->button == 1) {
            g_signal_emit(widget, textbox_signals[DOUBLE_CLICKED], 0);
        }
    }

    return TRUE;
}

static gboolean ui_skinned_textbox_button_release(GtkWidget *widget, GdkEventButton *event) {
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(widget);

    if (event->button == 1) {
        priv->is_dragging = FALSE;
    } else if (event->button == 3) {
        g_signal_emit(widget, textbox_signals[RIGHT_CLICKED], 0);
    }

    return TRUE;
}

static gboolean ui_skinned_textbox_motion_notify(GtkWidget *widget, GdkEventMotion *event) {
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (UI_SKINNED_IS_TEXTBOX (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);
    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX(widget);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(widget);

    if (priv->is_dragging) {
        if (priv->scroll_allowed &&
            priv->pixmap_width > textbox->width) {
            priv->offset = priv->drag_off - (event->x - priv->drag_x);

            while (priv->offset < 0)
                priv->offset = 0;

            while (priv->offset > (priv->pixmap_width - textbox->width))
                priv->offset = (priv->pixmap_width - textbox->width);

            gtk_widget_queue_draw(widget);
        }
    }

  return TRUE;
}

static void ui_skinned_textbox_toggle_doublesize(UiSkinnedTextbox *textbox) {
    GtkWidget *widget = GTK_WIDGET (textbox);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    priv->double_size = !priv->double_size;

    gtk_widget_set_size_request(widget, textbox->width*(1+priv->double_size), textbox->height*(1+priv->double_size));
    gtk_widget_set_uposition(widget, textbox->x*(1+priv->double_size), textbox->y*(1+priv->double_size));

    gtk_widget_queue_draw(GTK_WIDGET(textbox));
}

static void ui_skinned_textbox_redraw(UiSkinnedTextbox *textbox) {
    g_mutex_lock(mutex);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    if (priv->resize_width || priv->resize_height)
        gtk_widget_set_size_request(GTK_WIDGET(textbox),
                                   (textbox->width+priv->resize_width)*(1+priv->double_size),
                                   (textbox->height+priv->resize_height)*(1+priv->double_size));
    if (priv->move_x || priv->move_y)
        gtk_fixed_move(GTK_FIXED(priv->fixed), GTK_WIDGET(textbox), textbox->x+priv->move_x, textbox->y+priv->move_y);

    gtk_widget_queue_draw(GTK_WIDGET(textbox));
    g_mutex_unlock(mutex);
}

static gboolean ui_skinned_textbox_should_scroll(UiSkinnedTextbox *textbox) {
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    if (!priv->scroll_allowed)
        return FALSE;

    if (priv->font) {
        gint width;
        text_get_extents(priv->fontname, textbox->text, &width, NULL, NULL, NULL);

        if (width <= textbox->width)
            return FALSE;
        else
            return TRUE;
    }

    if (g_utf8_strlen(textbox->text, -1) * bmp_active_skin->properties.textbox_bitmap_font_width > textbox->width)
        return TRUE;

    return FALSE;
}

void ui_skinned_textbox_set_xfont(GtkWidget *widget, gboolean use_xfont, const gchar * fontname) {
    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX (widget);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    gint ascent, descent;

    g_return_if_fail(textbox != NULL);

    if (priv->font) {
        pango_font_description_free(priv->font);
        priv->font = NULL;
    }

    textbox->y = priv->nominal_y;
    textbox->height = priv->nominal_height;

    /* Make sure the pixmap is regenerated */
    if (priv->pixmap_text) {
        g_free(priv->pixmap_text);
        priv->pixmap_text = NULL;
    }

    if (!use_xfont || strlen(fontname) == 0)
        return;

    priv->font = pango_font_description_from_string(fontname);
    priv->fontname = g_strdup(fontname);

    text_get_extents(fontname,
                     "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz ",
                     NULL, NULL, &ascent, &descent);
    priv->font_ascent = ascent;
    priv->font_descent = descent;


    if (priv->font == NULL)
        return;

    textbox->height = priv->font_ascent;
    if (textbox->height > priv->nominal_height)
        textbox->y -= (textbox->height - priv->nominal_height) / 2;
    else
        textbox->height = priv->nominal_height;
}

void ui_skinned_textbox_set_text(GtkWidget *widget, const gchar *text) {
    g_return_if_fail(text != NULL);
    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX (widget);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    if (!strcmp(textbox->text, text))
         return;
    if (textbox->text)
        g_free(textbox->text);

    textbox->text = str_to_utf8(text);
    priv->scroll_back = FALSE;
    gtk_widget_queue_draw(GTK_WIDGET(textbox));
}

static void textbox_generate_xfont_pixmap(UiSkinnedTextbox *textbox, const gchar *pixmaptext) {
    gint length, i;
    GdkGC *gc, *maskgc;
    GdkColor *c, pattern;
    GdkBitmap *mask;
    PangoLayout *layout;
    gint width;

    g_return_if_fail(textbox != NULL);
    g_return_if_fail(pixmaptext != NULL);

    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    length = g_utf8_strlen(pixmaptext, -1);

    text_get_extents(priv->fontname, pixmaptext, &width, NULL, NULL, NULL);

    priv->pixmap_width = MAX(width, textbox->width);
    priv->pixmap = gdk_pixmap_new(mainwin->window, priv->pixmap_width,
                                   textbox->height,
                                   gdk_rgb_get_visual()->depth);
    gc = gdk_gc_new(priv->pixmap);
    c = skin_get_color(bmp_active_skin, SKIN_TEXTBG);
    for (i = 0; i < textbox->height; i++) {
        gdk_gc_set_foreground(gc, &c[6 * i / textbox->height]);
        gdk_draw_line(priv->pixmap, gc, 0, i, priv->pixmap_width, i);
    }

    mask = gdk_pixmap_new(mainwin->window, priv->pixmap_width, textbox->height, 1);
    maskgc = gdk_gc_new(mask);
    pattern.pixel = 0;
    gdk_gc_set_foreground(maskgc, &pattern);

    gdk_draw_rectangle(mask, maskgc, TRUE, 0, 0, priv->pixmap_width, textbox->height);
    pattern.pixel = 1;
    gdk_gc_set_foreground(maskgc, &pattern);

    gdk_gc_set_foreground(gc, skin_get_color(bmp_active_skin, SKIN_TEXTFG));

    layout = gtk_widget_create_pango_layout(mainwin, pixmaptext);
    pango_layout_set_font_description(layout, priv->font);

    gdk_draw_layout(priv->pixmap, gc, 0, (priv->font_descent / 2), layout);
    g_object_unref(layout);

    g_object_unref(maskgc);

    gdk_gc_set_clip_mask(gc, mask);
    c = skin_get_color(bmp_active_skin, SKIN_TEXTFG);
    for (i = 0; i < textbox->height; i++) {
        gdk_gc_set_foreground(gc, &c[6 * i / textbox->height]);
        gdk_draw_line(priv->pixmap, gc, 0, i, priv->pixmap_width, i);
    }
    g_object_unref(mask);
    g_object_unref(gc);
}

static gboolean textbox_scroll(gpointer data) {
    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX(data);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    if (!priv->is_dragging) {
        if (priv->scroll_dummy < TEXTBOX_SCROLL_WAIT)
            priv->scroll_dummy++;
        else {
            if(cfg.twoway_scroll) {
                if (priv->scroll_back)
                    priv->offset -= 1;
                else
                    priv->offset += 1;

                if (priv->offset >= (priv->pixmap_width - textbox->width)) {
                    priv->scroll_back = TRUE;
                    priv->scroll_dummy = 0;
                }
                if (priv->offset <= 0) {
                    priv->scroll_back = FALSE;
                    priv->scroll_dummy = 0;
                }
            }
            else { // oneway scroll
                priv->scroll_back = FALSE;
                priv->offset += 1;
            }
            gtk_widget_queue_draw(GTK_WIDGET(textbox));
        }
    }
    return TRUE;
}

static void textbox_generate_pixmap(UiSkinnedTextbox *textbox) {
    gint length, i, x, y, wl;
    gchar *pixmaptext;
    gchar *tmp, *stxt;
    GdkGC *gc;

    g_return_if_fail(textbox != NULL);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    if (priv->pixmap) {
        g_object_unref(priv->pixmap);
        priv->pixmap = NULL;
    }

    /*
     * Don't reset the offset if only text after the last '(' has
     * changed.  This is a hack to avoid visual noice on vbr files
     * where we guess the length.
     */
    if (!(priv->pixmap_text && strrchr(textbox->text, '(') &&
          !strncmp(priv->pixmap_text, textbox->text,
                   strrchr(textbox->text, '(') - textbox->text)))
        priv->offset = 0;

    g_free(priv->pixmap_text);
    priv->pixmap_text = g_strdup(textbox->text);

    /*
     * wl is the number of (partial) letters visible. Only makes
     * sense when using skinned font.
     */
    wl = textbox->width / 5;
    if (wl * 5 != textbox->width)
        wl++;

    length = g_utf8_strlen(textbox->text, -1);

    priv->is_scrollable = FALSE;

    priv->is_scrollable = ui_skinned_textbox_should_scroll(textbox);

    if (priv->is_scrollable) {
        if(!cfg.twoway_scroll) {
            pixmaptext = g_strdup_printf("%s *** ", priv->pixmap_text);
            length += 5;
        } else
            pixmaptext = g_strdup(priv->pixmap_text);
    } else
    if (!priv->font && length <= wl) {
        gint pad = wl - length;
        gchar *padchars = g_strnfill(pad, ' ');

        pixmaptext = g_strconcat(priv->pixmap_text, padchars, NULL);
        g_free(padchars);
        length += pad;
    } else
        pixmaptext = g_strdup(priv->pixmap_text);

    if (priv->is_scrollable) {
        if (priv->scroll_enabled && !priv->scroll_timeout) {
            gint tag;
            tag = TEXTBOX_SCROLL_SMOOTH_TIMEOUT;
            priv->scroll_timeout = g_timeout_add(tag, textbox_scroll, textbox);
        }
    } else {
        if (priv->scroll_timeout) {
            g_source_remove(priv->scroll_timeout);
            priv->scroll_timeout = 0;
        }
        priv->offset = 0;
    }

    if (priv->font) {
        textbox_generate_xfont_pixmap(textbox, pixmaptext);
        g_free(pixmaptext);
        return;
    }

    priv->pixmap_width = length * bmp_active_skin->properties.textbox_bitmap_font_width;
    priv->pixmap = gdk_pixmap_new(NULL,
                                     priv->pixmap_width, bmp_active_skin->properties.textbox_bitmap_font_height,
                                     gdk_rgb_get_visual()->depth);
    gc = gdk_gc_new(priv->pixmap);

    for (tmp = stxt = g_utf8_strup(pixmaptext, -1), i = 0;
         tmp != NULL && i < length; i++, tmp = g_utf8_next_char(tmp)) {
        gchar c = *tmp;
        x = y = -1;
        if (c >= 'A' && c <= 'Z') {
            x = bmp_active_skin->properties.textbox_bitmap_font_width * (c - 'A');
            y = 0;
        }
        else if (c >= '0' && c <= '9') {
            x = bmp_active_skin->properties.textbox_bitmap_font_width * (c - '0');
            y = bmp_active_skin->properties.textbox_bitmap_font_height;
        }
        else
            textbox_handle_special_char(c, &x, &y);

        skin_draw_pixmap(bmp_active_skin,
                         priv->pixmap, gc, priv->skin_index,
                         x, y, i * bmp_active_skin->properties.textbox_bitmap_font_width, 0,
                         bmp_active_skin->properties.textbox_bitmap_font_width, 
                         bmp_active_skin->properties.textbox_bitmap_font_height);
    }
    g_free(stxt);
    g_free(pixmaptext);
    g_object_unref(gc);
}

void ui_skinned_textbox_set_scroll(GtkWidget *widget, gboolean scroll) {
    g_return_if_fail(widget != NULL);
    UiSkinnedTextbox *textbox = UI_SKINNED_TEXTBOX(widget);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(textbox);

    priv->scroll_enabled = scroll;
    if (priv->scroll_enabled && priv->is_scrollable && priv->scroll_allowed) {
        gint tag;
        tag = TEXTBOX_SCROLL_SMOOTH_TIMEOUT;
        if (priv->scroll_timeout) {
            g_source_remove(priv->scroll_timeout);
            priv->scroll_timeout = 0;
        }
        priv->scroll_timeout = g_timeout_add(tag, textbox_scroll, textbox);

    } else {

        if (priv->scroll_timeout) {
            g_source_remove(priv->scroll_timeout);
            priv->scroll_timeout = 0;
        }

        priv->offset = 0;
        gtk_widget_queue_draw(GTK_WIDGET(textbox));
    }
}

static void textbox_handle_special_char(gchar c, gint * x, gint * y) {
    gint tx, ty;

    switch (c) {
    case '"':
        tx = 26;
        ty = 0;
        break;
    case '\r':
        tx = 10;
        ty = 1;
        break;
    case ':':
    case ';':
        tx = 12;
        ty = 1;
        break;
    case '(':
        tx = 13;
        ty = 1;
        break;
    case ')':
        tx = 14;
        ty = 1;
        break;
    case '-':
        tx = 15;
        ty = 1;
        break;
    case '`':
    case '\'':
        tx = 16;
        ty = 1;
        break;
    case '!':
        tx = 17;
        ty = 1;
        break;
    case '_':
        tx = 18;
        ty = 1;
        break;
    case '+':
        tx = 19;
        ty = 1;
        break;
    case '\\':
        tx = 20;
        ty = 1;
        break;
    case '/':
        tx = 21;
        ty = 1;
        break;
    case '[':
        tx = 22;
        ty = 1;
        break;
    case ']':
        tx = 23;
        ty = 1;
        break;
    case '^':
        tx = 24;
        ty = 1;
        break;
    case '&':
        tx = 25;
        ty = 1;
        break;
    case '%':
        tx = 26;
        ty = 1;
        break;
    case '.':
    case ',':
        tx = 27;
        ty = 1;
        break;
    case '=':
        tx = 28;
        ty = 1;
        break;
    case '$':
        tx = 29;
        ty = 1;
        break;
    case '#':
        tx = 30;
        ty = 1;
        break;
    case '?':
        tx = 3;
        ty = 2;
        break;
    case '*':
        tx = 4;
        ty = 2;
        break;
    default:
        tx = 29;
        ty = 0;
        break;
    }

    *x = tx * bmp_active_skin->properties.textbox_bitmap_font_width;
    *y = ty * bmp_active_skin->properties.textbox_bitmap_font_height;
}

void ui_skinned_textbox_move_relative(GtkWidget *widget, gint x, gint y) {
    g_mutex_lock(mutex);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(widget);
    priv->move_x += x;
    priv->move_y += y;
    g_mutex_unlock(mutex);
}

void ui_skinned_textbox_resize_relative(GtkWidget *widget, gint w, gint h) {
    g_mutex_lock(mutex);
    UiSkinnedTextboxPrivate *priv = UI_SKINNED_TEXTBOX_GET_PRIVATE(widget);
    priv->resize_width += w;
    priv->resize_height += h;
    g_mutex_unlock(mutex);
}
