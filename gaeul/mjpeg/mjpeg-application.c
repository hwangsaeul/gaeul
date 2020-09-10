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

#include <glib/gi18n.h>
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
    "srtsrc name=src uri=\"%s\" ! queue ! tsdemux latency=%d ! h264parse ! decodebin ! " \
    "videoconvert ! videoscale ! videorate ! " \
    "video/x-raw, framerate=%d/1, width=%d, height=%d ! "\
    "jpegenc ! multipartmux boundary=endofsection ! multisocketsink name=msocksink sync=false"
/* *INDENT-ON* */

struct _GaeulMjpegApplication
{
  GaeulApplication parent;

  SoupServer *soup_server;

  Gaeul2DBusMJPEGService *service;

  GHashTable *request_ids;
  GHashTable *pipelines;

  GSettings *settings;

  gchar *external_url;
  gchar *relay_url;
  guint limit_srt_connections;
  guint limit_user_sessions;
  guint limit_sessions_per_user;

  gchar *local_ip;

  /* statistics */
  guint stats_timeout_id;
  GList *client_sockets;

  guint64 srt_bytes_received;
  guint64 http_bytes_sent;
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

  if (GST_STATE (pipeline) != GST_STATE_PLAYING) {
    gst_element_set_state (pipeline, GST_STATE_PLAYING);
  }
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
  g_autoptr (GVariant) lookup_data = NULL;

  g_autofree gchar *request_id = NULL;
  GaeulMjpegRequest *r = NULL;

  if (!g_str_has_prefix (path, "/mjpeg/")) {
    soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
    return;
  }

  /* Assuming that the rest string of url excluded "/mjpeg/" is "request_id". */
  request_id = g_strdup (path + 7);

  g_debug ("requested transcoding stream (id: %s)", request_id);

  if ((r = g_hash_table_lookup (self->request_ids, request_id)) == NULL) {
    g_info ("invalid request (id: %s)", request_id);
    soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
    return;
  }

  pipeline = g_hash_table_lookup (self->pipelines, r);

  if (pipeline == NULL) {
    g_info ("no proper pipeline is found (id: %s)", request_id);
    soup_message_set_status (msg, SOUP_STATUS_NOT_FOUND);
    return;
  }

  soup_message_set_http_version (msg, SOUP_HTTP_1_0);
  soup_message_headers_set_encoding (msg->response_headers, SOUP_ENCODING_EOF);
  soup_message_headers_set_content_type (msg->response_headers,
      "multipart/x-mixed-replace; boundary=--endofsection", NULL);
  soup_message_set_status (msg, SOUP_STATUS_OK);

  sink = gst_bin_get_by_name (GST_BIN (pipeline), "msocksink");

  g_signal_emit_by_name (sink, "add",
      soup_client_context_get_gsocket (client_ctx));

  g_signal_connect (G_OBJECT (msg), "wrote-headers",
      G_CALLBACK (gaeul_mjpeg_http_message_wrote_headers_cb), pipeline);
}

static void
gaeul_mjpeg_application_dispose (GObject * object)
{
  GaeulMjpegApplication *self = GAEUL_MJPEG_APPLICATION (object);

  g_clear_object (&self->settings);
  g_clear_pointer (&self->pipelines, g_hash_table_unref);
  g_clear_pointer (&self->request_ids, g_hash_table_unref);

  soup_server_disconnect (self->soup_server);
  g_clear_object (&self->soup_server);

  G_OBJECT_CLASS (gaeul_mjpeg_application_parent_class)->dispose (object);
}

