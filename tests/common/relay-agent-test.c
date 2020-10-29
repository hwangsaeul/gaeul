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

#include "relay-agent-test.h"

#include "gaeul/relay/relay-application.h"
#include "gaeul/relay/relay-generated.h"

#include <gaeguli/test/receiver.h>

#define SINK_PORT 7777
#define SOURCE_PORT 8888

typedef struct
{
  GObject parent;

  gchar *app_id;

  GThread *relay_thread;
  GMainLoop *loop;

  GApplication *relay_app;
  Gaeul2DBusRelay *relay_proxy;
} GaeulRelayAgentTestPrivate;

/* *INDENT-OFF* */
G_DEFINE_TYPE_WITH_PRIVATE (GaeulRelayAgentTest, gaeul_relay_agent_test,
    G_TYPE_OBJECT)
/* *INDENT-ON* */

void
gaeul_relay_agent_test_add_sink_token (GaeulRelayAgentTest * self,
    const gchar * username)
{
  GaeulRelayAgentTestPrivate *priv =
      gaeul_relay_agent_test_get_instance_private (self);

  g_autoptr (GError) error = NULL;

  g_assert_true (gaeul2_dbus_relay_call_add_sink_token_sync (priv->relay_proxy,
          username, NULL, &error));
  g_assert_no_error (error);
}

void
gaeul_relay_agent_test_add_source_token (GaeulRelayAgentTest * self,
    const gchar * username, const gchar * resource)
{
  GaeulRelayAgentTestPrivate *priv =
      gaeul_relay_agent_test_get_instance_private (self);

  g_autoptr (GError) error = NULL;

  g_assert_true (gaeul2_dbus_relay_call_add_source_token_sync
      (priv->relay_proxy, username, resource, NULL, &error));
  g_assert_no_error (error);
}

static gboolean
quit_relay (GApplication * app)
{
  g_application_quit (app);

  return G_SOURCE_REMOVE;
}

static gboolean
gaeul_relay_agent_test_thread (GaeulRelayAgentTest * self)
{
  GaeulRelayAgentTestPrivate *priv =
      gaeul_relay_agent_test_get_instance_private (self);
  GaeulRelayAgentTestClass *klass = GAEUL_RELAY_AGENT_TEST_GET_CLASS (self);

  g_autoptr (GMainContext) context = g_main_context_new ();
  g_autoptr (GError) error = NULL;
  g_autofree gchar *source_uri = NULL;

  g_main_context_push_thread_default (context);

  priv->relay_proxy =
      gaeul2_dbus_relay_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
      G_DBUS_PROXY_FLAGS_NONE, priv->app_id, "/org/hwangsaeul/Gaeul2/Relay",
      NULL, &error);
  g_assert_no_error (error);

  klass->setup (self);

  priv->loop = g_main_loop_new (context, FALSE);
  g_main_loop_run (priv->loop);

  klass->teardown (self);

  g_idle_add ((GSourceFunc) quit_relay, priv->relay_app);

  return TRUE;
}

static void
on_app_activate (GaeulRelayAgentTest * self)
{
  GaeulRelayAgentTestPrivate *priv =
      gaeul_relay_agent_test_get_instance_private (self);

  priv->relay_thread = g_thread_new ("relay-thread",
      (GThreadFunc) gaeul_relay_agent_test_thread, self);
}

static void
gaeul_relay_agent_test_init (GaeulRelayAgentTest * self)
{
  GaeulRelayAgentTestPrivate *priv =
      gaeul_relay_agent_test_get_instance_private (self);

  priv->app_id =
      g_strdup_printf (GAEUL_RELAY_APPLICATION_SCHEMA_ID "_%d", getpid ());
}

void
gaeul_relay_agent_test_run (GaeulRelayAgentTest * self)
{
  GaeulRelayAgentTestPrivate *priv =
      gaeul_relay_agent_test_get_instance_private (self);

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

  priv->relay_app = G_APPLICATION (g_object_new (GAEUL_TYPE_RELAY_APPLICATION,
          "application-id", priv->app_id, "config-path", config_path,
          "dbus-type", GAEUL_APPLICATION_DBUS_TYPE_SESSION, NULL));
  g_application_hold (priv->relay_app);

  g_signal_connect_swapped (priv->relay_app, "activate",
      (GCallback) on_app_activate, self);

  gaeul_application_run (GAEUL_APPLICATION (priv->relay_app), 0, NULL);
}

void
gaeul_relay_agent_test_remove_sink_token (GaeulRelayAgentTest * self,
    const gchar * username)
{
  GaeulRelayAgentTestPrivate *priv =
      gaeul_relay_agent_test_get_instance_private (self);

  g_autoptr (GError) error = NULL;

  g_assert_true (gaeul2_dbus_relay_call_remove_sink_token_sync
      (priv->relay_proxy, username, TRUE, NULL, &error));
  g_assert_no_error (error);
}

void
gaeul_relay_agent_test_remove_source_token (GaeulRelayAgentTest * self,
    const gchar * username, const gchar * resource)
{
  GaeulRelayAgentTestPrivate *priv =
      gaeul_relay_agent_test_get_instance_private (self);

  g_autoptr (GError) error = NULL;

  g_assert_true (gaeul2_dbus_relay_call_remove_source_token_sync
      (priv->relay_proxy, username, resource, TRUE, NULL, &error));
  g_assert_no_error (error);
}

HwangsaeTestStreamer *
gaeul_relay_agent_test_create_sink (GaeulRelayAgentTest * self)
{
  GaeulRelayAgentTestPrivate *priv =
      gaeul_relay_agent_test_get_instance_private (self);

  HwangsaeTestStreamer *sink;
  g_autofree gchar *sink_uri = NULL;

  g_object_get (priv->relay_proxy, "sink-uri", &sink_uri, NULL);

  sink = hwangsae_test_streamer_new ();
  hwangsae_test_streamer_set_uri (sink, sink_uri);

  return sink;
}

GstElement *
gaeul_relay_agent_test_create_source (GaeulRelayAgentTest * self)
{
  return gaeguli_tests_create_receiver (GAEGULI_SRT_MODE_CALLER, SOURCE_PORT);
}

void
gaeul_relay_agent_test_finish (GaeulRelayAgentTest * self)
{
  GaeulRelayAgentTestPrivate *priv =
      gaeul_relay_agent_test_get_instance_private (self);

  if (g_main_loop_is_running (priv->loop)) {
    g_debug ("Test finished. Releasing relay application...");
    g_main_loop_quit (priv->loop);
  }
}

static void
gaeul_relay_agent_test_dispose (GObject * object)
{
  GaeulRelayAgentTest *self = GAEUL_RELAY_AGENT_TEST (object);
  GaeulRelayAgentTestPrivate *priv =
      gaeul_relay_agent_test_get_instance_private (self);

  g_clear_pointer (&priv->relay_thread, g_thread_join);
  g_clear_pointer (&priv->app_id, g_free);
  g_clear_object (&priv->relay_app);
  g_clear_object (&priv->relay_proxy);
}

static void
gaeul_relay_agent_test_class_init (GaeulRelayAgentTestClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gaeul_relay_agent_test_dispose;
}
