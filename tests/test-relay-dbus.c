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

/*
 * Checks adding and removing authentication tokens in the following scenario:
 *
 * 1. add sink and source tokens to the relay agent
 * 2. let a sink and a source connect to the relay
 * 3. wait until the source receives a video frame
 * 4. remove source token from the relay and force drop of  pending connections
 * 5. check that the source gets disconnected
 * 6. put the source token back
 * 7. let the source reconnect
 * 8. remove sink token
 * 9. check that the sink AND the source are both disconnected
 */

#include "gaeul/relay/relay-application.h"
#include "gaeul/relay/relay-generated.h"

#include <gaeguli/test/receiver.h>
#include <hwangsae/test/test.h>

#define SINK_NAME "ValidSink"
#define SOURCE_NAME "ValidSource"

#define SINK_PORT 7777
#define SOURCE_PORT 8888

#define GAEUL_TYPE_RELAY_DBUS_TEST     (gaeul_relay_dbus_test_get_type ())
/* *INDENT-OFF* */
G_DECLARE_FINAL_TYPE (GaeulRelayDBusTest, gaeul_relay_dbus_test, GAEUL, RELAY_DBUS_TEST, GObject)
/* *INDENT-ON* */

typedef enum
{
  STAGE_WAIT_FOR_BUFFER_1 = 0,
  STAGE_DISCONNECT_SOURCE,
  STAGE_WAIT_FOR_BUFFER_2,
  STAGE_DISCONNECT_SINK,
} Stage;

struct _GaeulRelayDBusTest
{
  GObject parent;

  gchar *app_id;

  GThread *relay_thread;
  GMainLoop *loop;

  GApplication *relay_app;
  Gaeul2DBusRelay *relay_proxy;

  GstElement *receiver_srtsrc;

  Stage stage;

  gboolean sink_disconnected;
  gboolean source_disconnected;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulRelayDBusTest, gaeul_relay_dbus_test, G_TYPE_OBJECT)
/* *INDENT-ON* */

static void
dbus_add_sink_token (Gaeul2DBusRelay * relay_proxy)
{
  g_autoptr (GError) error = NULL;

  g_assert_true (gaeul2_dbus_relay_call_add_sink_token_sync (relay_proxy,
          SINK_NAME, NULL, &error));
  g_assert_no_error (error);
}

static void
dbus_add_source_token (Gaeul2DBusRelay * relay_proxy)
{
  g_autoptr (GError) error = NULL;

  g_assert_true (gaeul2_dbus_relay_call_add_source_token_sync (relay_proxy,
          SOURCE_NAME, SINK_NAME, NULL, &error));
  g_assert_no_error (error);
}

static void
maybe_finish_test (GaeulRelayDBusTest * self)
{
  if (self->source_disconnected && self->sink_disconnected) {
    g_debug ("Test finished. Releasing relay application...");
    g_main_loop_quit (self->loop);
  }
}

gboolean
source_bus_cb (GstBus * bus, GstMessage * message, GaeulRelayDBusTest * self)
{
  if (message->type == GST_MESSAGE_WARNING) {
    g_autoptr (GError) error = NULL;

    gst_message_parse_warning (message, &error, NULL);
    if (message->src == GST_OBJECT (self->receiver_srtsrc) &&
        g_error_matches (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_READ)) {
      switch (self->stage) {
        case STAGE_WAIT_FOR_BUFFER_1:
          g_debug ("%s STAGE_WAIT_FOR_BUFFER_1", __FUNCTION__);
          break;
        case STAGE_DISCONNECT_SOURCE:
          g_debug ("%s STAGE_DISCONNECT_SOURCE", __FUNCTION__);
          /* Receiver disconnected. Put receiver token back and wait for
           * receiver reconnection. */
          self->stage = STAGE_WAIT_FOR_BUFFER_2;
          dbus_add_source_token (self->relay_proxy);
          break;
        case STAGE_WAIT_FOR_BUFFER_2:
          g_debug ("%s STAGE_WAIT_FOR_BUFFER_2", __FUNCTION__);
          break;
        case STAGE_DISCONNECT_SINK:
          g_debug ("%s STAGE_DISCONNECT_SINK", __FUNCTION__);
          self->source_disconnected = TRUE;
          maybe_finish_test (self);
          break;
      }
    }
  }

  return G_SOURCE_CONTINUE;
}

