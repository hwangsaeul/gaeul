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

#include "common/relay-agent-test.h"

#include <gaeguli/test/receiver.h>

#define SINK_NAME "Sink"
#define SOURCE1_NAME "SourceOne"
#define SOURCE2_NAME "SourceTwo"

#define GAEUL_TYPE_RELAY_REROUTE_TEST     (gaeul_relay_reroute_test_get_type ())
/* *INDENT-OFF* */
G_DECLARE_FINAL_TYPE (GaeulRelayRerouteTest, gaeul_relay_reroute_test,
    GAEUL, RELAY_REROUTE_TEST, GaeulRelayAgentTest)
/* *INDENT-ON* */

typedef enum
{
  STAGE_SRC1_RECEIVING = 0,
  STAGE_SRC2_RECEIVING,
} Stage;

struct _GaeulRelayRerouteTest
{
  GaeulRelayAgentTest parent;

  HwangsaeTestStreamer *streamer;
  GstElement *receiver1;
  GstElement *receiver1_src;
  GstElement *receiver2;

  Stage stage;

  gboolean receiver1_disconnected;
  gboolean receiver2_connected;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulRelayRerouteTest, gaeul_relay_reroute_test,
    GAEUL_TYPE_RELAY_AGENT_TEST)
/* *INDENT-ON* */

static void
gaeul_relay_reroute_test_maybe_finish (GaeulRelayRerouteTest * self)
{
  if (self->receiver1_disconnected && self->receiver2_connected) {
    gaeul_relay_agent_test_finish (GAEUL_RELAY_AGENT_TEST (self));
  }
}

gboolean
src1_bus_cb (GstBus * bus, GstMessage * message, GaeulRelayRerouteTest * self)
{
  if (message->type == GST_MESSAGE_WARNING) {
    g_autoptr (GError) error = NULL;

    gst_message_parse_warning (message, &error, NULL);
    if (message->src == GST_OBJECT (self->receiver1_src) &&
        g_error_matches (error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_READ)) {

      g_debug ("%s stage %d", __FUNCTION__, self->stage);

      switch (self->stage) {
        case STAGE_SRC1_RECEIVING:
          break;
        case STAGE_SRC2_RECEIVING:
          self->receiver1_disconnected = TRUE;
          gaeul_relay_reroute_test_maybe_finish (self);
          break;
      }
    }
  }

  return G_SOURCE_CONTINUE;
}

static void
src1_on_buffer_received (GstElement * object, GstBuffer * buffer, GstPad * pad,
    GaeulRelayRerouteTest * self)
{
  g_debug ("%s stage %d", __FUNCTION__, self->stage);

  switch (self->stage) {
    case STAGE_SRC1_RECEIVING:{
      Gaeul2DBusRelay *proxy =
          gaeul_relay_agent_test_get_dbus_proxy (GAEUL_RELAY_AGENT_TEST (self));
      g_autoptr (GError) error = NULL;

      self->stage = STAGE_SRC2_RECEIVING;

      g_assert_true (gaeul2_dbus_relay_call_reroute_source_sync (proxy,
              SOURCE1_NAME, SINK_NAME, SOURCE2_NAME, NULL, &error));
      g_assert_no_error (error);
      break;
    }
    case STAGE_SRC2_RECEIVING:
      break;
  }
}

static void
src2_on_buffer_received (GstElement * object, GstBuffer * buffer, GstPad * pad,
    GaeulRelayRerouteTest * self)
{
  g_debug ("%s stage %d", __FUNCTION__, self->stage);

  switch (self->stage) {
    case STAGE_SRC1_RECEIVING:
      break;
    case STAGE_SRC2_RECEIVING:
      self->receiver2_connected = TRUE;
      gaeul_relay_reroute_test_maybe_finish (self);
      break;
  }
}

static void
gaeul_relay_reroute_test_setup (GaeulRelayAgentTest * test)
{
  GaeulRelayRerouteTest *self = GAEUL_RELAY_REROUTE_TEST (test);

  g_autoptr (GstBus) bus = NULL;
  g_autoptr (GError) error = NULL;

  gaeul_relay_agent_test_add_sink_token (test, SINK_NAME);
  gaeul_relay_agent_test_add_source_token (test, SOURCE1_NAME, SINK_NAME);

  self->streamer = gaeul_relay_agent_test_create_sink (test);
  g_object_set (self->streamer, "username", SINK_NAME, NULL);

  self->receiver1 = gaeul_relay_agent_test_create_source (test);
  self->receiver1_src = gst_bin_get_by_name (GST_BIN (self->receiver1), "src");
  gaeguli_tests_receiver_set_handoff_callback (self->receiver1,
      (GCallback) src1_on_buffer_received, self);
  gaeguli_tests_receiver_set_username (self->receiver1, SOURCE1_NAME,
      SINK_NAME);
  bus = gst_element_get_bus (self->receiver1);
  gst_bus_add_watch (bus, (GstBusFunc) src1_bus_cb, self);

  self->receiver2 = gaeul_relay_agent_test_create_source (test);
  gaeguli_tests_receiver_set_handoff_callback (self->receiver2,
      (GCallback) src2_on_buffer_received, self);
  gaeguli_tests_receiver_set_username (self->receiver2, SOURCE2_NAME,
      SINK_NAME);

  hwangsae_test_streamer_start (self->streamer);
}

static void
gaeul_relay_reroute_test_teardown (GaeulRelayAgentTest * test)
{
  GaeulRelayRerouteTest *self = GAEUL_RELAY_REROUTE_TEST (test);

  gst_element_set_state (self->receiver1, GST_STATE_NULL);
  gst_element_set_state (self->receiver2, GST_STATE_NULL);
}

static void
gaeul_relay_reroute_test_init (GaeulRelayRerouteTest * self)
{
}

static void
gaeul_relay_reroute_test_dispose (GObject * object)
{
  GaeulRelayRerouteTest *self = GAEUL_RELAY_REROUTE_TEST (object);

  g_clear_object (&self->streamer);
  gst_clear_object (&self->receiver1);
  gst_clear_object (&self->receiver1_src);
  gst_clear_object (&self->receiver2);

  G_OBJECT_CLASS (gaeul_relay_reroute_test_parent_class)->dispose (object);
}

static void
gaeul_relay_reroute_test_class_init (GaeulRelayRerouteTestClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GaeulRelayAgentTestClass *test_class = GAEUL_RELAY_AGENT_TEST_CLASS (klass);

  gobject_class->dispose = gaeul_relay_reroute_test_dispose;
  test_class->setup = gaeul_relay_reroute_test_setup;
  test_class->teardown = gaeul_relay_reroute_test_teardown;
}

static void
test_gaeul_relay_reroute (void)
{
  g_autoptr (GaeulRelayAgentTest) test =
      g_object_new (GAEUL_TYPE_RELAY_REROUTE_TEST, NULL);

  gaeul_relay_agent_test_run (test);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  gst_init (&argc, &argv);

  /* Don't treat warnings as fatal, which is GTest default. */
  g_log_set_always_fatal (G_LOG_FATAL_MASK | G_LOG_LEVEL_CRITICAL);

  g_test_add_func ("/gaeul/relay-reroute", test_gaeul_relay_reroute);

  return g_test_run ();
}
