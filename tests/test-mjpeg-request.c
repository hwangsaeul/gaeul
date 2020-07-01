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
#include "gaeul/mjpeg/mjpeg-request.h"

static void
test_gaeul_mjpeg_request_create (void)
{
  g_autoptr (GaeulMjpegRequest) r = NULL;
  GaeulMjpegRequest *tmp = NULL;

  r = gaeul_mjpeg_request_new ("uid", "rid", 100, 100, 1920, 1080, 30);

  g_assert_nonnull (r);

  tmp = gaeul_mjpeg_request_ref (r);

  g_assert_nonnull (tmp);

  gaeul_mjpeg_request_unref (tmp);
}

static void
test_gaeul_mjpeg_request_compare (void)
{
  g_autoptr (GaeulMjpegRequest) r1 = NULL;
  g_autoptr (GaeulMjpegRequest) r2 = NULL;
  g_autoptr (GaeulMjpegRequest) r3 = NULL;

  r1 = gaeul_mjpeg_request_new ("uid1", "rid1", 100, 100, 1920, 1080, 30);
  r2 = gaeul_mjpeg_request_new ("uid", "rid", 100, 100, 1920, 1080, 30);
  r3 = gaeul_mjpeg_request_new ("uid", "rid", 100, 100, 1920, 1080, 30);

  g_assert_false (gaeul_mjpeg_request_equal (r1, r2));
  g_assert_true (gaeul_mjpeg_request_equal (r2, r3));
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  /* Don't treat warnings as fatal, which is GTest default. */
  g_log_set_always_fatal (G_LOG_FATAL_MASK | G_LOG_LEVEL_CRITICAL);

  g_test_add_func ("/gaeul/mjpeg/request-create",
      test_gaeul_mjpeg_request_create);

  g_test_add_func ("/gaeul/mjpeg/request-compare",
      test_gaeul_mjpeg_request_compare);

  return g_test_run ();
}
