/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2000  Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front Technologies
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1307, USA.
 */
#ifndef XMMS_CONFIGFILE_H
#define XMMS_CONFIGFILE_H

#include <glib.h>


typedef enum {
    XMMS_CFG_INT,
    XMMS_CFG_FLOAT,
    XMMS_CFG_BOOLEAN,
    XMMS_CFG_STRING
} XmmsCfgValueType;


struct _ConfigLine {
    gchar *key;
    gchar *value;
};

struct _ConfigSection {
    gchar *name;
    GList *lines;
};

struct _ConfigFile {
    GList *sections;
};

typedef struct _ConfigLine ConfigLine;
typedef struct _ConfigSection ConfigSection;
typedef struct _ConfigFile ConfigFile;

/*
 * Poison all of the xmms ConfigFile interfaces as we are to
 * no longer use these anymore. They have been deprecated since 0.2,
 * and will be REMOVED in 1.2.
 *
 * If you are a third party developer and you need a quick fix, add
 *   #define I_AM_A_THIRD_PARTY_DEVELOPER_WHO_NEEDS_TO_BE_KICKED_IN_THE_HEAD_BY_CHUCK_NORRIS
 * to your code.
 *
 * - nenolod, 05/09/2006
 */
#ifndef I_AM_A_THIRD_PARTY_DEVELOPER_WHO_NEEDS_TO_BE_KICKED_IN_THE_HEAD_BY_CHUCK_NORRIS
#pragma GCC poison xmms_cfg_new
#pragma GCC poison xmms_cfg_open_file
#pragma GCC poison xmms_cfg_write_file
#pragma GCC poison xmms_cfg_open_default_file
#pragma GCC poison xmms_cfg_write_default_file
#pragma GCC poison xmms_cfg_read_value
#pragma GCC poison xmms_cfg_write_value
#pragma GCC poison xmms_cfg_read_string
#pragma GCC poison xmms_cfg_read_int
#pragma GCC poison xmms_cfg_read_boolean
#pragma GCC poison xmms_cfg_read_float
#pragma GCC poison xmms_cfg_read_double
#pragma GCC poison xmms_cfg_write_string
#pragma GCC poison xmms_cfg_write_int
#pragma GCC poison xmms_cfg_write_boolean
#pragma GCC poison xmms_cfg_write_float
#pragma GCC poison xmms_cfg_write_double
#pragma GCC poison xmms_cfg_remove_key
#endif

#ifdef __cplusplus
extern "C" {
#endif

    ConfigFile *xmms_cfg_new(void);
    ConfigFile *xmms_cfg_open_file(const gchar * filename);
    gboolean xmms_cfg_write_file(ConfigFile * cfg, const gchar * filename);
    void xmms_cfg_free(ConfigFile * cfg);
    ConfigFile *xmms_cfg_open_default_file(void);
    gboolean xmms_cfg_write_default_file(ConfigFile * cfg);

    gboolean xmms_cfg_read_value(ConfigFile * config_file,
                                 const gchar * section, const gchar * key,
                                 XmmsCfgValueType value_type,
                                 gpointer * value);

    void xmms_cfg_write_value(ConfigFile * config_file,
                              const gchar * section, const gchar * key,
                              XmmsCfgValueType value_type,
                              gpointer * value);

    gboolean xmms_cfg_read_string(ConfigFile * cfg, const gchar * section,
                                  const gchar * key, gchar ** value);
    gboolean xmms_cfg_read_int(ConfigFile * cfg, const gchar * section,
                               const gchar * key, gint * value);
    gboolean xmms_cfg_read_boolean(ConfigFile * cfg, const gchar * section,
                                   const gchar * key, gboolean * value);
    gboolean xmms_cfg_read_float(ConfigFile * cfg, const gchar * section,
                                 const gchar * key, gfloat * value);
    gboolean xmms_cfg_read_double(ConfigFile * cfg, const gchar * section,
                                  const gchar * key, gdouble * value);

    void xmms_cfg_write_string(ConfigFile * cfg, const gchar * section,
                               const gchar * key, gchar * value);
    void xmms_cfg_write_int(ConfigFile * cfg, const gchar * section,
                            const gchar * key, gint value);
    void xmms_cfg_write_boolean(ConfigFile * cfg, const gchar * section,
                                const gchar * key, gboolean value);
    void xmms_cfg_write_float(ConfigFile * cfg, const gchar * section,
                              const gchar * key, gfloat value);
    void xmms_cfg_write_double(ConfigFile * cfg, const gchar * section,
                               const gchar * key, gdouble value);

    void xmms_cfg_remove_key(ConfigFile * cfg, const gchar * section,
                             const gchar * key);

#ifdef __cplusplus
};
#endif

#endif
