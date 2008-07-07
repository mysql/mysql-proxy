/* Copyright (C) 2007, 2008 MySQL AB */ 

#ifndef _GLIB_EXT_H_
#define _GLIB_EXT_H_

#include <glib.h>

#include "chassis-exports.h"

CHASSIS_API void     g_list_string_free(gpointer data, gpointer user_data);

CHASSIS_API gboolean g_hash_table_true(gpointer key, gpointer value, gpointer user_data);
CHASSIS_API guint    g_hash_table_string_hash(gconstpointer _key);
CHASSIS_API gboolean g_hash_table_string_equal(gconstpointer _a, gconstpointer _b);
CHASSIS_API void     g_hash_table_string_free(gpointer data);
CHASSIS_API gpointer g_hash_table_lookup_const(GHashTable *h, const gchar *name, gsize name_len);

CHASSIS_API GString *g_string_dup(GString *);

CHASSIS_API gboolean strleq(const gchar *a, gsize a_len, const gchar *b, gsize b_len);
CHASSIS_API gboolean g_string_equal_ci(const GString *a, const GString *b);

CHASSIS_API int g_string_get_time(GString *s, GTimeVal *gt);
CHASSIS_API int g_string_get_current_time(GString *s);
CHASSIS_API GString *g_string_assign_len(GString *s, const char *, gsize );
CHASSIS_API void g_debug_hexdump(const char *msg, const void *s, size_t len);

#endif
