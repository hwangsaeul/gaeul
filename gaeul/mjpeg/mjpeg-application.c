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

#include "config.h"

#include "gaeul.h"
#include "mjpeg/mjpeg-application.h"

#include "mjpeg/mjpeg-request.h"

#include "mjpeg/mjpeg-generated.h"

#include <gmodule.h>
#include <gst/gst.h>
#include <libsoup/soup.h>
#include <libsoup/soup-server.h>

typedef enum
{
  PROP_EXTERNAL_URL = 1,
  PROP_RELAY_URL,
  PROP_LIMIT_SRT_CONNECTIONS,
  PROP_LIMIT_USER_SESSIONS,
  PROP_LIMIT_SESSIONS_PER_USER,

  /*< private > */
  PROP_LAST
} GaeulMjpegApplicationProperty;

static GParamSpec *properties[PROP_LAST] = { NULL };

/* *INDENT-OFF* */
#define GST_SRTSRC_PIPELINE_DESC \
    "srtsrc name=src uri=\"%s\" ! queue ! tsdemux latency=%d ! decodebin ! " \
    "videoconvert ! videoscale ! videorate ! " \
    "video/x-raw, framerate=%d/1, width=%d, height=%d ! "\
    "jpegenc ! multipartmux boundary=endofsection ! multisocketsink name=msocksink"
/* *INDENT-ON* */

struct _GaeulMjpegApplication
{
  GaeulApplication parent;

  SoupServer *soup_server;

  Gaeul2DBusMJPEGService *service;

  GHashTable *pipelines;

  GSettings *settings;

  gchar *external_url;
  gchar *relay_url;
  guint limit_srt_connections;
  guint limit_user_sessions;
  guint limit_sessions_per_user;

  gchar *local_ip;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulMjpegApplication, gaeul_mjpeg_application, GAEUL_TYPE_APPLICATION)
/* *INDENT-ON* */

static gchar *
_build_srtsrc_pipeline_desc (const gchar * relay_uri,
    gint width, gint height, gint fps, gint latency)
{
  g_autofree gchar *pipeline_desc = NULL;

  pipeline_desc = g_strdup_printf (GST_SRTSRC_PIPELINE_DESC,
      relay_uri, latency, fps, width, height);

  return g_steal_pointer (&pipeline_desc);
}

static void
gaeul_mjpeg_http_message_wrote_headers_cb (SoupMessage * msg,
    gpointer user_data)
{
  GstElement *pipeline = user_data;

  g_debug ("wrote_headers");

  gst_element_set_state (pipeline, GST_STATE_PLAYING);
}

static gboolean
_find_pipeline (gpointer key, gpointer value, gpointer user_data)
{
  GaeulMjpegRequest *r = key;
  gchar *uid, *rid;
  guint stream_hash;
  GVariant *v = user_data;
  gboolean ret = FALSE;

  g_variant_get (v, "(ssu)", &uid, &rid, &stream_hash);
  g_debug ("uid: [%s], rid: [%s], hash: [%u]", uid, rid, stream_hash);

  ret = (g_strcmp0 (r->uid, uid) == 0) && (g_strcmp0 (r->rid, rid) == 0);

  if (ret) {
    ret = gaeul_mjpeg_request_parameter_hash (r) == stream_hash;
  }

  return ret;
}

