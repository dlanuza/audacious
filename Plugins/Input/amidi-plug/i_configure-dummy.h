/*
*
* Author: Giacomo Lozito <james@develia.org>, (C) 2005-2006
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02111-1307  USA
*
*/

#ifndef _I_CONFIGURE_DUMMY_H
#define _I_CONFIGURE_DUMMY_H 1

#include "i_configure_private.h"


extern amidiplug_cfg_backend_t * amidiplug_cfg_backend;

void i_configure_gui_tab_dumm( GtkWidget * , gpointer , gpointer );
void i_configure_gui_tablabel_dumm( GtkWidget * , gpointer , gpointer );
void i_configure_cfg_dumm_alloc( void );
void i_configure_cfg_dumm_free( void );
void i_configure_cfg_dumm_read( pcfg_t * );
void i_configure_cfg_dumm_save( pcfg_t * );

#endif /* !_I_CONFIGURE_DUMMY_H */
