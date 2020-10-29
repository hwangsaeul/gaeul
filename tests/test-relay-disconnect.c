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

#include "common/relay-agent-test.h"

#include <gaeguli/test/receiver.h>

#define SINK_NAME "ValidSink"
#define SOURCE_NAME "ValidSource"

#define GAEUL_TYPE_RELAY_DISCO_TEST     (gaeul_relay_disco_test_get_type ())
/* *INDENT-OFF* */
G_DECLARE_FINAL_TYPE (GaeulRelayDiscoTest, gaeul_relay_disco_test,
    GAEUL, RELAY_DISCO_TEST, GaeulRelayAgentTest)
/* *INDENT-ON* */

typedef enum
{
  STAGE_WAIT_FOR_BUFFER_1 = 0,
  STAGE_DISCONNECT_SOURCE,
  STAGE_WAIT_FOR_BUFFER_2,
  STAGE_DISCONNECT_SINK,
} Stage;

struct _GaeulRelayDiscoTest
{
  GaeulRelayAgentTest parent;

  HwangsaeTestStreamer *streamer;
  GstElement *receiver;
  GstElement *receiver_srtsrc;

  Stage stage;

  gboolean sink_disconnected;
  gboolean source_disconnected;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulRelayDiscoTest, gaeul_relay_disco_test,
    GAEUL_TYPE_RELAY_AGENT_TEST)
/* *INDENT-ON* */

static void
maybe_finish_test (GaeulRelayDiscoTest * self)
{
  if (self->source_disconnected && self->sink_disconnected) {
    gaeul_relay_agent_test_finish (GAEUL_RELAY_AGENT_TEST (self));
  }
}

gboolean
source_bus_cb (GstBus * bus, GstMessage * message, GaeulRelayDiscoTest * self)
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
          gaeul_relay_agent_test_add_source_token (GAEUL_RELAY_AGENT_TEST
              (self), SOURCE_NAME, SINK_NAME);
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
    GaeulRelayDiscoTest * self)
{
  switch (self->stage) {
    case STAGE_WAIT_FOR_BUFFER_1:{
      g_debug ("%s STAGE_WAIT_FOR_BUFFER_1", __FUNCTION__);
      self->stage = STAGE_DISCONNECT_SOURCE;
      /* Remove source token and force receiver be disconnected. */
      gaeul_relay_agent_test_remove_source_token (GAEUL_RELAY_AGENT_TEST (self),
          SOURCE_NAME, SINK_NAME);
      break;
    }
    case STAGE_DISCONNECT_SOURCE:
      g_debug ("%s STAGE_DISCONNECT_SOURCE", __FUNCTION__);
      break;
    case STAGE_WAIT_FOR_BUFFER_2:
      g_debug ("%s STAGE_WAIT_FOR_BUFFER_2", __FUNCTION__);
      /* Receiver is back. Remove sink token & force disconnect. */
      gaeul_relay_agent_test_remove_sink_token (GAEUL_RELAY_AGENT_TEST (self),
          SINK_NAME);
      self->stage = STAGE_DISCONNECT_SINK;
      break;
    case STAGE_DISCONNECT_SINK:
      g_debug ("%s STAGE_DISCONNECT_SINK", __FUNCTION__);
      break;
  }
}

static void
on_sink_connection_error (GaeulRelayDiscoTest * self, GaeguliTarget * target,
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

static void
gaeul_relay_disco_test_setup (GaeulRelayAgentTest * test)
{
  GaeulRelayDiscoTest *self = GAEUL_RELAY_DISCO_TEST (test);

  g_autoptr (GstBus) bus = NULL;
  g_autoptr (GError) error = NULL;

  gaeul_relay_agent_test_add_sink_token (test, SINK_NAME);
  gaeul_relay_agent_test_add_source_token (test, SOURCE_NAME, SINK_NAME);

  self->streamer = gaeul_relay_agent_test_create_sink (test);
  g_object_set (self->streamer, "username", SINK_NAME, NULL);

  self->receiver = gaeul_relay_agent_test_create_source (test);
  gaeguli_tests_receiver_set_handoff_callback (self->receiver,
      (GCallback) on_buffer_received, self);
  gaeguli_tests_receiver_set_username (self->receiver, SOURCE_NAME, SINK_NAME);

  self->receiver_srtsrc = gst_bin_get_by_name (GST_BIN (self->receiver), "src");

  bus = gst_element_get_bus (self->receiver);
  gst_bus_add_watch (bus, (GstBusFunc) source_bus_cb, self);

  hwangsae_test_streamer_start (self->streamer);
  g_signal_connect_swapped (self->streamer->pipeline, "connection-error",
      (GCallback) on_sink_connection_error, self);
}

static void
gaeul_relay_disco_test_teardown (GaeulRelayAgentTest * test)
{
  GaeulRelayDiscoTest *self = GAEUL_RELAY_DISCO_TEST (test);

  gst_element_set_state (self->receiver, GST_STATE_NULL);
}

static void
gaeul_relay_disco_test_init (GaeulRelayDiscoTest * self)
{
}

static void
gaeul_relay_disco_test_dispose (GObject * object)
{
  GaeulRelayDiscoTest *self = GAEUL_RELAY_DISCO_TEST (object);

  g_clear_object (&self->streamer);
  gst_clear_object (&self->receiver);
  gst_clear_object (&self->receiver_srtsrc);

  G_OBJECT_CLASS (gaeul_relay_disco_test_parent_class)->dispose (object);
}

static void
gaeul_relay_disco_test_class_init (GaeulRelayDiscoTestClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GaeulRelayAgentTestClass *test_class = GAEUL_RELAY_AGENT_TEST_CLASS (klass);

  gobject_class->dispose = gaeul_relay_disco_test_dispose;
  test_class->setup = gaeul_relay_disco_test_setup;
  test_class->teardown = gaeul_relay_disco_test_teardown;
}

static void
test_gaeul_relay_disconnect (void)
{
  g_autoptr (GaeulRelayAgentTest) test =
      g_object_new (GAEUL_TYPE_RELAY_DISCO_TEST, NULL);

  gaeul_relay_agent_test_run (test);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  gst_init (&argc, &argv);

  /* Don't treat warnings as fatal, which is GTest default. */
  g_log_set_always_fatal (G_LOG_FATAL_MASK | G_LOG_LEVEL_CRITICAL);

  g_test_add_func ("/gaeul/relay-disconnect", test_gaeul_relay_disconnect);

  return g_test_run ();
}