static void
gaeul_mjpeg_http_request_cb (SoupServer * server, SoupMessage * msg,
    const char *path, GHashTable * query, SoupClientContext * client_ctx,
    gpointer user_data)
{
  GaeulMjpegApplication *self = user_data;

  GstElement *pipeline = NULL;
  g_autoptr (GstElement) sink = NULL;
  g_autoptr (GstElement) src = NULL;
  g_autofree gchar *uid = NULL;
  g_autofree gchar *rid = NULL;
  g_autoptr (GError) error = NULL;
  g_auto (GStrv) tmp_strv = NULL;
  guint stream_hash = 0;
  g_autoptr (GVariant) lookup_data = NULL;

  if (!g_str_has_prefix (path, "/mjpeg/")) {
    soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
    return;
  }

  tmp_strv = g_strsplit (path + 7, "/", 3);
  stream_hash = (guint) g_ascii_strtoll (tmp_strv[2], NULL, 10);
  lookup_data = g_variant_new ("(ssu)", tmp_strv[0], tmp_strv[1], stream_hash);

  /* look up id */
  pipeline = g_hash_table_find (self->pipelines, _find_pipeline, lookup_data);

  if (pipeline == NULL) {
    g_info ("no proper pipeline was found. (%s)", path);
    soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
    return;
  }

  soup_message_set_http_version (msg, SOUP_HTTP_1_0);
  soup_message_headers_set_encoding (msg->response_headers, SOUP_ENCODING_EOF);
  soup_message_headers_set_content_type (msg->response_headers,
      "multipart/x-mixed-replace; boundary=--endofsection", NULL);
  soup_message_set_status (msg, SOUP_STATUS_OK);

  sink = gst_bin_get_by_name (GST_BIN (pipeline), "msocksink");

  gst_element_set_state (pipeline, GST_STATE_READY);
  g_signal_emit_by_name (sink, "add",
      soup_client_context_get_gsocket (client_ctx));

  src = gst_bin_get_by_name (GST_BIN (pipeline), "src");
  g_object_set (src, "streamid", tmp_strv[0], NULL);

  g_signal_connect (G_OBJECT (msg), "wrote-headers",
      G_CALLBACK (gaeul_mjpeg_http_message_wrote_headers_cb), pipeline);
}

static void
gaeul_mjpeg_application_dispose (GObject * object)
{
  GaeulMjpegApplication *self = GAEUL_MJPEG_APPLICATION (object);

  g_clear_object (&self->settings);
  g_clear_pointer (&self->pipelines, g_hash_table_unref);

  soup_server_disconnect (self->soup_server);
  g_clear_object (&self->soup_server);

  G_OBJECT_CLASS (gaeul_mjpeg_application_parent_class)->dispose (object);
}

static void
gaeul_mjpeg_application_startup (GApplication * app)
{
  GaeulMjpegApplication *self = GAEUL_MJPEG_APPLICATION (app);
  g_autoptr (GError) error = NULL;

  guint port = 0;

  g_clear_object (&self->settings);
  self->settings = gaeul_gsettings_new (GAEUL_MJPEG_APPLICATION_SCHEMA_ID,
      gaeul_application_get_config_path (GAEUL_APPLICATION (self)));

  g_settings_bind (self->settings, "uid", self, "uid", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "external-url", self, "external-url",
      G_SETTINGS_BIND_GET);
  g_settings_bind (self->settings, "relay-url", self, "relay-url",
      G_SETTINGS_BIND_GET);
  g_settings_bind (self->settings, "limit-srt-connections", self,
      "limit-srt-connections", G_SETTINGS_BIND_GET);
  g_settings_bind (self->settings, "limit-user-sessions", self,
      "limit-user-sessions", G_SETTINGS_BIND_GET);
  g_settings_bind (self->settings, "limit-sessions-per-user", self,
      "limit-sessions-per-user", G_SETTINGS_BIND_GET);

  port = g_settings_get_uint (self->settings, "bind-port");
  g_free (self->local_ip);
  self->local_ip = gaeul_get_local_ip ();

  g_debug ("starting up transcoding server (port: %" G_GUINT32_FORMAT ")",
      port);

  soup_server_add_handler (self->soup_server, "/mjpeg",
      gaeul_mjpeg_http_request_cb, self, NULL);

  if (!soup_server_listen_all (self->soup_server, port, 0, &error)) {
    g_error ("failed to start http server (reason: %s)", error->message);
  }

  G_APPLICATION_CLASS (gaeul_mjpeg_application_parent_class)->startup (app);
}

static void
gaeul_mjpeg_application_activate (GApplication * app)
{
  g_debug ("activate");
  G_APPLICATION_CLASS (gaeul_mjpeg_application_parent_class)->activate (app);
}

static void
gaeul_mjpeg_application_shutdown (GApplication * app)
{
  GaeulMjpegApplication *self = GAEUL_MJPEG_APPLICATION (app);

  soup_server_remove_handler (self->soup_server, "/mjpeg");

  g_debug ("shutdown");

  g_clear_pointer (&self->local_ip, g_free);

  G_APPLICATION_CLASS (gaeul_mjpeg_application_parent_class)->shutdown (app);
}

