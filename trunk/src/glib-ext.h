#ifndef _GLIB_EXT_H_
#define _GLIB_EXT_H_

#include <glib.h>

void     g_list_string_free(gpointer data, gpointer user_data);

gboolean g_hash_table_true(gpointer key, gpointer value, gpointer user_data);
guint    g_hash_table_string_hash(gconstpointer _key);
gboolean g_hash_table_string_equal(gconstpointer _a, gconstpointer _b);
void     g_hash_table_string_free(gpointer data);

GString *g_string_dup(GString *);

gboolean strleq(const gchar *a, gsize a_len, const gchar *b, gsize b_len);

#endif
