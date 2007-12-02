#ifndef _CHASSIS_MAINLOOP_H_
#define _CHASSIS_MAINLOOP_H_

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
