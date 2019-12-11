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

typedef enum
{
  PROP_EDGE_ID = 1,
  PROP_ENCODING_METHOD,
  PROP_VIDEO_SOURCE,
  PROP_VIDEO_DEVICE,

  /*< private > */
  PROP_LAST
} _GaeulAgentProperty;

static GParamSpec *properties[PROP_LAST] = { NULL };

struct _GaeulAgent
{
  HwangsaeulApplication parent;
  GaeulDBusManager *dbus_manager;

  GSettings *settings;
  ChamgeEdge *edge;
  gulong edge_state_changed_id;
  gulong edge_user_command_id;
  gulong pipeline_stopped_id;
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

  gchar *edge_id;
  GaeguliEncodingMethod encoding_method;
  GaeguliVideoSource video_source;
  gchar *video_device;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulAgent, gaeul_agent, HWANGSAEUL_TYPE_APPLICATION)
/* *INDENT-ON* */

#define DBUS_STATE_PAUSED       0
#define DBUS_STATE_PLAYING      1
#define DBUS_STATE_RECORDING    2

#define DEFAULT_RESOLUTION GAEGULI_VIDEO_RESOLUTION_1280X720
#define DEFAULT_FPS 30
#define DEFAULT_BITRATES 20000000

static gboolean gaeul_agent_handle_get_edge_id (GaeulDBusManager * manager,
    GDBusMethodInvocation * invocation, gpointer user_data);

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
    g_autofree gchar *edge_id = NULL;

    if (chamge_node_get_uid (CHAMGE_NODE (self->edge),
            &edge_id) != CHAMGE_RETURN_OK) {
      g_warning ("failed to get edge ID");
    }

    self->transmit_id =
        gaeguli_fifo_transmit_start_full (self->transmit, host, port, srt_mode,
        edge_id, &error);
  }
  if (self->target_stream_id == 0) {
    self->target_stream_id =
        gaeguli_pipeline_add_fifo_target_full (self->pipeline,
        GAEGULI_VIDEO_CODEC_H264, self->resolution, self->fps, self->bitrates,
        gaeguli_fifo_transmit_get_fifo (self->transmit), &error);
  }
  g_debug ("start stream to fifo (id: %u)", self->target_stream_id);

  gaeul_dbus_manager_set_state (self->dbus_manager, DBUS_STATE_PLAYING);

  g_debug ("\n\n\n %s return \n\n\n", __func__);
  return G_SOURCE_REMOVE;
}

