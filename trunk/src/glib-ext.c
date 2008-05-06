/* Copyright (C) 2007, 2008 MySQL AB */ 

#include <glib.h>

#include "glib-ext.h"
#include "sys-pedantic.h"
#include <string.h>

/** @file
 * helper functions for common glib operations
 *
 * - g_list_string_free()
 */


/**
 * free function for GStrings in a GList
 */
void g_list_string_free(gpointer data, gpointer UNUSED_PARAM(user_data)) {
	g_string_free(data, TRUE);
}

/**
 * free function for GStrings in a GHashTable
 */
void g_hash_table_string_free(gpointer data) {
	g_string_free(data, TRUE);
}

/**
 * hash function for GString
 */
guint g_hash_table_string_hash(gconstpointer _key) {
	return g_string_hash(_key);
}

/**
 * compare function for GString
 */
gboolean g_hash_table_string_equal(gconstpointer _a, gconstpointer _b) {
	return g_string_equal(_a, _b);
}

/**
 * true-function for g_hash_table_foreach
 */
gboolean g_hash_table_true(gpointer UNUSED_PARAM(key), gpointer UNUSED_PARAM(value), gpointer UNUSED_PARAM(u)) {
	return TRUE;
}	

gpointer g_hash_table_lookup_const(GHashTable *h, const gchar *name, gsize name_len) {
	GString key;

	key.str = (char *)name; /* we are still const */
	key.len = name_len;

	return g_hash_table_lookup(h, &key);
}


/**
 * duplicate a GString
 */
GString *g_string_dup(GString *src) {
	GString *dst = g_string_sized_new(src->len);

	g_string_assign(dst, src->str);

	return dst;
}

/**
 * compare two strings (gchar arrays), whose lengths are known
 */
gboolean strleq(const gchar *a, gsize a_len, const gchar *b, gsize b_len) {
	if (a_len != b_len) return FALSE;
	return (0 == strcmp(a, b));
}

int g_string_get_time(GString *s, GTimeVal *gt) {
	time_t t = gt->tv_sec;

#ifndef HAVE_GMTIME_R
	static GStaticMutex m = G_STATIC_MUTEX_INIT; /* gmtime() isn't thread-safe */

	g_static_mutex_lock(&m);

	s->len = strftime(s->str, s->allocated_len, "%Y-%m-%dT%H:%M:%S.", gmtime(&(t)));
	
	g_static_mutex_unlock(&m);
#else
	struct tm tm;
	gmtime_r(&(t), &tm);
	s->len = strftime(s->str, s->allocated_len, "%Y-%m-%dT%H:%M:%S.", &tm);
#endif

	/* append microsec + Z */
	g_string_append_printf(s, "%03ldZ", gt->tv_usec / 1000);
	
	return 0;
}

int g_string_get_current_time(GString *s) {
	GTimeVal gt;

	g_get_current_time(&gt);

	return g_string_get_time(s, &gt);
}


GString * g_string_assign_len(GString *s, const char *str, gsize str_len) {
	g_string_truncate(s, 0);
	return g_string_append_len(s, str, str_len);
}

