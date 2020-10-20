/**
 *  Copyright 2020 SK Telecom Co., Ltd.
 *    Author: Jakub Adam <jakub.adam@collabora.com>
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

#include "gaeul/stream-authenticator.h"

#include <hwangsae/test/test.h>
#include <gio/gio.h>

static void
_set_flag (HwangsaeRelay * relay, HwangsaeCallerDirection direction,
    GInetSocketAddress * addr, const gchar * username, const gchar * resource,
    gpointer data)
{
  *(gboolean *) data = TRUE;
}

#define STREAMER_NAME_VALID "ValidStreamer"
#define STREAMER_NAME_INVALID "InvalidStreamer"
#define RECEIVER_NAME_VALID "ValidReceiver"
#define RECEIVER_NAME_INVALID "InvalidReceiver"

static void
test_gaeul_authenticator (void)
{
  g_autoptr (HwangsaeRelay) relay = hwangsae_relay_new (NULL, 28888, 29999);
  g_autoptr (GaeulStreamAuthenticator) auth =
      gaeul_stream_authenticator_new (relay);
  g_autoptr (HwangsaeTestStreamer) stream = hwangsae_test_streamer_new ();
  g_autoptr (GstElement) receiver = NULL;

  gboolean accepted = FALSE;
  gboolean rejected = FALSE;

  g_object_set (stream, "username", STREAMER_NAME_INVALID, NULL);
  g_object_set (relay, "authentication", TRUE, NULL);

  g_signal_connect (relay, "caller-accepted", (GCallback) _set_flag, &accepted);
  g_signal_connect (relay, "caller-rejected", (GCallback) _set_flag, &rejected);

  hwangsae_test_streamer_set_uri (stream, hwangsae_relay_get_sink_uri (relay));

  hwangsae_relay_start (relay);
  hwangsae_test_streamer_start (stream);

  /* No sink tokens in authenticator - streamer should get rejected. */
  while (!rejected) {
    g_main_context_iteration (NULL, FALSE);
  }

  g_assert_false (accepted);
  accepted = rejected = FALSE;

  /* Good token in authenticator, bad streamer username - still rejected. */
  gaeul_stream_authenticator_add_sink_token (auth, STREAMER_NAME_VALID);

  while (!rejected) {
    g_main_context_iteration (NULL, FALSE);
  }

  hwangsae_test_streamer_stop (stream);

  g_assert_false (accepted);
  accepted = rejected = FALSE;

  /* Valid name in both authenticator and streamer - accepted. */
  g_object_set (stream, "username", STREAMER_NAME_VALID, NULL);
  hwangsae_test_streamer_start (stream);

  while (!accepted) {
    g_main_context_iteration (NULL, FALSE);
  }

  g_assert_false (rejected);
  accepted = rejected = FALSE;

  /* No source tokens in authenticator - rejected. */
  receiver = hwangsae_test_make_receiver (stream, relay, RECEIVER_NAME_INVALID);

  while (!rejected) {
    g_main_context_iteration (NULL, FALSE);
  }

  g_assert_false (accepted);
  accepted = rejected = FALSE;
  gst_element_set_state (receiver, GST_STATE_NULL);
  gst_clear_object (&receiver);

  /* Valid token in authenticator but invalid source name - rejected. */
  gaeul_stream_authenticator_add_source_token (auth, RECEIVER_NAME_VALID,
      STREAMER_NAME_VALID);
  receiver = hwangsae_test_make_receiver (stream, relay, RECEIVER_NAME_INVALID);

  while (!rejected) {
    g_main_context_iteration (NULL, FALSE);
  }

  g_assert_false (accepted);
  accepted = rejected = FALSE;
  gst_element_set_state (receiver, GST_STATE_NULL);
  gst_clear_object (&receiver);

  /* Valid token in authenticator and valid source name - accepted. */
  receiver = hwangsae_test_make_receiver (stream, relay, RECEIVER_NAME_VALID);

  while (!accepted) {
    g_main_context_iteration (NULL, FALSE);
  }

  g_assert_false (rejected);
  accepted = rejected = FALSE;
  gst_element_set_state (receiver, GST_STATE_NULL);
  gst_clear_object (&receiver);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  /* Don't treat warnings as fatal, which is GTest default. */
  g_log_set_always_fatal (G_LOG_FATAL_MASK | G_LOG_LEVEL_CRITICAL);

  g_test_add_func ("/gaeul/authenticator", test_gaeul_authenticator);

  return g_test_run ();
}