static void
_stop_pipeline (GaeulAgent * self)
{
  g_autoptr (GError) error = NULL;

  if (self->target_stream_id > 0) {
    gaeguli_pipeline_remove_target (self->pipeline, self->target_stream_id,
        &error);
    g_debug ("stop stream to fifo (id: %u)", self->target_stream_id);
    self->target_stream_id = 0;
  }
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

static inline guint
_get_node_value (JsonObject * obj, const gchar * str)
{
  if (obj && json_object_has_member (obj, str)) {
    JsonNode *node = json_object_get_member (obj, str);
    return json_node_get_int (node);
  }
  return 0;
}

static inline const gchar *
_get_node_string (JsonObject * obj, const gchar * str)
{
  if (obj && json_object_has_member (obj, str)) {
    JsonNode *node = json_object_get_member (obj, str);
    return json_node_get_string (node);
  }
  return 0;
}

static void
_parse_streaming_params (JsonObject * json_object,
    GaeguliVideoResolution * resolution, guint * fps, guint * bitrates,
    gchar ** srt_target_uri)
{
  guint width;
  guint height;

  json_object =
      json_node_get_object (json_object_get_member (json_object, "params"));

  width = _get_node_value (json_object, "width");
  height = _get_node_value (json_object, "height");
  *fps = _get_node_value (json_object, "fps");
  *bitrates = _get_node_value (json_object, "bitrates");
  switch (width) {
    case 640:
      if (height != 480) {
        g_debug ("width(%d) height(%d). resolution would be set 640x480", width,
            height);
      }
      *resolution = GAEGULI_VIDEO_RESOLUTION_640X480;
      break;
    case 1280:
      if (height != 720) {
        g_debug ("width(%d) height(%d). resolution would be set 1280x720",
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
      break;
  }
  if (srt_target_uri) {
    g_clear_pointer (srt_target_uri, g_free);
    *srt_target_uri = g_strdup (_get_node_string (json_object, "url"));
  }

  g_debug ("params [resolution : %d], [fps : %d], [bitrates : %d], [url : %s]",
      *resolution, *fps, *bitrates, _get_node_string (json_object, "url"));
  if (*fps == 0) {
    g_debug ("set default fps : 30");
    *fps = DEFAULT_FPS;
  }
  if (*bitrates == 0) {
    g_debug ("set default bitrates : 209710250 (20x1024x1024)");
    *bitrates = DEFAULT_BITRATES;
  }
  g_debug
      ("final params [resolution : %d], [fps : %d], [bitrates : %d], [url : %s]",
      *resolution, *fps, *bitrates, _get_node_string (json_object, "url"));
}

static void
gaeul_agent_start_pipeline (GaeulAgent * self)
{
  g_debug ("Scheduling start of streaming pipeline");
  g_idle_add_full (G_PRIORITY_HIGH, (GSourceFunc) _start_pipeline, self, NULL);
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
        _parse_streaming_params (json_object,
            &(self->resolution), &(self->fps), &(self->bitrates),
            &(self->srt_target_uri));
        if (!self->srt_target_uri)
          _get_srt_uri (self);
        if (self->srt_target_uri) {
          gaeul_agent_start_pipeline (self);
          self->is_playing = TRUE;
        } else {
          g_warning ("Couldn't determine SRT URI for streaming");
        }
      } else if (gaeul_dbus_manager_get_state (self->dbus_manager) !=
          DBUS_STATE_PAUSED) {
        g_debug ("streaming is stopping");
        *response =
            g_strdup_printf
            ("{\"result\":\"nok\",\"reason\":\"streaming is stopping, please try later\"}");
        goto out;
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
        self->is_playing = FALSE;
        g_debug ("streaming stopped");
      } else {
        g_debug ("streaming is not started");
        *response =
            g_strdup_printf
            ("{\"result\":\"nok\",\"reason\":\"streaming is not started\"}");
        goto out;
      }
    } else if (!g_strcmp0 (method, "streamingChangeParameters")) {
      if (!self->is_playing) {
        *response = g_strdup_printf
            ("{\"result\":\"nok\",\"reason\":\"streaming is not started\"}");
        goto out;
      }
      if (!json_object_has_member (json_object, "params")) {
        *response = g_strdup_printf
            ("{\"result\":\"nok\",\"reason\":\"no streaming parameters in request\"}");
        goto out;
      }
      _parse_streaming_params (json_object, &(self->resolution), &(self->fps),
          &(self->bitrates), NULL);
      _stop_pipeline (self);
      gaeul_agent_start_pipeline (self);
    }
  }
  /* TODO : implement to execute user command and make return */
  *response =
      g_strdup_printf ("{\"result\":\"ok\",\"url\":\"%s\"}",
      self->srt_target_uri);
out:
  g_debug ("response >> %s", *response);
}

static gboolean
gaeul_agent_dbus_register (GApplication * application,
    GDBusConnection * connection, const gchar * name, GError ** error)
{
  GaeulAgent *self = GAEUL_AGENT (application);

  self->dbus_manager = gaeul_dbus_manager_skeleton_new ();
  g_signal_connect (self->dbus_manager, "handle-get-edge-id",
      G_CALLBACK (gaeul_agent_handle_get_edge_id), self);

  return g_dbus_interface_skeleton_export
      (G_DBUS_INTERFACE_SKELETON (self->dbus_manager), connection,
      "/org/hwangsaeul/Gaeul/Manager", error);
}