static void
_collect_stats (gpointer key, gpointer value, gpointer user_data)
{
  GaeulMjpegApplication *self = user_data;
  GstElement *pipeline = value;

  g_autoptr (GstElement) src = NULL;
  g_autoptr (GstElement) sink = NULL;
  g_autoptr (GstStructure) s1 = NULL;
  g_autoptr (GstStructure) s2 = NULL;
  g_autofree gchar *str = NULL;
  GList *l = NULL;
  guint64 srt_received = 0;
  guint64 http_sent = 0;

  src = gst_bin_get_by_name (GST_BIN (pipeline), "src");
  sink = gst_bin_get_by_name (GST_BIN (pipeline), "msocksink");

  g_object_get (src, "stats", &s1, NULL);

  if (gst_structure_get_uint64 (s1, "bytes-received-total", &srt_received)) {
    self->srt_bytes_received += srt_received;
  }

  for (l = self->client_sockets; l != NULL; l = l->next) {
    g_signal_emit_by_name (sink, "get-stats", l->data, &s2, NULL);

    if (gst_structure_get_uint64 (s2, "bytes-sent", &http_sent)) {
      self->http_bytes_sent += http_sent;
    }
  }

  {
    gint n_srtconnections = g_hash_table_size (self->pipelines);
    gint n_httpconnections = g_list_length (self->client_sockets);
    g_autoptr (GVariant) overall_stats = NULL;

    gaeul2_dbus_mjpegservice_set_number_of_httpconnections (self->service,
        n_httpconnections);
    gaeul2_dbus_mjpegservice_set_number_of_srtconnections (self->service,
        n_srtconnections);
    gaeul2_dbus_mjpegservice_set_total_bytes_received (self->service,
        self->srt_bytes_received);
    gaeul2_dbus_mjpegservice_set_total_bytes_sent (self->service,
        self->http_bytes_sent);

    overall_stats =
        g_variant_new ("(iitt)", n_srtconnections, n_httpconnections,
        self->srt_bytes_received, self->http_bytes_sent);
    gaeul2_dbus_mjpegservice_set_overall_stats (self->service,
        g_variant_ref_sink (overall_stats));
  }
}

