/*
 * Copyright (C) 2011 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundatoin; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundatoin, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include <xfconf/xfconf.h>
#include <migrate/migrate-config.h>



static guint
migrate_config_strchr_count (const gchar *haystack,
                             const gchar  needle)
{
  const gchar *p;
  guint        count;

  if (G_UNLIKELY (haystack != NULL))
    {
      for (p = haystack, count = 0; *p != '\0'; ++p)
        if (*p == needle)
          count++;
    }

  return count;
}



static void
migrate_config_session_menu (gpointer key,
                             gpointer value,
                             gpointer channel)
{
  const GValue *gvalue = value;
  const gchar  *prop = key;

  /* skip non root plugin properties */
  if (!G_VALUE_HOLDS_STRING (gvalue)
      || migrate_config_strchr_count (prop, G_DIR_SEPARATOR) != 2
      || g_strcmp0 (g_value_get_string (gvalue), "xfsm-logout-plugin") != 0)
    return;

  /* this plugin never had any properties and matches the default
   * settings of the new actions plugin */
  xfconf_channel_set_string (XFCONF_CHANNEL (channel), prop, "actions");
}



static gint
migrate_config_action_48_convert (gint action)
{
  switch (action)
    {
    case 1: /* ACTION_LOG_OUT_DIALOG */
      return 3; /* ACTION_TYPE_LOGOUT_DIALOG */

    case 2: /* ACTION_LOG_OUT */
      return 2; /* ACTION_TYPE_LOGOUT */

    case 3: /* ACTION_LOCK_SCREEN */
      return 5; /* ACTION_TYPE_LOCK_SCREEN */

    case 4: /* ACTION_SHUT_DOWN */
      return 9; /* ACTION_TYPE_SHUTDOWN */

    case 5: /* ACTION_RESTART */
      return 8; /* ACTION_TYPE_RESTART */

    case 6: /* ACTION_SUSPEND */
      return 7; /* ACTION_TYPE_SUSPEND */

    case 7: /* ACTION_HIBERNATE */
      return 6; /* ACTION_TYPE_HIBERNATE */

    default: /* ACTION_DISABLED */
      return -4; /* ACTION_TYPE_SWITCH_USER */
    }
}



static void
migrate_config_action_48 (gpointer key,
                          gpointer value,
                          gpointer channel)
{
  const GValue *gvalue = value;
  const gchar  *prop = key;
  gchar         str[64];
  gint          first_action;
  gint          second_action;


  /* skip non root plugin properties */
  if (!G_VALUE_HOLDS_STRING (gvalue)
      || migrate_config_strchr_count (prop, G_DIR_SEPARATOR) != 2
      || g_strcmp0 (g_value_get_string (gvalue), "actions") != 0)
    return;

  /* read and remove the old properties */
  g_snprintf (str, sizeof (str), "%s/first-action", prop);
  first_action = xfconf_channel_get_uint (channel, str, 0) + 1;
  xfconf_channel_reset_property (channel, str, FALSE);

  g_snprintf (str, sizeof (str), "%s/second-action", prop);
  second_action = xfconf_channel_get_uint (channel, str, 0);
  xfconf_channel_reset_property (channel, str, FALSE);

  /* corrections for new plugin */
  if (first_action == 0)
    first_action = 1;
  if (first_action == second_action)
    second_action = 0;

  /* set appearance to button mode */
  g_snprintf (str, sizeof (str), "%s/appearance", prop);
  xfconf_channel_set_uint (channel, str, 0);

  /* set orientation */
  g_snprintf (str, sizeof (str), "%s/invert-orientation", prop);
  xfconf_channel_set_bool (channel, str, second_action > 0);

  /* convert the old value to new ones */
  first_action = migrate_config_action_48_convert (first_action);
  second_action = migrate_config_action_48_convert (second_action);

  /* set the visible properties */
  g_snprintf (str, sizeof (str), "%s/items", prop);
  xfconf_channel_set_array (channel, str,
                            G_TYPE_INT, &first_action,
                            G_TYPE_INT, &second_action,
                            G_TYPE_INVALID);
}



gboolean
migrate_config (XfconfChannel  *channel,
                gint            configver,
                GError        **error)
{
  GHashTable *plugins;

  plugins = xfconf_channel_get_properties (channel, "/plugins");

  /* migrate plugins to the new actions plugin */
  if (configver < 1)
    {
      /* migrate xfsm-logout-plugin */
      g_hash_table_foreach (plugins, migrate_config_session_menu, channel);

      /* migrate old action plugins */
      g_hash_table_foreach (plugins, migrate_config_action_48, channel);
    }

  return TRUE;
}