static void
gaeul_shutdown (GApplication * app)
{
  GaeulAgent *self = GAEUL_AGENT (app);
  ChamgeNodeState edge_state;
  g_autoptr (GError) error = NULL;

  g_debug ("shutdown");

  if (self->pipeline_stopped_id > 0) {
    g_signal_handler_disconnect (self->pipeline, self->pipeline_stopped_id);
    self->pipeline_stopped_id = 0;
  }
  if (self->target_stream_id > 0) {
    gaeguli_pipeline_remove_target (self->pipeline, self->target_stream_id,
        &error);
    g_debug ("stop stream to fifo (id: %u)", self->target_stream_id);
    self->target_stream_id = 0;
  }
  if (self->pipeline != NULL) {
    g_debug ("pipeline stop");
    gaeguli_pipeline_stop (self->pipeline);
  }

  if (self->transmit_id > 0) {
    gaeguli_fifo_transmit_stop (self->transmit, self->transmit_id, &error);
    g_debug ("Removed fifo %u", self->transmit_id);
    self->transmit_id = 0;
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

  self->is_playing = FALSE;
  if (self->dbus_manager) {
    gaeul_dbus_manager_set_state (self->dbus_manager, DBUS_STATE_PAUSED);
  }

  G_APPLICATION_CLASS (gaeul_agent_parent_class)->shutdown (app);
}

static gboolean
gaeul_agent_handle_get_edge_id (GaeulDBusManager * manager,
    GDBusMethodInvocation * invocation, gpointer user_data)
{
  GaeulAgent *self = (GaeulAgent *) user_data;
  g_autofree gchar *edge_id = g_settings_get_string (self->settings, "edge-id");

  g_debug ("edge_id >> %s", edge_id);
  gaeul_dbus_manager_complete_get_edge_id (manager, invocation, edge_id);
  return TRUE;
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
gaeul_agent_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GaeulAgent *self = GAEUL_AGENT (object);
  switch (prop_id) {
    case PROP_EDGE_ID:
      g_value_set_string (value, self->edge_id);
      break;
    case PROP_ENCODING_METHOD:
      g_value_set_enum (value, self->encoding_method);
      break;
    case PROP_VIDEO_SOURCE:
      g_value_set_enum (value, self->video_source);
      break;
    case PROP_VIDEO_DEVICE:
      g_value_set_string (value, self->video_device);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gaeul_agent_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GaeulAgent *self = GAEUL_AGENT (object);
  switch (prop_id) {
    case PROP_EDGE_ID:
      g_free (self->edge_id);
      self->edge_id = g_value_dup_string (value);
      break;
    case PROP_ENCODING_METHOD:
      self->encoding_method = g_value_get_enum (value);
      break;
    case PROP_VIDEO_SOURCE:
      self->video_source = g_value_get_enum (value);
      break;
    case PROP_VIDEO_DEVICE:
      g_free (self->video_device);
      self->video_device = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
stream_stopped_cb (GaeguliPipeline * pipeline, guint target_id,
    GaeulAgent * self)
{
  g_autoptr (GError) error = NULL;

  if (!self->is_playing && self->transmit_id > 0) {
    gaeguli_fifo_transmit_stop (self->transmit, self->transmit_id, &error);
    g_debug ("Removed fifo %u", self->transmit_id);
    self->transmit_id = 0;
  }

  gaeul_dbus_manager_set_state (self->dbus_manager, DBUS_STATE_PAUSED);
}

static void
gaeul_agent_activate (GApplication * app)
{
  GaeulAgent *self = GAEUL_AGENT (app);

  chamge_node_enroll (CHAMGE_NODE (self->edge), FALSE);
}

static void
gaeul_agent_class_init (GaeulAgentClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  object_class->get_property = gaeul_agent_get_property;
  object_class->set_property = gaeul_agent_set_property;
  object_class->dispose = gaeul_agent_dispose;

  properties[PROP_EDGE_ID] =
      g_param_spec_string ("edge-id", "edge-id", "edge-id", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ENCODING_METHOD] =
      g_param_spec_enum ("encoding-method", "encoding-method",
      "encoding-method", GAEGULI_TYPE_ENCODING_METHOD,
      GAEGULI_ENCODING_METHOD_NVIDIA_TX1,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_VIDEO_SOURCE] =
      g_param_spec_enum ("video-source", "video-source", "video-source",
      GAEGULI_TYPE_VIDEO_SOURCE, GAEGULI_VIDEO_SOURCE_V4L2SRC,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_VIDEO_DEVICE] =
      g_param_spec_string ("video-device", "video-device", "video-device", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties),
      properties);

  app_class->dbus_register = gaeul_agent_dbus_register;
  app_class->activate = gaeul_agent_activate;
  app_class->shutdown = gaeul_shutdown;
}

static gboolean
_settings_map_get_enum (GValue * value, GVariant * variant,
    gpointer enum_type_class)
{
  GEnumValue *ev = g_enum_get_value_by_nick (enum_type_class,
      g_variant_get_string (variant, NULL));

  if (ev) {
    g_value_set_enum (value, ev->value);
    return TRUE;
  }

  return FALSE;
}

static GVariant *
_settings_map_set_enum (const GValue * value,
    const GVariantType * expected_type, gpointer enum_type_class)
{
  GEnumValue *ev = g_enum_get_value (enum_type_class, g_value_get_enum (value));

  if (ev) {
    return g_variant_new_string (ev->value_nick);
  }

  return NULL;
}

static void
gaeul_agent_init (GaeulAgent * self)
{
  self->settings = chamge_common_gsettings_new (GAEUL_SCHEMA_ID);

  g_settings_bind (self->settings, "edge-id", self, "edge-id",
      G_SETTINGS_BIND_DEFAULT);

  g_settings_bind_with_mapping (self->settings, "encoding-method",
      self, "encoding-method", G_SETTINGS_BIND_DEFAULT,
      _settings_map_get_enum, _settings_map_set_enum,
      g_type_class_peek (GAEGULI_TYPE_ENCODING_METHOD), NULL);

  g_settings_bind_with_mapping (self->settings, "video-source",
      self, "video-source", G_SETTINGS_BIND_DEFAULT,
      _settings_map_get_enum, _settings_map_set_enum,
      g_type_class_peek (GAEGULI_TYPE_VIDEO_SOURCE), NULL);

  g_settings_bind (self->settings, "video-device", self, "video-device",
      G_SETTINGS_BIND_DEFAULT);

  if (!g_strcmp0 (self->edge_id, "randomized-string")
      || strlen (self->edge_id) == 0) {
    g_autofree gchar *uid = g_uuid_string_random ();
    g_autofree gchar *edge_id =
        g_compute_checksum_for_string (G_CHECKSUM_SHA256, uid, strlen (uid));
    g_object_set (self, "edge-id", edge_id, NULL);
  }

  g_debug ("activate edge id:[%s], encoding-method:[%s]", self->edge_id,
      g_enum_get_value (g_type_class_peek (GAEGULI_TYPE_ENCODING_METHOD),
          self->encoding_method)->value_nick);

  self->edge = chamge_edge_new (self->edge_id);
  self->edge_prev_state = CHAMGE_NODE_STATE_NULL;
  self->pipeline = gaeguli_pipeline_new_full (self->video_source,
      self->video_device, self->encoding_method);

  self->edge_state_changed_id =
      g_signal_connect (self->edge, "state-changed",
      G_CALLBACK (_edge_state_changed_cb), self);
  self->edge_user_command_id =
      g_signal_connect (self->edge, "user-command",
      G_CALLBACK (_edge_user_command_cb), self);

  self->pipeline_stopped_id =
      g_signal_connect (self->pipeline, "stream-stopped",
      (GCallback) stream_stopped_cb, self);

  self->transmit = gaeguli_fifo_transmit_new ();
}

static gboolean
intr_handler (gpointer user_data)
{
  GApplication *app = user_data;

  g_debug ("releasing app");
  g_application_release (app);

  return G_SOURCE_REMOVE;
}

int
main (int argc, char **argv)
{
  g_autoptr (GApplication) app = NULL;
  g_autoptr (GError) error = NULL;

  app = G_APPLICATION (g_object_new (GAEUL_TYPE_AGENT,
          "application-id", GAEUL_SCHEMA_ID, NULL));

  g_unix_signal_add (SIGINT, (GSourceFunc) intr_handler, app);

  g_application_hold (app);

  return hwangsaeul_application_run (HWANGSAEUL_APPLICATION (app), argc, argv);
}
