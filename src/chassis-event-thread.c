/* $%BEGINLICENSE%$
 Copyright (C) 2007-2008 MySQL AB, 2008 Sun Microsystems, Inc

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 $%ENDLICENSE%$ */

#include <glib.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for write() */
#endif

#include "chassis-event-thread.h"

#define C(x) x, sizeof(x) - 1

/**
 * create a new event-op
 *
 * event-ops are async requests around event_add()
 */
chassis_event_op_t *chassis_event_op_new() {
	chassis_event_op_t *e;

	e = g_new0(chassis_event_op_t, 1);

	return e;
}

/**
 * free a event-op
 */
void chassis_event_op_free(chassis_event_op_t *e) {
	if (!e) return;

	g_free(e);
}

/**
 * execute a event-op on a event-base
 *
 * @see: chassis_event_add_local(), chassis_threaded_event_op()
 */
void chassis_event_op_apply(chassis_event_op_t *op, struct event_base *event_base) {
	switch (op->type) {
	case CHASSIS_EVENT_OP_ADD:
		event_base_set(event_base, op->ev);
		event_add(op->ev, NULL);
		break;
	case CHASSIS_EVENT_OP_UNSET:
		g_assert_not_reached();
		break;
	}
}

/**
 * add a event asynchronously
 *
 * the event is added to the global event-queue and a fd-notification is sent allowing any
 * of the event-threads to handle it
 *
 * @see network_mysqld_con_handle()
 */
void chassis_event_add(chassis *chas, struct event *ev) {
	chassis_event_op_t *op = chassis_event_op_new();

	op->type = CHASSIS_EVENT_OP_ADD;
	op->ev   = ev;
	g_async_queue_push(chas->threads->event_queue, op);

	write(chas->threads->event_notify_fds[1], C(".")); /* ping the event handler */
}

GPrivate *tls_event_base_key = NULL;

/**
 * add a event to the current thread 
 *
 * needs event-base stored in the thread local storage
 *
 * @see network_connection_pool_lua_add_connection()
 */
void chassis_event_add_local(chassis G_GNUC_UNUSED *chas, struct event *ev) {
	struct event_base *event_base = ev->ev_base;
	chassis_event_op_t *op;

	if (!event_base) event_base = g_private_get(tls_event_base_key);

	g_assert(event_base); /* the thread-local event-base has to be initialized */

	op = chassis_event_op_new();

	op->type = CHASSIS_EVENT_OP_ADD;
	op->ev   = ev;

	chassis_event_op_apply(op, event_base);
	
	chassis_event_op_free(op);
}

/**
 * handled events sent through the global event-queue 
 *
 * each event-thread has its own listener on the event-queue and 
 * calls chassis_event_handle() with its own event-base
 *
 * @see chassis_event_add()
 */
void chassis_event_handle(int G_GNUC_UNUSED event_fd, short G_GNUC_UNUSED events, void *user_data) {
	chassis_event_thread_t *event_thread = user_data;
	struct event_base *event_base = event_thread->event_base;
	chassis *chas = event_thread->chas;
	chassis_event_op_t *op;
	char ping[1024];
	guint received = 0;
	gssize removed;

	while ((op = g_async_queue_try_pop(chas->threads->event_queue))) {
		chassis_event_op_apply(op, event_base);

		chassis_event_op_free(op);

		received++;
	}

	/* the pipe has one . per event, remove as many as we received */
	while (received > 0 && 
	       (removed = read(event_thread->notify_fd, ping, MIN(received, sizeof(ping)))) > 0) {
		received -= removed;
	}
}

/**
 * create the data structure for a new event-thread
 */
chassis_event_thread_t *chassis_event_thread_new() {
	chassis_event_thread_t *event_thread;

	event_thread = g_new0(chassis_event_thread_t, 1);

	return event_thread;
}

/**
 * free the data-structures for a event-thread
 *
 * joins the event-thread, closes notification-pipe and free's the event-base
 */
void chassis_event_thread_free(chassis_event_thread_t *event_thread) {
	gboolean is_thread = (event_thread->thr != NULL);

	if (!event_thread) return;

	if (event_thread->thr) g_thread_join(event_thread->thr);

	if (event_thread->notify_fd != -1) {
		event_del(&(event_thread->notify_fd_event));
		close(event_thread->notify_fd);
	}

	/* we don't want to free the global event-base */
	if (is_thread && event_thread->event_base) event_base_free(event_thread->event_base);

	g_free(event_thread);
}

/**
 * set the event-based for the current event-thread
 *
 * @see chassis_event_add_local()
 */
