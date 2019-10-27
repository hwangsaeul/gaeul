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

#include <json-glib/json-glib.h>

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
  gulong edge_user_command_id;
  ChamgeNodeState edge_prev_state;
  GaeguliPipeline *pipeline;
  GaeguliFifoTransmit *transmit;

  gchar *srt_target_uri;
  guint target_stream_id;
  guint transmit_id;

  gboolean is_playing;
  guint resolution;
  guint fps;
  guint bitrates;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulAgent, gaeul_agent, G_TYPE_APPLICATION)
/* *INDENT-ON* */

#define DBUS_STATE_PAUSED       0
#define DBUS_STATE_PLAYING      1
#define DBUS_STATE_RECORDING    2

#define DEFAULT_RESOLUTION GAEGULI_VIDEO_RESOLUTION_1280X720
#define DEFAULT_FPS 30
#define DEFAULT_BITRATES 20000000

static char
_search_delimiter (const char *from, guint * position)
{
  *position = 0;

  for (;;) {
    char ch = *from++;
    (*position)++;

    switch (ch) {
      case ':':
      case 0:
      case '/':
      case '?':
      case '#':
      case '=':
        return ch;
      default:
        break;
    }
  }
}

static ChamgeReturn
_srt_parse_uri (const gchar * url, gchar ** host, guint * portnum,
    gchar ** mode)
{
  ChamgeReturn res = CHAMGE_RETURN_FAIL;
  gchar delimiter = 0;
  g_autofree gchar *port = NULL;
  guint position = 0;

  g_return_val_if_fail (host != NULL, res);
  g_return_val_if_fail (portnum != NULL, res);

  if (!strncmp (url, "srt://", 6)) {
    url += 6;
  }

  delimiter = _search_delimiter (url, &position);
  *host = g_strndup (url, position - 1);

  if (delimiter == ':') {
    url += position;
    delimiter = _search_delimiter (url, &position);
    port = g_strndup (url, position - 1);
  }

  if (port) {
    gchar *end = NULL;
    *portnum = strtol (port, &end, 10);

    if (port == end || *end != 0 || *portnum < 0 || *portnum > 65535) {
      goto out;
    }
  }

  if (delimiter == '?' && mode != NULL) {
    url += position;
    delimiter = _search_delimiter (url, &position);

    if (delimiter != '=') {
      goto out;
    }

    *mode = g_strndup (url, position - 1);
    if (!g_strcmp0 (*mode, "mode")) {
      url += position;
      delimiter = _search_delimiter (url, &position);
      if (position > 1) {
        g_free (*mode);
        *mode = g_strndup (url, position - 1);
      }
    } else {
      g_free (*mode);
      *mode = NULL;
    }
    res = CHAMGE_RETURN_OK;
  } else if (delimiter == 0) {
    res = CHAMGE_RETURN_OK;
  }

out:
  return res;
}


static ChamgeReturn
_get_srt_uri (GaeulAgent * self)
{
  g_autoptr (GError) error = NULL;

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
    return CHAMGE_RETURN_FAIL;
  }
  return CHAMGE_RETURN_OK;
}