static void
gaeul_mjpeg_application_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GaeulMjpegApplication *self = GAEUL_MJPEG_APPLICATION (object);

  switch (prop_id) {
    case PROP_EXTERNAL_URL:
      g_value_set_string (value, self->external_url);
      break;
    case PROP_RELAY_URL:
      g_value_set_string (value, self->relay_url);
      break;
    case PROP_LIMIT_SRT_CONNECTIONS:
      g_value_set_uint (value, self->limit_srt_connections);
      break;
    case PROP_LIMIT_USER_SESSIONS:
      g_value_set_uint (value, self->limit_user_sessions);
      break;
    case PROP_LIMIT_SESSIONS_PER_USER:
      g_value_set_uint (value, self->limit_sessions_per_user);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gaeul_mjpeg_application_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GaeulMjpegApplication *self = GAEUL_MJPEG_APPLICATION (object);

  switch (prop_id) {
    case PROP_EXTERNAL_URL:
      g_free (self->external_url);
      self->external_url = g_value_dup_string (value);
      break;
    case PROP_RELAY_URL:
      g_free (self->relay_url);
      self->relay_url = g_value_dup_string (value);
      break;
    case PROP_LIMIT_SRT_CONNECTIONS:
      self->limit_srt_connections = g_value_get_uint (value);
      break;
    case PROP_LIMIT_USER_SESSIONS:
      self->limit_user_sessions = g_value_get_uint (value);
      break;
    case PROP_LIMIT_SESSIONS_PER_USER:
      self->limit_sessions_per_user = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gaeul_mjpeg_application_dbus_register (GApplication * app,
    GDBusConnection * connection, const gchar * object_path, GError ** error)
{
  gboolean ret = FALSE;
  GaeulMjpegApplication *self = GAEUL_MJPEG_APPLICATION (app);

  g_debug ("registering dbus (%s)", object_path);

  ret =
      G_APPLICATION_CLASS (gaeul_mjpeg_application_parent_class)->dbus_register
      (app, connection, object_path, error);

  if (ret && !g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON
          (self->service), connection,
          "/org/hwangsaeul/Gaeul2/MJPEG/Service", error)) {
    g_warning
        ("Failed to export Gaeul2MJPEG.Service D-Bus interface (reason: %s)",
        (*error)->message);
  }

  return ret;
}

static void
gaeul_mjpeg_application_dbus_unregister (GApplication * app,
    GDBusConnection * connection, const gchar * object_path)
{
  GaeulMjpegApplication *self = GAEUL_MJPEG_APPLICATION (app);

  if (self->service)
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON
        (self->service));

  /* chain up */
  G_APPLICATION_CLASS (gaeul_mjpeg_application_parent_class)->dbus_unregister
      (app, connection, object_path);
}

static void
gaeul_mjpeg_application_class_init (GaeulMjpegApplicationClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  object_class->get_property = gaeul_mjpeg_application_get_property;
  object_class->set_property = gaeul_mjpeg_application_set_property;
  object_class->dispose = gaeul_mjpeg_application_dispose;

  properties[PROP_EXTERNAL_URL] =
      g_param_spec_string ("external-url", "external-url", "external-url",
      NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_RELAY_URL] =
      g_param_spec_string ("relay-url", "relay-url", "relay-url", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_LIMIT_SRT_CONNECTIONS] =
      g_param_spec_uint ("limit-srt-connections", "limit-srt-connections",
      "limit-srt-connections", 0, G_MAXUINT32, 4,
      G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_LIMIT_USER_SESSIONS] =
      g_param_spec_uint ("limit-user-sessions", "limit-user-sessions",
      "limit-user-sessions", 0, G_MAXUINT32, 4,
      G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_LIMIT_SESSIONS_PER_USER] =
      g_param_spec_uint ("limit-sessions-per-user", "limit-sessions-per-user",
      "limit-sessions-per-user", 0, G_MAXUINT32, 4,
      G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties),
      properties);

  app_class->startup = gaeul_mjpeg_application_startup;
  app_class->activate = gaeul_mjpeg_application_activate;
  app_class->shutdown = gaeul_mjpeg_application_shutdown;
  app_class->dbus_register = gaeul_mjpeg_application_dbus_register;
  app_class->dbus_unregister = gaeul_mjpeg_application_dbus_unregister;
}

