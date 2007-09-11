#include <glib.h>

#include "glib-ext.h"
#include "sys-pedantic.h"

void g_list_string_free(gpointer data, gpointer UNUSED_PARAM(user_data)) {
	g_string_free(data, TRUE);
}

void g_hash_table_string_free(gpointer data) {
	g_string_free(data, TRUE);
}

guint g_hash_table_string_hash(gconstpointer _key) {
	return g_string_hash(_key);
}

gboolean g_hash_table_string_equal(gconstpointer _a, gconstpointer _b) {
	return g_string_equal(_a, _b);
}

gboolean g_hash_table_true(gpointer UNUSED_PARAM(key), gpointer UNUSED_PARAM(value), gpointer UNUSED_PARAM(u)) {
	return TRUE;
}	

GString *g_string_dup(GString *src) {
	GString *dst = g_string_sized_new(src->len);

	g_string_assign(dst, src->str);

	return dst;
}
