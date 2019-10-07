/**
 *  Copyright 2019 SK Telecom Co., Ltd.
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

#include "gaeul-agent.h"
#include "dbus/dbus-manager-generated.h"

#include <glib-unix.h>

#include <chamge/chamge.h>
#include <gaeguli/gaeguli.h>

#define GAEUL_SCHEMA_ID "org.hwangsaeul.Gaeul"

/* *INDENT-OFF* */
#if !GLIB_CHECK_VERSION(2,57,1)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GEnumClass, g_type_class_unref)
#endif
/* *INDENT-ON* */

struct _GaeulAgent
{
  GApplication parent;
  GaeulDBusManager *dbus_manager;

  GSettings *settings;
  ChamgeEdge *edge;
  gulong edge_state_changed_id;
  ChamgeNodeState edge_prev_state;
  GaeguliPipeline *pipeline;
  GaeguliFifoTransmit *transmit;

  gchar *srt_target_uri;
  guint target_stream_id;
  guint transmit_id;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulAgent, gaeul_agent, G_TYPE_APPLICATION)
/* *INDENT-ON* */

#define DBUS_STATE_PAUSED       0
#define DBUS_STATE_PLAYING      1
#define DBUS_STATE_RECORDING    2

static gboolean
_start_pipeline (GaeulAgent * self)
{
  g_autoptr (GError) error = NULL;

  g_debug ("edge is activated.");
  /* SRT connection uri is available only when activated */

  g_free (self->srt_target_uri);

  /* TODO: request uri from chamge 
   *
   * self->srt_target_uri = chamge_edge_request_target_uri (self->edge, &error);
   */

  self->srt_target_uri =
      g_settings_get_string (self->settings, "default-srt-target-uri");

  if (self->srt_target_uri == NULL) {
    g_warning ("failed to get srt connection uri");
  }

  g_debug ("connect to (uri: %s)", self->srt_target_uri);

  self->transmit_id =
      gaeguli_fifo_transmit_start (self->transmit, "172.30.254.3", 8888,
      GAEGULI_SRT_MODE_CALLER, &error);

  self->target_stream_id =
      gaeguli_pipeline_add_fifo_target_full (self->pipeline,
      GAEGULI_VIDEO_CODEC_H264, GAEGULI_VIDEO_RESOLUTION_640x480,
      gaeguli_fifo_transmit_get_fifo (self->transmit), &error);

  g_debug ("start stream to fifo (id: %u)", self->target_stream_id);

  gaeul_dbus_manager_set_state (self->dbus_manager, DBUS_STATE_PLAYING);

  return G_SOURCE_REMOVE;
}

static gboolean
_edge_activate (GaeulAgent * self)
{
  chamge_node_activate (CHAMGE_NODE (self->edge));

  return G_SOURCE_REMOVE;
}

static void
_edge_state_changed_cb (ChamgeEdge * edge, ChamgeNodeState state,
    GaeulAgent * self)
{
  switch (state) {
    case CHAMGE_NODE_STATE_NULL:
      g_debug ("edge goes NULL");
      break;
    case CHAMGE_NODE_STATE_ENROLLED:
      if (self->edge_prev_state == CHAMGE_NODE_STATE_NULL) {
        g_debug ("edge is enrolled.");
        /* FIXME: different context or dispatcher is required to avoid blocking */
        g_idle_add ((GSourceFunc) _edge_activate, self);
      } else {
        g_debug ("edge is deactivated.");
      }
      break;
    case CHAMGE_NODE_STATE_ACTIVATED:{
      g_idle_add ((GSourceFunc) _start_pipeline, self);
      break;
    }
    default:
      g_assert_not_reached ();
  }

  self->edge_prev_state = state;
}

static void
gaeul_agent_activate (GApplication * app)
{
  GaeulAgent *self = GAEUL_AGENT (app);

  g_debug ("activate");

  chamge_node_enroll (CHAMGE_NODE (self->edge), FALSE);
}

static gboolean
gaeul_agent_dbus_register (GApplication * app,
    GDBusConnection * connection, const gchar * object_path, GError ** error)
{
  gboolean ret = TRUE;
  GaeulAgent *self = GAEUL_AGENT (app);

  /* chain up */
  ret = G_APPLICATION_CLASS (gaeul_agent_parent_class)->dbus_register (app,
      connection, object_path, error);
  if (ret &&
      !g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON
          (self->dbus_manager), connection, "/org/hwangsaeul/Gaeul/Manager",
          error)) {
    g_warning ("Failed to export Gaeul D-Bus interface (reason: %s)",
        (*error)->message);
  }

  return ret;
}

static void
gaeul_agent_dbus_unregister (GApplication * app,
    GDBusConnection * connection, const gchar * object_path)
{
  GaeulAgent *self = GAEUL_AGENT (app);

  if (self->dbus_manager)
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON
        (self->dbus_manager));

  /* chain up */
  G_APPLICATION_CLASS (gaeul_agent_parent_class)->dbus_unregister (app,
      connection, object_path);
}