static void
on_buffer_received (GstElement * object, GstBuffer * buffer, GstPad * pad,
    GaeulRelayDBusTest * self)
{
  g_autoptr (GError) error = NULL;

  switch (self->stage) {
    case STAGE_WAIT_FOR_BUFFER_1:{
      g_debug ("%s STAGE_WAIT_FOR_BUFFER_1", __FUNCTION__);
      self->stage = STAGE_DISCONNECT_SOURCE;
      /* Remove source token and force receiver be disconnected. */
      g_assert_true (gaeul2_dbus_relay_call_remove_source_token_sync
          (self->relay_proxy, SOURCE_NAME, SINK_NAME, TRUE, NULL, &error));
      g_assert_no_error (error);
      break;
    }
    case STAGE_DISCONNECT_SOURCE:
      g_debug ("%s STAGE_DISCONNECT_SOURCE", __FUNCTION__);
      break;
    case STAGE_WAIT_FOR_BUFFER_2:
      g_debug ("%s STAGE_WAIT_FOR_BUFFER_2", __FUNCTION__);
      /* Receiver is back. Remove sink token & force disconnect. */
      g_assert_true (gaeul2_dbus_relay_call_remove_sink_token_sync
          (self->relay_proxy, SINK_NAME, TRUE, NULL, &error));
      g_assert_no_error (error);
      self->stage = STAGE_DISCONNECT_SINK;
      break;
    case STAGE_DISCONNECT_SINK:
      g_debug ("%s STAGE_DISCONNECT_SINK", __FUNCTION__);
      break;
  }
}

static void
on_sink_connection_error (GaeulRelayDBusTest * self, GaeguliTarget * target,
    GError * error)
{
  switch (self->stage) {
    case STAGE_WAIT_FOR_BUFFER_1:
      g_debug ("%s STAGE_WAIT_FOR_BUFFER_1", __FUNCTION__);
      break;
    case STAGE_DISCONNECT_SOURCE:
      g_debug ("%s STAGE_DISCONNECT_SOURCE", __FUNCTION__);
      g_assert_not_reached ();
      break;
    case STAGE_WAIT_FOR_BUFFER_2:
      g_debug ("%s STAGE_WAIT_FOR_BUFFER_2", __FUNCTION__);
      g_assert_not_reached ();
      break;
    case STAGE_DISCONNECT_SINK:
      g_debug ("%s STAGE_DISCONNECT_SINK", __FUNCTION__);
      self->sink_disconnected = TRUE;
      maybe_finish_test (self);
      break;
  }
}

static gboolean
quit_relay (GApplication * app)
{
  g_application_quit (app);

  return G_SOURCE_REMOVE;
}