void chassis_event_thread_set_event_base(chassis_event_thread_t G_GNUC_UNUSED *e, struct event_base *event_base) {
	g_private_set(tls_event_base_key, event_base);
}

/**
 * create the event-threads handler
 *
 * provides the event-queue that is contains the event_ops from the event-threads
 * and notifies all the idling event-threads for the new event-ops to process
 */
chassis_event_threads_t *chassis_event_threads_new() {
	chassis_event_threads_t *threads;

	tls_event_base_key = g_private_new(NULL);

	threads = g_new0(chassis_event_threads_t, 1);

	/* create the ping-fds
	 *
	 * the event-thread write a byte to the ping-pipe to trigger a fd-event when
	 * something is available in the event-async-queues
	 */
	if (0 != pipe(threads->event_notify_fds)) {
		g_error("%s: pipe() failed: %s (%d)", 
				G_STRLOC,
				g_strerror(errno),
				errno);
	}

	threads->event_threads = g_ptr_array_new();
	threads->event_queue = g_async_queue_new();

	return threads;
}

/**
 * free all event-threads
 *
 * frees all the registered event-threads and event-queue
 */
void chassis_event_threads_free(chassis_event_threads_t *threads) {
	guint i;
	chassis_event_op_t *op;

	if (!threads) return;

	/* all threads are running, now wait until they are down again */
	for (i = 0; i < threads->event_threads->len; i++) {
		chassis_event_thread_t *event_thread = threads->event_threads->pdata[i];

		chassis_event_thread_free(event_thread);
	}

	g_ptr_array_free(threads->event_threads, TRUE);

	/* free the events that are still in the queue */
	while ((op = g_async_queue_try_pop(threads->event_queue))) {
		chassis_event_op_free(op);
	}
	g_async_queue_unref(threads->event_queue);

	/* close the notification pipe */
	if (threads->event_notify_fds[0] != -1) {
		close(threads->event_notify_fds[0]);
	}
	if (threads->event_notify_fds[1] != -1) {
		close(threads->event_notify_fds[1]);
	}


	g_free(threads);
}

/**
 * add a event-thread to the event-threads handler
 */
void chassis_event_threads_add(chassis_event_threads_t *threads, chassis_event_thread_t *thread) {
	g_ptr_array_add(threads->event_threads, thread);
}


/**
 * setup the notification-fd of a event-thread
 *
 * all event-threads listen on the same notification pipe
 *
 * @see chassis_event_handle()
 */ 
int chassis_event_threads_init_thread(chassis_event_threads_t *threads, chassis_event_thread_t *event_thread, chassis *chas) {
	event_thread->event_base = event_base_new();
	event_thread->chas = chas;

	event_thread->notify_fd = dup(threads->event_notify_fds[0]);

	evutil_make_socket_nonblocking(event_thread->notify_fd);

	event_set(&(event_thread->notify_fd_event), event_thread->notify_fd, EV_READ | EV_PERSIST, chassis_event_handle, event_thread);
	event_base_set(event_thread->event_base, &(event_thread->notify_fd_event));
	event_add(&(event_thread->notify_fd_event), NULL);

	return 0;
}

/**
 * event-handler thread
 *
 */
void *chassis_event_thread_loop(chassis_event_thread_t *event_thread) {
	chassis_event_thread_set_event_base(event_thread, event_thread->event_base);

	/**
	 * check once a second if we shall shutdown the proxy
	 */
	while (!chassis_is_shutdown()) {
		struct timeval timeout;
		int r;

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		g_assert(event_base_loopexit(event_thread->event_base, &timeout) == 0);

		r = event_base_dispatch(event_thread->event_base);

		if (r == -1) {
			if (errno == EINTR) continue;

			break;
		}
	}

	return NULL;
}

/**
 * start all the event-threads 
 *
 * starts all the event-threads that got added by chassis_event_threads_add()
 *
 * @see chassis_event_threads_add
 */
void chassis_event_threads_start(chassis_event_threads_t *threads) {
	guint i;

	g_message("%s: starting %d threads", G_STRLOC, threads->event_threads->len - 1);

	for (i = 1; i < threads->event_threads->len; i++) { /* the 1st is the main-thread and already set up */
		chassis_event_thread_t *event_thread = threads->event_threads->pdata[i];
		GError *gerr = NULL;

		event_thread->thr = g_thread_create((GThreadFunc)chassis_event_thread_loop, event_thread, TRUE, &gerr);

		if (gerr) {
			g_critical("%s: %s", G_STRLOC, gerr->message);
			g_error_free(gerr);
			gerr = NULL;
		}
	}
}


