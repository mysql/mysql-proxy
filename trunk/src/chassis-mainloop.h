/* Copyright (C) 2007, 2008 MySQL AB */ 

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
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <event.h>     /* struct event_base */

#include "chassis-exports.h"

typedef struct chassis_private chassis_private;
typedef struct chassis chassis;

struct chassis {
	struct event_base *event_base;

	GPtrArray *modules;                       /**< array(chassis_plugin) */

	chassis_private *priv;
	void (*priv_free)(chassis *chas, chassis_private *priv);
};

CHASSIS_API chassis *chassis_init(void);
CHASSIS_API void chassis_free(chassis *chas);

/**
 * the mainloop for all chassis apps 
 *
 * can be called directly or as gthread_* functions 
 */
CHASSIS_API void *chassis_mainloop(void *user_data);

CHASSIS_API void chassis_set_shutdown(void);
CHASSIS_API gboolean chassis_is_shutdown(void);

#endif
