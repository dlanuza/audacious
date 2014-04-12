/*
 * interface.c
 * Copyright 2010-2013 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudgui/init.h>
#include <libaudgui/libaudgui.h>

#include "interface.h"
#include "misc.h"
#include "plugin.h"
#include "plugins.h"
#include "visualization.h"

typedef struct {
    const char * name;
    const char * icon;
    MenuFunc func;
} MenuItem;

extern AudAPITable api_table;

static IfacePlugin * current_interface = NULL;

static GList * menu_items[AUD_MENU_COUNT]; /* of MenuItem */

bool_t interface_load (PluginHandle * plugin)
{
    IfacePlugin * i = plugin_get_header (plugin);
    g_return_val_if_fail (i, FALSE);

    audgui_init (& api_table, _AUD_PLUGIN_VERSION);

    if (PLUGIN_HAS_FUNC (i, init) && ! i->init ())
    {
        audgui_cleanup ();
        return FALSE;
    }

    current_interface = i;

    for (int id = 0; id < AUD_MENU_COUNT; id ++)
    {
        for (GList * node = menu_items[id]; node; node = node->next)
        {
            MenuItem * item = node->data;
            audgui_plugin_menu_add (id, item->func, item->name, item->icon);
        }
    }

    return TRUE;
}

void interface_unload (void)
{
    g_return_if_fail (current_interface);

    for (int id = 0; id < AUD_MENU_COUNT; id ++)
    {
        for (GList * node = menu_items[id]; node; node = node->next)
        {
            MenuItem * item = node->data;
            audgui_plugin_menu_remove (id, item->func);
        }
    }

    if (PLUGIN_HAS_FUNC (current_interface, cleanup))
        current_interface->cleanup ();

    current_interface = NULL;

    audgui_cleanup ();
}

void interface_show (bool_t show)
{
    g_return_if_fail (current_interface);

    aud_set_bool (NULL, "show_interface", show);

    if (PLUGIN_HAS_FUNC (current_interface, show))
        current_interface->show (show);

    vis_activate (show);
}

bool_t interface_is_shown (void)
{
    g_return_val_if_fail (current_interface, FALSE);

    return aud_get_bool (NULL, "show_interface");
}

void interface_show_error (const char * message)
{
    if (aud_get_headless_mode ())
        fprintf (stderr, "ERROR: %s\n", message);
    else
        event_queue_full (0, "ui show error", str_get (message), (GDestroyNotify) str_unref);
}

static bool_t probe_cb (PluginHandle * p, PluginHandle * * pp)
{
    * pp = p;
    return FALSE;
}

PluginHandle * iface_plugin_probe (void)
{
    PluginHandle * p = NULL;
    plugin_for_each (PLUGIN_TYPE_IFACE, (PluginForEachFunc) probe_cb, & p);
    return p;
}

static PluginHandle * current_plugin = NULL;

PluginHandle * iface_plugin_get_current (void)
{
    return current_plugin;
}

bool_t iface_plugin_set_current (PluginHandle * plugin)
{
    hook_call ("config save", NULL); /* tell interface to save layout */

    bool_t shown = aud_get_bool (NULL, "show_interface");

    if (current_plugin)
    {
        if (shown && current_interface && PLUGIN_HAS_FUNC (current_interface, show))
            current_interface->show (FALSE);

        AUDDBG ("Unloading %s.\n", plugin_get_name (current_plugin));
        interface_unload ();

        current_plugin = NULL;
    }

    if (plugin)
    {
        AUDDBG ("Loading %s.\n", plugin_get_name (plugin));

        if (! interface_load (plugin))
            return FALSE;

        current_plugin = plugin;

        if (shown && current_interface && PLUGIN_HAS_FUNC (current_interface, show))
            current_interface->show (TRUE);
    }

    vis_activate (shown && current_interface);

    return TRUE;
}

void plugin_menu_add (int id, MenuFunc func, const char * name, const char * icon)
{
    g_return_if_fail (id >= 0 && id < AUD_MENU_COUNT);

    MenuItem * item = g_slice_new (MenuItem);
    item->name = name;
    item->icon = icon;
    item->func = func;

    menu_items[id] = g_list_append (menu_items[id], item);

    if (current_interface)
        audgui_plugin_menu_add (id, func, name, icon);
}

void plugin_menu_remove (int id, MenuFunc func)
{
    g_return_if_fail (id >= 0 && id < AUD_MENU_COUNT);

    if (current_interface)
        audgui_plugin_menu_remove (id, func);

    GList * next;
    for (GList * node = menu_items[id]; node; node = next)
    {
        MenuItem * item = node->data;
        next = node->next;

        if (item->func == func)
        {
            menu_items[id] = g_list_delete_link (menu_items[id], node);
            g_slice_free (MenuItem, item);
        }
    }
}
