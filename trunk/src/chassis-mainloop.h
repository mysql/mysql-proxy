/* Copyright (C) 2007, 2008 MySQL AB
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */ 

#ifndef _CHASSIS_MAINLOOP_H_
#define _CHASSIS_MAINLOOP_H_

#include <glib.h>    /* GPtrArray */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>  /* event.h needs struct tm */
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <event.h>     /* struct event_base */

typedef struct chassis_private chassis_private;
typedef struct chassis chassis;

struct chassis {
	struct event_base *event_base;

	GPtrArray *modules;                       /**< array(chassis_plugin) */

	chassis_private *priv;
	void (*priv_free)(chassis *chas, chassis_private *priv);
};

chassis *chassis_init(void);
void chassis_free(chassis *chas);

/**
 * the mainloop for all chassis apps 
 *
 * can be called directly or as gthread_* functions 
 */
void *chassis_mainloop(void *user_data);

void chassis_set_shutdown(void);
gboolean chassis_is_shutdown(void);

#endif