static void
gaeul_shutdown (GApplication * app)
{
  GaeulAgent *self = GAEUL_AGENT (app);
  ChamgeNodeState edge_state;
  g_autoptr (GError) error = NULL;

  g_debug ("shutdown");

  if (self->target_stream_id > 0) {
    gaeguli_pipeline_remove_target (self->pipeline, self->target_stream_id,
        &error);
  }

  if (self->edge_state_changed_id > 0) {
    g_signal_handler_disconnect (self->edge, self->edge_state_changed_id);
    self->edge_state_changed_id = 0;
  }

  /* FIXME: chamge should do internally when disposing */
  g_object_get (self->edge, "state", &edge_state, NULL);
  switch (edge_state) {
    case CHAMGE_NODE_STATE_ACTIVATED:
      chamge_node_deactivate (CHAMGE_NODE (self->edge));
    case CHAMGE_NODE_STATE_ENROLLED:
      chamge_node_delist (CHAMGE_NODE (self->edge));
    case CHAMGE_NODE_STATE_NULL:
      g_debug ("edge is NULL state now");
  }

  gaeguli_pipeline_stop (self->pipeline);

  gaeguli_fifo_transmit_stop (self->transmit, self->transmit_id, &error);

  gaeul_dbus_manager_set_state (self->dbus_manager, DBUS_STATE_PAUSED);

  G_APPLICATION_CLASS (gaeul_agent_parent_class)->shutdown (app);
}

static void
gaeul_agent_dispose (GObject * object)
{
  GaeulAgent *self = GAEUL_AGENT (object);

  g_clear_object (&self->edge);
  g_clear_object (&self->pipeline);
  g_clear_object (&self->transmit);

  g_clear_object (&self->dbus_manager);
  g_clear_object (&self->settings);

  g_clear_pointer (&self->srt_target_uri, g_free);

  G_OBJECT_CLASS (gaeul_agent_parent_class)->dispose (object);
}

static void
gaeul_agent_class_init (GaeulAgentClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  object_class->dispose = gaeul_agent_dispose;

  app_class->activate = gaeul_agent_activate;
  app_class->dbus_register = gaeul_agent_dbus_register;
  app_class->dbus_unregister = gaeul_agent_dbus_unregister;
  app_class->shutdown = gaeul_shutdown;
}

static void
gaeul_agent_init (GaeulAgent * self)
{
  self->dbus_manager = gaeul_dbus_manager_skeleton_new ();
  self->settings = g_settings_new (GAEUL_SCHEMA_ID);
  g_autofree gchar *uid = NULL;
  g_autofree gchar *encoding_method = NULL;
  g_autofree gchar *video_source = NULL;
  g_autofree gchar *video_device = NULL;

  GEnumValue *enum_value = NULL;
  g_autoptr (GEnumClass) enum_class = NULL;

  GaeguliVideoSource vsrc;
  GaeguliEncodingMethod enc;

  uid = g_settings_get_string (self->settings, "edge-id");
  encoding_method = g_settings_get_string (self->settings, "encoding-method");
  video_source = g_settings_get_string (self->settings, "video-source");
  video_device = g_settings_get_string (self->settings, "video-device");

  g_debug ("activate edge id:[%s], encoding-method:[%s]", uid, encoding_method);

  enum_class = g_type_class_ref (GAEGULI_TYPE_ENCODING_METHOD);
  enum_value = g_enum_get_value_by_nick (enum_class, encoding_method);

  enc = enum_value->value;

  enum_class = g_type_class_ref (GAEGULI_TYPE_VIDEO_SOURCE);
  enum_value = g_enum_get_value_by_nick (enum_class, video_source);

  vsrc = enum_value->value;

  self->edge = chamge_edge_new (uid);
  self->edge_prev_state = CHAMGE_NODE_STATE_NULL;
  self->pipeline = gaeguli_pipeline_new_full (vsrc, video_device, enc);

  self->edge_state_changed_id =
      g_signal_connect (self->edge, "state-changed",
      G_CALLBACK (_edge_state_changed_cb), self);

  self->transmit = gaeguli_fifo_transmit_new ();
}

static guint signal_watch_intr_id;

static gboolean
intr_handler (gpointer user_data)
{
  GApplication *app = user_data;
  GaeulAgent *self = GAEUL_AGENT (app);

  g_debug ("releasing app");
  g_application_release (app);

  signal_watch_intr_id = 0;

  return G_SOURCE_REMOVE;
}

int
main (int argc, char **argv)
{
  g_autoptr (GApplication) app = NULL;
  g_autoptr (GError) error = NULL;
  int ret;

  app = G_APPLICATION (g_object_new (GAEUL_TYPE_AGENT,
          "application-id", "org.hwangsaeul.Gaeul",
          "flags", G_APPLICATION_IS_SERVICE, NULL));

  signal_watch_intr_id =
      g_unix_signal_add (SIGINT, (GSourceFunc) intr_handler, app);
  g_application_set_inactivity_timeout (app, 10000);

  if (!g_application_register (app, NULL, &error)) {
    g_debug ("failed to register app (reason: %s)", error->message);
    return -1;
  }

  g_application_hold (app);

  g_application_activate (app);

  ret = g_application_run (app, argc, argv);

  if (signal_watch_intr_id > 0) {
    g_source_remove (signal_watch_intr_id);
  }

  return ret;
}
