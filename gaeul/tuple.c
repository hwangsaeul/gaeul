/**
 *  Copyright 2020 SK Telecom Co., Ltd.
 *    Author: Jeongseok Kim <jeongseok.kim@sk.com>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#include "config.h"

#include "tuple.h"

#include <glib.h>

struct _GaeulTuple
{
  GObject parent;

  GHashTable *object_map;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulTuple, gaeul_tuple, G_TYPE_OBJECT)
/* *INDENT-ON* */

static void
gaeul_tuple_dispose (GObject * object)
{
  GaeulTuple *self = GAEUL_TUPLE (object);

  g_clear_pointer (&self->object_map, g_hash_table_destroy);

  G_OBJECT_CLASS (gaeul_tuple_parent_class)->dispose (object);
}

static void
gaeul_tuple_class_init (GaeulTupleClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gaeul_tuple_dispose;
}

static guint
_key_hash_func (GVariant * key)
{
  gchar *k1, *k2;
  g_autofree gchar *concat_key;

  g_variant_get (key, "(ss)", &k1, &k2);
  concat_key = g_strdup_printf ("%s%s", k1, k2);

  return g_str_hash (concat_key);
}

static void
gaeul_tuple_init (GaeulTuple * self)
{
  self->object_map =
      g_hash_table_new_full ((GHashFunc) _key_hash_func, g_variant_equal,
      (GDestroyNotify) g_variant_unref, g_object_unref);
}

GaeulTuple *
gaeul_tuple_new (void)
{
  return g_object_new (GAEUL_TYPE_TUPLE, NULL);
}

gboolean
gaeul_tuple_insert (GaeulTuple * self,
    const gchar * first_key, const gchar * second_key, GObject * object)
{
  g_autoptr (GVariant) variant_key = NULL;

  g_return_val_if_fail (GAEUL_IS_TUPLE (self), FALSE);
  g_return_val_if_fail (first_key != NULL, FALSE);
  g_return_val_if_fail (second_key != NULL, FALSE);
  g_return_val_if_fail (object != NULL, FALSE);

  variant_key = g_variant_new ("(ss)", first_key, second_key);

  if (g_hash_table_lookup (self->object_map, variant_key) != NULL) {
    return FALSE;
  }

  g_hash_table_insert (self->object_map, g_steal_pointer (&variant_key),
      g_object_ref (object));

  return TRUE;
}

gboolean
gaeul_tuple_remove (GaeulTuple * self,
    const gchar * first_key, const gchar * second_key)
{
  g_autoptr (GVariant) variant_key = NULL;

  g_return_val_if_fail (GAEUL_IS_TUPLE (self), FALSE);
  g_return_val_if_fail (first_key != NULL, FALSE);
  g_return_val_if_fail (second_key != NULL, FALSE);

  variant_key = g_variant_new ("(ss)", first_key, second_key);

  if (g_hash_table_lookup (self->object_map, variant_key) == NULL) {
    return FALSE;
  }

  return g_hash_table_remove (self->object_map, variant_key);
}

GObject *
gaeul_tuple_lookup (GaeulTuple * self, const gchar * first_key,
    const gchar * second_key)
{
  g_autoptr (GVariant) variant_key = NULL;

  g_return_val_if_fail (GAEUL_IS_TUPLE (self), FALSE);
  g_return_val_if_fail (first_key != NULL, FALSE);
  g_return_val_if_fail (second_key != NULL, FALSE);

  variant_key = g_variant_new ("(ss)", first_key, second_key);

  return g_hash_table_lookup (self->object_map, variant_key);
}