static gchar *
_build_return_url (const gchar * external_url, const gchar * local_ip,
    guint port, GaeulMjpegRequest * request)
{
  g_autofree gchar *url = NULL;

  /* Use external_url if given */
  if (g_str_has_prefix (external_url, "http")) {
    url =
        g_strdup_printf ("%s/mjpeg/%s/%s/%u", external_url, request->uid,
        request->rid, gaeul_mjpeg_request_parameter_hash (request));
  } else {
    url =
        g_strdup_printf ("http://%s:%u/mjpeg/%s/%s/%u", local_ip, port,
        request->uid, request->rid,
        gaeul_mjpeg_request_parameter_hash (request));
  }

  return g_steal_pointer (&url);
}

static gboolean
gaeul_mjpeg_application_handle_start (Gaeul2DBusMJPEGService * object,
    GDBusMethodInvocation * invocation,
    const gchar * uid, const gchar * rid,
    guint width, guint height, guint fps, guint latency, gpointer user_data)
{
  GaeulMjpegApplication *self = GAEUL_MJPEG_APPLICATION (user_data);

  g_autofree gchar *return_url = NULL;
  g_autofree gchar *path = NULL;
  g_autoptr (GstElement) pipeline = NULL;
  g_autoptr (GaeulMjpegRequest) request = NULL;
  guint stream_hash = 0;

  g_debug ("start transcoder uid: %s rid: %s "
      "  resolution: %" G_GUINT32_FORMAT "x%" G_GUINT32_FORMAT
      "  fps: %" G_GUINT32_FORMAT " latency: %" G_GUINT32_FORMAT " ms.",
      uid, rid, width, height, fps, latency);

  request = gaeul_mjpeg_request_new (uid, rid, 0, latency, width, height, fps);
  stream_hash = gaeul_mjpeg_request_parameter_hash (request);

  return_url =
      _build_return_url (self->external_url, self->local_ip,
      g_settings_get_uint (self->settings, "bind-port"), request);

  if ((pipeline = g_hash_table_lookup (self->pipelines, request)) == NULL) {
    /* the first request */

    g_autoptr (GError) error = NULL;
    g_autofree gchar *pipeline_desc =
        _build_srtsrc_pipeline_desc (self->relay_url, width, height, fps,
        latency);
    pipeline = gst_parse_launch (pipeline_desc, &error);

    if (error != NULL) {
      g_dbus_method_invocation_return_gerror (invocation, error);
      return TRUE;
    }

    g_hash_table_insert (self->pipelines, g_steal_pointer (&request),
        g_steal_pointer (&pipeline));

  } else {
    /* TODO: perhaps, we might want to increase pipeline usage count */
  }

  gaeul2_dbus_mjpegservice_complete_start (object, invocation, return_url,
      stream_hash);

  return TRUE;
}

static gboolean
gaeul_mjpeg_application_handle_stop (Gaeul2DBusMJPEGService * object,
    GDBusMethodInvocation * invocation,
    const gchar * uid, const gchar * rid, guint stream_hash, gpointer user_data)
{
  GaeulMjpegApplication *self = GAEUL_MJPEG_APPLICATION (user_data);
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, self->pipelines);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    GstElement *pipeline = value;
    GaeulMjpegRequest *r = key;

    if ((g_strcmp0 (r->uid, uid) == 0) && (g_strcmp0 (r->rid, rid) == 0) &&
        (gaeul_mjpeg_request_parameter_hash (r) == stream_hash)) {
      /* Found it */

      gst_element_set_state (pipeline, GST_STATE_NULL);

      g_hash_table_iter_remove (&iter);
    }
  }

  gaeul2_dbus_mjpegservice_complete_stop (object, invocation);

  return TRUE;
}

static void
gaeul_mjpeg_application_init (GaeulMjpegApplication * self)
{
  gst_init (NULL, NULL);

  self->soup_server = soup_server_new (NULL, NULL);

  self->pipelines =
      g_hash_table_new_full ((GHashFunc) gaeul_mjpeg_request_hash,
      (GEqualFunc) gaeul_mjpeg_request_equal,
      (GDestroyNotify) gaeul_mjpeg_request_unref,
      (GDestroyNotify) gst_object_unref);

  self->service = gaeul2_dbus_mjpegservice_skeleton_new ();

  g_signal_connect (self->service, "handle-start",
      G_CALLBACK (gaeul_mjpeg_application_handle_start), self);
  g_signal_connect (self->service, "handle-stop",
      G_CALLBACK (gaeul_mjpeg_application_handle_stop), self);
}