static gboolean
_stats_collection_timeout (gpointer user_data)
{
  GaeulMjpegApplication *self = user_data;

  g_hash_table_foreach (self->pipelines, _collect_stats, self);

  return G_SOURCE_CONTINUE;
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

  if (g_settings_get_boolean (self->settings, "statistics")) {
    self->stats_timeout_id =
        g_timeout_add (g_settings_get_uint (self->settings,
            "statistics-interval"), _stats_collection_timeout, self);
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
    guint port, const gchar * request_id)
{
  g_autofree gchar *url = NULL;

  /* Use external_url if given */
  if (g_str_has_prefix (external_url, "http")) {
    url = g_strdup_printf ("%s/mjpeg/%s", external_url, request_id);
  } else {
    url = g_strdup_printf ("http://%s:%u/mjpeg/%s", local_ip, port, request_id);
  }

  return g_steal_pointer (&url);
}

static void
_pipeline_sink_client_added_cb (GstElement * sink, GSocket * socket,
    gpointer user_data)
{
  GaeulMjpegApplication *self = GAEUL_MJPEG_APPLICATION (user_data);

  g_debug ("client added %p", socket);
  self->client_sockets = g_list_prepend (self->client_sockets, socket);
}

static void
_pipeline_sink_client_socket_removed_cb (GstElement * sink, GSocket * socket,
    gpointer user_data)
{
  GaeulMjpegApplication *self = GAEUL_MJPEG_APPLICATION (user_data);

  g_debug ("client removed %p", socket);
  self->client_sockets = g_list_remove (self->client_sockets, socket);
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
  g_autofree gchar *request_id = NULL;
  g_autoptr (GaeulMjpegRequest) request = NULL;

  GHashTableIter iter;
  gpointer key, value;
  gboolean found = FALSE;

  request = gaeul_mjpeg_request_new (uid, rid, 0, latency, width, height, fps);

  /* Every start request will have unique id. */
  request_id = g_uuid_string_random ();

  g_debug ("start transcoder (id: %s) uid: %s rid: %s "
      "  resolution: %" G_GUINT32_FORMAT "x%" G_GUINT32_FORMAT
      "  fps: %" G_GUINT32_FORMAT " latency: %" G_GUINT32_FORMAT " ms.",
      request_id, uid, rid, width, height, fps, latency);

  return_url =
      _build_return_url (self->external_url, self->local_ip,
      g_settings_get_uint (self->settings, "bind-port"), request_id);

  /* Check if transcoding pipeline is running */
  g_hash_table_iter_init (&iter, self->pipelines);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    GaeulMjpegRequest *r = key;

    if (gaeul_mjpeg_request_equal (r, request)) {
      g_clear_pointer (&request, gaeul_mjpeg_request_unref);
      request = gaeul_mjpeg_request_ref (r);
      found = TRUE;
      g_debug ("found existing mjpeg transcoder pipeline (id: %s)", request_id);
      break;
    }
  }

  if (!found) {

    /* Transcoding pipeline must be created */

    g_autoptr (GstElement) pipeline = NULL;
    g_autoptr (GstElement) src = NULL;
    g_autoptr (GstElement) sink = NULL;
    g_autoptr (GError) error = NULL;
    g_autofree gchar *streamid = NULL;
    g_autofree gchar *pipeline_desc =
        _build_srtsrc_pipeline_desc (self->relay_url, width, height, fps,
        latency);
    pipeline = gst_parse_launch (pipeline_desc, &error);

    if (error != NULL) {
      g_dbus_method_invocation_return_gerror (invocation, error);
      return TRUE;
    }

    src = gst_bin_get_by_name (GST_BIN (pipeline), "src");
    streamid = g_strdup_printf ("#!::u=%s,r=%s", uid, rid);
    g_object_set (src, "streamid", streamid, NULL);

    g_debug ("Created transcoding pipeline for (id: %s, %s)", request_id,
        streamid);

    sink = gst_bin_get_by_name (GST_BIN (pipeline), "msocksink");
    g_signal_connect (sink, "client-added",
        G_CALLBACK (_pipeline_sink_client_added_cb), self);
    g_signal_connect (sink, "client-socket-removed",
        G_CALLBACK (_pipeline_sink_client_socket_removed_cb), self);

    gst_element_set_state (pipeline, GST_STATE_READY);

    g_hash_table_insert (self->pipelines, request, g_steal_pointer (&pipeline));
  }

  g_hash_table_insert (self->request_ids, g_strdup (request_id),
      g_steal_pointer (&request));

  gaeul2_dbus_mjpegservice_complete_start (object, invocation, return_url,
      request_id);

  return TRUE;
}

static gboolean
gaeul_mjpeg_application_handle_stop (Gaeul2DBusMJPEGService * object,
    GDBusMethodInvocation * invocation,
    const gchar * request_id, gpointer user_data)
{
  GaeulMjpegApplication *self = GAEUL_MJPEG_APPLICATION (user_data);
  GaeulMjpegRequest *r = NULL;

  if ((r = g_hash_table_lookup (self->request_ids, request_id)) == NULL) {
    g_info ("Stop operation is requested (id: %s), but not existed",
        request_id);
    return TRUE;
  }

  /* FIXME: It's not a good idea to access refcount directly */
  if (r->refcount > 1) {
    gaeul_mjpeg_request_unref (r);
    g_info ("after unref r->refcount: %u", r->refcount);
  } else {
    GstElement *pipeline = g_hash_table_lookup (self->pipelines, r);

    if (pipeline != NULL) {
      gst_element_set_state (pipeline, GST_STATE_NULL);
    }
    g_debug ("stopping pipeline %p", pipeline);
    g_hash_table_remove (self->pipelines, r);
  }

  g_hash_table_remove (self->request_ids, request_id);

  gaeul2_dbus_mjpegservice_complete_stop (object, invocation);

  return TRUE;
}

static void
gaeul_mjpeg_application_init (GaeulMjpegApplication * self)
{
  gst_init (NULL, NULL);

  self->soup_server = soup_server_new (NULL, NULL);

  self->request_ids =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

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
