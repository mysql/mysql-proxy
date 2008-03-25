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
