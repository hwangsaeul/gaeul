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

#include "gaeul/tuple.h"

/* *INDENT-OFF* */
#define GAEUL_TYPE_TEST_OBJECT  (gaeul_test_object_get_type ())
G_DECLARE_FINAL_TYPE            (GaeulTestObject, gaeul_test_object, GAEUL, TEST_OBJECT, GObject)
/* *INDENT-ON* */

struct _GaeulTestObject
{
  GObject parent;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulTestObject, gaeul_test_object, G_TYPE_OBJECT)
/* *INDENT-ON* */

static void
gaeul_test_object_class_init (GaeulTestObjectClass * klass)
{
}

static void
gaeul_test_object_init (GaeulTestObject * self)
{
}

static void
test_gaeul_tuple_insert_remove (void)
{
  g_autoptr (GaeulTuple) tuple = gaeul_tuple_new ();
  g_autoptr (GaeulTestObject) obj = g_object_new (GAEUL_TYPE_TEST_OBJECT, NULL);

  g_assert_true (gaeul_tuple_insert (tuple, "key1", "key2", G_OBJECT (obj)));
  g_assert_false (gaeul_tuple_insert (tuple, "key1", "key2", G_OBJECT (obj)));

  g_assert_true (gaeul_tuple_remove (tuple, "key1", "key2"));
  g_assert_false (gaeul_tuple_remove (tuple, "key1", "key2"));
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  /* Don't treat warnings as fatal, which is GTest default. */
  g_log_set_always_fatal (G_LOG_FATAL_MASK | G_LOG_LEVEL_CRITICAL);

  g_test_add_func ("/gaeul/tuple-insert-remove",
      test_gaeul_tuple_insert_remove);

  return g_test_run ();
}