static gboolean
_start_pipeline (GaeulAgent * self)
{
  g_autoptr (GError) error = NULL;
  g_autofree gchar *host = NULL;
  g_autofree gchar *mode = NULL;
  guint port = 0;
  GaeguliSRTMode srt_mode = GAEGULI_SRT_MODE_CALLER;

  if (self->is_playing) {
    g_debug ("pipeline is already playing");
    return G_SOURCE_REMOVE;
  }
  g_debug ("edge is activated.");
  /* SRT connection uri is available only when activated */

  g_debug ("connect to (uri: %s)", self->srt_target_uri);

  if (_srt_parse_uri (self->srt_target_uri, &host, &port, &mode)
      != CHAMGE_RETURN_OK) {
    g_warning ("failed to parse uri");
  }

  if (host == NULL || port == 0) {
    g_warning ("failed to get host or port number");
  }

  if (mode != NULL) {
    g_debug ("uri mode : %s", mode);
  }

  if (host == NULL || port == 0) {
    g_warning ("failed to get host or port number");
  }

  if (mode != NULL) {
    g_debug ("uri mode : %s", mode);
    /* if target uri mode is listener, edge should be caller
     * and target uri mode is caller, edge should be listener */
    if (!g_strcmp0 (mode, "listener"))
      srt_mode = GAEGULI_SRT_MODE_CALLER;
    else if (!g_strcmp0 (mode, "caller"))
      srt_mode = GAEGULI_SRT_MODE_LISTENER;
  }

  g_debug ("connect to host : %s, port : %d, srt_mode : %d", host, port,
      srt_mode);
  if (self->transmit_id == 0) {
    self->transmit_id =
        gaeguli_fifo_transmit_start (self->transmit, host, port, srt_mode,
        &error);
  }
  if (self->target_stream_id == 0) {
    self->target_stream_id =
        gaeguli_pipeline_add_fifo_target_full (self->pipeline,
        GAEGULI_VIDEO_CODEC_H264, self->resolution,
        gaeguli_fifo_transmit_get_fifo (self->transmit), &error);
  }
  g_debug ("start stream to fifo (id: %u)", self->target_stream_id);

  self->is_playing = TRUE;
  gaeul_dbus_manager_set_state (self->dbus_manager, DBUS_STATE_PLAYING);

  g_debug ("\n\n\n %s return \n\n\n", __func__);
  return G_SOURCE_REMOVE;
}

static gboolean
_stop_pipeline (GaeulAgent * self)
{
  g_autoptr (GError) error = NULL;

  if (!self->is_playing) {
    g_debug ("pipeline is not started");
    return CHAMGE_RETURN_FAIL;
  }

  g_debug ("stop stream to fifo (id: %u)", self->target_stream_id);
  if (self->transmit_id > 0) {
    gaeguli_fifo_transmit_stop (self->transmit, self->transmit_id, &error);
    self->transmit_id = 0;
  }
  if (self->target_stream_id > 0) {
    gaeguli_pipeline_remove_target (self->pipeline, self->target_stream_id,
        &error);
    self->target_stream_id = 0;
  }

  gaeul_dbus_manager_set_state (self->dbus_manager, DBUS_STATE_PAUSED);
  self->is_playing = FALSE;

  return CHAMGE_RETURN_OK;
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
      //self->streaming_process_id = g_idle_add ((GSourceFunc) _start_pipeline, self);
      g_debug ("edge is activated.");
      break;
    }
    default:
      g_assert_not_reached ();
  }

  self->edge_prev_state = state;
}

inline guint
_get_node_value (JsonObject * obj, const gchar * str)
{
  if (json_object_has_member (obj, str)) {
    JsonNode *node = json_object_get_member (obj, str);
    return json_node_get_int (node);
  }
  return 0;
}

