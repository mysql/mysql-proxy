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