static gboolean
gaeul_relay_dbus_test_thread (GaeulRelayDBusTest * self)
{
  g_autoptr (GMainContext) context = g_main_context_new ();
  g_autoptr (HwangsaeTestStreamer) streamer = NULL;
  g_autoptr (GstBus) bus = NULL;
  g_autoptr (GstElement) receiver = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree gchar *sink_uri = NULL;
  g_autofree gchar *source_uri = NULL;

  g_main_context_push_thread_default (context);

  self->relay_proxy =
      gaeul2_dbus_relay_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
      G_DBUS_PROXY_FLAGS_NONE, self->app_id, "/org/hwangsaeul/Gaeul2/Relay",
      NULL, &error);
  g_assert_no_error (error);

  dbus_add_sink_token (self->relay_proxy);
  dbus_add_source_token (self->relay_proxy);

  g_object_get (self->relay_proxy, "sink-uri", &sink_uri,
      "source-uri", &source_uri, NULL);

  streamer = hwangsae_test_streamer_new ();
  hwangsae_test_streamer_set_uri (streamer, sink_uri);
  g_object_set (streamer, "username", SINK_NAME, NULL);

  receiver = gaeguli_tests_create_receiver (GAEGULI_SRT_MODE_CALLER,
      SOURCE_PORT);
  gaeguli_tests_receiver_set_handoff_callback (receiver,
      (GCallback) on_buffer_received, self);
  gaeguli_tests_receiver_set_username (receiver, SOURCE_NAME, SINK_NAME);

  self->receiver_srtsrc = gst_bin_get_by_name (GST_BIN (receiver), "src");

  bus = gst_element_get_bus (receiver);
  gst_bus_add_watch (bus, (GstBusFunc) source_bus_cb, self);

  hwangsae_test_streamer_start (streamer);
  g_signal_connect_swapped (streamer->pipeline, "connection-error",
      (GCallback) on_sink_connection_error, self);

  self->loop = g_main_loop_new (context, FALSE);
  g_main_loop_run (self->loop);

  gst_element_set_state (receiver, GST_STATE_NULL);

  g_idle_add ((GSourceFunc) quit_relay, self->relay_app);

  return TRUE;
}

static void
on_app_activate (GaeulRelayDBusTest * self)
{
  self->relay_thread = g_thread_new ("relay-thread",
      (GThreadFunc) gaeul_relay_dbus_test_thread, self);
}

static void
gaeul_relay_dbus_test_init (GaeulRelayDBusTest * self)
{
  self->app_id =
      g_strdup_printf (GAEUL_RELAY_APPLICATION_SCHEMA_ID "_%d", getpid ());
}

static void
gaeul_relay_dbus_test_run (GaeulRelayDBusTest * self)
{
  g_autofree gchar *config_path = NULL;
  g_autofree gchar *config = NULL;
  g_autoptr (GError) error = NULL;

/* *INDENT-OFF* */
  config = g_strdup_printf ("[org/hwangsaeul/Gaeul2/Relay]\n"
      "sink-port=%d\n"
      "source-port=%d\n"
      "authentication=true", SINK_PORT, SOURCE_PORT);
/* *INDENT-ON* */

  config_path = g_build_filename (g_get_tmp_dir (), "relay-XXXXXX.conf", NULL);
  g_file_set_contents (config_path, config, strlen (config), &error);
  g_assert_no_error (error);

  self->relay_app = G_APPLICATION (g_object_new (GAEUL_TYPE_RELAY_APPLICATION,
          "application-id", self->app_id, "config-path", config_path,
          "dbus-type", GAEUL_APPLICATION_DBUS_TYPE_SESSION, NULL));
  g_application_hold (self->relay_app);

  g_signal_connect_swapped (self->relay_app, "activate",
      (GCallback) on_app_activate, self);

  gaeul_application_run (GAEUL_APPLICATION (self->relay_app), 0, NULL);
}

static void
gaeul_relay_dbus_test_dispose (GObject * object)
{
  GaeulRelayDBusTest *self = GAEUL_RELAY_DBUS_TEST (object);

  g_clear_pointer (&self->relay_thread, g_thread_join);
  g_clear_pointer (&self->app_id, g_free);
  g_clear_object (&self->relay_app);
  g_clear_object (&self->relay_proxy);
  gst_clear_object (&self->receiver_srtsrc);
}

static void
gaeul_relay_dbus_test_class_init (GaeulRelayDBusTestClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gaeul_relay_dbus_test_dispose;
}

static void
test_gaeul_remove_token_with_disconnect (void)
{
  g_autoptr (GaeulRelayDBusTest) test =
      g_object_new (GAEUL_TYPE_RELAY_DBUS_TEST, NULL);

  gaeul_relay_dbus_test_run (test);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  gst_init (&argc, &argv);

  /* Don't treat warnings as fatal, which is GTest default. */
  g_log_set_always_fatal (G_LOG_FATAL_MASK | G_LOG_LEVEL_CRITICAL);

  g_test_add_func ("/gaeul/remove-token-with-disconnect",
      test_gaeul_remove_token_with_disconnect);

  return g_test_run ();
}