static gboolean
_paras_streaming_params (JsonObject * json_object, guint * resolution,
    guint * fps, guint * bitrates)
{
  guint width = _get_node_value (json_object, "width");
  guint height = _get_node_value (json_object, "height");;
  gboolean ret = TRUE;
  *fps = _get_node_value (json_object, "fps");
  *bitrates = _get_node_value (json_object, "bitrates");
  switch (width) {
    case 640:
      if (height != 480) {
        g_debug ("width(%d) height(%d). resolution would be set 640x480", width,
            height);
      }
      *resolution = GAEGULI_VIDEO_RESOLUTION_640x480;
      break;
    case 1280:
      if (height != 7200) {
        g_debug ("width(%d) height(%d). resolution would be set 1280x7200",
            width, height);
      }
      *resolution = GAEGULI_VIDEO_RESOLUTION_1280X720;
      break;
    case 1920:
      if (height != 1080) {
        g_debug ("width(%d) height(%d). resolution would be set 1920x1080",
            width, height);
      }
      *resolution = GAEGULI_VIDEO_RESOLUTION_1920X1080;
      break;
    case 3840:
      if (height != 2160) {
        g_debug ("width(%d) height(%d). resolution would be set 3840x2160",
            width, height);
      }
      *resolution = GAEGULI_VIDEO_RESOLUTION_3840X2160;
      break;
    default:
      *resolution = DEFAULT_RESOLUTION;
      ret = FALSE;
      break;
  }
  g_debug ("params [resolution : %d], [fps : %d], [bitrates : %d]",
      *resolution, *fps, *bitrates);
  if (*fps == 0) {
    g_debug ("set default fps : 30");
    *fps = DEFAULT_FPS;
    ret = FALSE;
  }
  if (*bitrates == 0) {
    g_debug ("set default bitrates : 209710250 (20x1024x1024)");
    *bitrates = DEFAULT_BITRATES;
    ret = FALSE;
  }
  g_debug ("final params [resolution : %d], [fps : %d], [bitrates : %d]",
      *resolution, *fps, *bitrates);
  return ret;
}

static void
_edge_user_command_cb (ChamgeEdge * edge, const gchar * user_command,
    gchar ** response, GError ** error, GaeulAgent * self)
{
  g_autoptr (JsonParser) parser = json_parser_new ();
  JsonNode *root = NULL;
  JsonObject *json_object = NULL;

  g_debug ("user command cb >> %s\n", user_command);
  if (!json_parser_load_from_data (parser, user_command, strlen (user_command),
          error)) {
    g_debug ("failed to parse body: %s", (*error)->message);
    *response = g_strdup ("{\"result\":\"failed to parse user command\"}");
    return;
  }

  root = json_parser_get_root (parser);
  json_object = json_node_get_object (root);
  if (json_object_has_member (json_object, "method")) {
    JsonNode *node = json_object_get_member (json_object, "method");
    const gchar *method = json_node_get_string (node);
    g_debug ("METHOD >>> %s", method);
    if (!g_strcmp0 (method, "streamingStart")) {
      if (!self->is_playing) {
        if (_get_srt_uri (self) == CHAMGE_RETURN_OK) {
          if (json_object_has_member (json_object, "params")) {
            JsonNode *node = json_object_get_member (json_object, "params");
            _paras_streaming_params (json_node_get_object (node),
                &(self->resolution), &(self->fps), &(self->bitrates));
          } else {
            self->resolution = DEFAULT_RESOLUTION;
            self->fps = DEFAULT_FPS;
            self->bitrates = DEFAULT_BITRATES;
          }
          g_idle_add ((GSourceFunc) _start_pipeline, self);
          g_debug ("streaming is starting");
        }
      } else {
        g_debug ("streaming is already started");
        *response =
            g_strdup_printf
            ("{\"result\":\"nok\",\"reason\":\"streaming is already started\"}");
        goto out;
      }
    } else if (!g_strcmp0 (method, "streamingStop")) {
      /* TODO : need to implement to streaming stop */
      if (self->is_playing) {
        _stop_pipeline (self);
        g_debug ("streaming stopped");
      } else {
        g_debug ("streaming is not started");
        *response =
            g_strdup_printf
            ("{\"result\":\"nok\",\"reason\":\"streaming is not started\"}");
        goto out;
      }
    }
  }
  /* TODO : implement to execute user command and make return */
  *response =
      g_strdup_printf ("{\"result\":\"ok\",\"url\":\"%s\"}",
      self->srt_target_uri);
out:
  g_debug ("response >> %s", *response);
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

  self->is_playing = FALSE;
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
  self->edge_user_command_id =
      g_signal_connect (self->edge, "user-command",
      G_CALLBACK (_edge_user_command_cb), self);

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
  g_application_set_inactivity_timeout (app, 1000);

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
