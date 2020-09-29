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
#include "source/source-application.h"
#include "source/source-generated.h"

#include <gaeguli/gaeguli.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

/* *INDENT-OFF* */
#if !GLIB_CHECK_VERSION(2,57,1)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GEnumClass, g_type_class_unref)
#endif
/* *INDENT-ON* */

typedef struct _GaeguliNest
{
  gint refcount;

  gchar *id;
  GaeguliPipeline *pipeline;
  GaeguliTarget *target_stream;
  Gaeul2DBusSourceChannel *dbus;
} GaeguliNest;

static gboolean _channel_handle_get_stats (GaeguliNest * nest,
    GDBusMethodInvocation * invocation);

static GaeguliNest *
gaeguli_nest_new (const gchar * id, GaeguliPipeline * pipeline)
{
  GaeguliNest *nest = g_new0 (GaeguliNest, 1);

  nest->id = g_strdup (id);
  nest->refcount = 1;
  nest->pipeline = g_object_ref (pipeline);

  return nest;
}

static GaeguliNest *
gaeguli_nest_ref (GaeguliNest * nest)
{
  g_return_val_if_fail (nest != NULL, NULL);
  g_return_val_if_fail (nest->id != NULL, NULL);
  g_return_val_if_fail (nest->pipeline != NULL, NULL);

  g_atomic_int_inc (&nest->refcount);

  return nest;
}

static void
_on_target_property_change (GaeguliNest * nest, GParamSpec * pspec)
{
  GValue value = G_VALUE_INIT;

  g_object_get_property (G_OBJECT (nest->target_stream), pspec->name, &value);
  g_object_set_property (G_OBJECT (nest->dbus), pspec->name, &value);

  g_value_unset (&value);
}

static void
_on_dbus_property_change (GaeguliNest * nest, GParamSpec * pspec)
{
  GValue value = G_VALUE_INIT;

  g_object_get_property (G_OBJECT (nest->dbus), pspec->name, &value);
  g_object_set_property (G_OBJECT (nest->target_stream), pspec->name, &value);

  g_value_unset (&value);
}

static void
gaeguli_nest_dbus_register (GaeguliNest * nest, GDBusConnection * connection)
{
  g_autofree gchar *dbus_obj_path = NULL;
  g_autoptr (GError) error = NULL;
  GParamSpec **pspecs;
  guint n_prop;
  GValue value = G_VALUE_INIT;

  nest->dbus = gaeul2_dbus_source_channel_skeleton_new ();

  pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (nest->dbus),
      &n_prop);

  while (n_prop-- > 0) {
    const gchar *name = pspecs[n_prop]->name;

    if (g_str_equal (name, "g-flags")) {
      continue;
    }

    g_object_get_property (G_OBJECT (nest->target_stream), name, &value);
    g_object_set_property (G_OBJECT (nest->dbus), name, &value);
    g_value_unset (&value);
  }

  g_free (pspecs);

  g_signal_connect_swapped (nest->target_stream, "notify",
      G_CALLBACK (_on_target_property_change), nest);
  g_signal_connect_swapped (nest->dbus, "notify::bitrate",
      G_CALLBACK (_on_dbus_property_change), nest);
  g_signal_connect_swapped (nest->dbus, "notify::quantizer",
      G_CALLBACK (_on_dbus_property_change), nest);

  g_signal_connect_swapped (nest->dbus, "handle-get-stats",
      (GCallback) _channel_handle_get_stats, nest);

  dbus_obj_path = g_strdup_printf ("/org/hwangsaeul/Gaeul2/Source/channels/%s",
      nest->id);

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (nest->dbus),
          connection, dbus_obj_path, &error)) {
    g_warning ("Failed to export %s on D-Bus (reason: %s)", dbus_obj_path,
        error->message);
  }
}

static gboolean
gaeguli_nest_start (GaeguliNest * nest, const gchar * stream_id,
    const gchar * uri, GaeguliVideoCodec codec,
    GaeguliVideoResolution resolution, guint fps, guint bitrates)
{
  g_autoptr (GError) error = NULL;

  g_return_val_if_fail (nest->target_stream == NULL, FALSE);

  nest->target_stream =
      gaeguli_pipeline_add_srt_target_full (nest->pipeline, codec, resolution,
      fps, bitrates, uri, stream_id, &error);

  if (!nest->target_stream) {
    g_debug ("Failed to add srt target to pipeline (reason: %s)",
        error->message);
    return FALSE;
  }

  return TRUE;
}

static void
gaeguli_nest_stop (GaeguliNest * nest)
{
  if (nest->target_stream) {
    g_autoptr (GError) error = NULL;
    gaeguli_pipeline_remove_target (nest->pipeline, nest->target_stream,
        &error);
    nest->target_stream = NULL;

    if (error != NULL) {
      g_warning ("%s", error->message);
    }
  }

  if (nest->dbus) {
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (nest->dbus));
    g_clear_object (&nest->dbus);
  }

  gaeguli_pipeline_stop (nest->pipeline);
}

static void
gaeguli_nest_unref (GaeguliNest * nest)
{
  g_return_if_fail (nest != NULL);
  g_return_if_fail (nest->pipeline != NULL);

  if (g_atomic_int_dec_and_test (&nest->refcount)) {

    gaeguli_nest_stop (nest);

    g_clear_object (&nest->pipeline);
    g_clear_pointer (&nest->id, g_free);
    g_free (nest);
  }
}

/* *INDENT-OFF* */
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GaeguliNest, gaeguli_nest_unref)
/* *INDENT-ON* */

typedef enum
{
  PROP_TMPDIR = 1,

  /*< private > */

  PROP_LAST
} _GaeulSourceApplicationProperty;

static GParamSpec *properties[PROP_LAST] = { NULL };

struct _GaeulSourceApplication
{
  GaeulApplication parent;

  GSettings *settings;
  gchar *tmpdir;
  Gaeul2DBusSource *dbus_service;

  GList *gaegulis;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulSourceApplication, gaeul_source_application, GAEUL_TYPE_APPLICATION)
/* *INDENT-ON* */

static void
_delete_tmpdir (const gchar * tmpdir)
{
  const gchar *f = NULL;
  g_autoptr (GDir) tmpd = NULL;

  tmpd = g_dir_open (tmpdir, 0, NULL);

  g_debug ("delete tmpdir(%s)", tmpdir);

  while ((f = g_dir_read_name (tmpd)) != NULL) {
    g_autofree gchar *fname = g_build_filename (tmpdir, f, NULL);
    if (g_remove (fname) != 0) {
      g_error ("failed to remove %s", fname);
    }
  }
}

static int
gaeul_source_application_command_line (GApplication * app,
    GApplicationCommandLine * command_line)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (app);
  g_autoptr (GDBusConnection) dbus_connection = NULL;
  g_autofree gchar *uid = NULL;
  GaeulApplicationDBusType bus_type;
  GaeguliEncodingMethod encoding_method;

  g_auto (GStrv) channel_configs = NULL;
  guint i;

  g_debug ("command_line");

  g_clear_object (&self->settings);
  self->settings = gaeul_gsettings_new (GAEUL_SOURCE_APPLICATION_SCHEMA_ID,
      gaeul_application_get_config_path (GAEUL_APPLICATION (self))
      );

  g_settings_bind (self->settings, "uid", self, "uid", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "tmpdir", self, "tmpdir",
      G_SETTINGS_BIND_DEFAULT);

  if (g_settings_get_boolean (self->settings, "autoclean")) {
    g_autofree gchar *tmpdir = g_settings_get_string (self->settings, "tmpdir");
    _delete_tmpdir (tmpdir);
  }

  g_object_get (GAEUL_APPLICATION (self), "uid", &uid, NULL);
  encoding_method = (0x7fff & g_settings_get_enum (self->settings, "platform"));

  g_object_get (self, "dbus-type", &bus_type, NULL);
  if (bus_type != GAEUL_APPLICATION_DBUS_TYPE_NONE) {
    dbus_connection = g_bus_get_sync (bus_type, NULL, NULL);
  }

  channel_configs = g_settings_get_strv (self->settings, "channel-configs");

  for (i = 0; i < g_strv_length (channel_configs); i++) {
    const gchar *conf_file = channel_configs[i];

    g_autoptr (GaeguliNest) nest = NULL;
    g_autoptr (GaeguliPipeline) pipeline = NULL;

    g_autofree gchar *name = NULL;
    g_autofree gchar *device = NULL;

    g_autofree gchar *target_uri = NULL;
    g_autofree gchar *target_host = NULL;
    GaeguliSRTMode target_mode = GAEGULI_SRT_MODE_UNKNOWN;
    guint target_port = 0;
    g_autofree gchar *stream_id = NULL;

    g_autoptr (GSettings) ssettings =
        gaeul_gsettings_new (GAEUL_SOURCE_APPLICATION_SCHEMA_ID ".Channel",
        conf_file);

    GaeguliVideoSource video_source = g_settings_get_enum (ssettings, "source");
    GaeguliVideoCodec video_codec = g_settings_get_enum (ssettings, "codec");
    GaeguliVideoResolution video_resolution =
        g_settings_get_enum (ssettings, "resolution");
    guint bitrate = g_settings_get_uint (ssettings, "bitrate");
    guint fps = g_settings_get_uint (ssettings, "fps");

    name = g_settings_get_string (ssettings, "name");
    device = g_settings_get_string (ssettings, "device");
    target_uri = g_settings_get_string (ssettings, "target-uri");

    if (!name) {
      g_info ("config file[%s] doesn't have valid name property", conf_file);
      continue;
    }

    if (!gaeul_parse_srt_uri (target_uri, &target_host, &target_port,
            &target_mode)) {
      g_info ("can't parse srt target uri from config file[%s]", conf_file);
      continue;
    }

    stream_id = g_strconcat (uid, "_", name, NULL);

    pipeline =
        gaeguli_pipeline_new_full (video_source, device, encoding_method);

    nest = gaeguli_nest_new (stream_id, pipeline);

    if (!gaeguli_nest_start (nest, stream_id, target_uri, video_codec,
            video_resolution, fps, bitrate)) {
      goto error;
    }
    if (dbus_connection) {
      gaeguli_nest_dbus_register (nest, dbus_connection);
    }
    self->gaegulis = g_list_prepend (self->gaegulis, gaeguli_nest_ref (nest));
  }

  return EXIT_SUCCESS;

error:
  g_application_quit (app);
  return EXIT_FAILURE;
}

static void
gaeul_source_application_activate (GApplication * app)
{
  g_debug ("activate");

  // TBD

  G_APPLICATION_CLASS (gaeul_source_application_parent_class)->activate (app);
}

static void
gaeul_source_application_shutdown (GApplication * app)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (app);

  g_debug ("shutdown");

  g_list_free_full (self->gaegulis, (GDestroyNotify) gaeguli_nest_unref);

  G_APPLICATION_CLASS (gaeul_source_application_parent_class)->shutdown (app);
}

static void
gaeul_source_application_dispose (GObject * object)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (object);

  g_clear_object (&self->settings);
  g_clear_pointer (&self->tmpdir, g_free);

  G_OBJECT_CLASS (gaeul_source_application_parent_class)->dispose (object);
}

static void
gaeul_source_application_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (object);

  switch (prop_id) {
    case PROP_TMPDIR:
      g_value_set_string (value, self->tmpdir);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gaeul_source_application_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (object);

  switch (prop_id) {
    case PROP_TMPDIR:
      g_free (self->tmpdir);
      self->tmpdir = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gaeul_source_application_handle_list_channels (GaeulSourceApplication * self,
    GDBusMethodInvocation * invocation)
{
  g_autofree const gchar **paths = NULL;
  const gchar **path;
  GList *l = NULL;

  paths = g_new0 (const gchar *, g_list_length (self->gaegulis) + 1);
  path = paths;

  for (l = self->gaegulis; l != NULL; l = l->next) {
    GaeguliNest *nest = l->data;

    if (nest->dbus) {
      *path++ = g_dbus_interface_skeleton_get_object_path
          (G_DBUS_INTERFACE_SKELETON (nest->dbus));
    }
  }

  gaeul2_dbus_source_complete_list_channels (self->dbus_service, invocation,
      paths);

  return TRUE;
}

static gboolean
_channel_handle_get_stats (GaeguliNest * nest,
    GDBusMethodInvocation * invocation)
{
  gint64 packets_sent = 0;
  gint packets_sent_lost = 0;
  gint packets_retransmitted = 0;
  gint packet_ack_received = 0;
  gint packet_nack_received = 0;
  gint64 send_duration_us = 0;
  guint64 bytes_sent = 0;
  guint64 bytes_retransmitted = 0;
  guint64 bytes_sent_dropped = 0;
  guint64 packets_sent_dropped = 0;
  gdouble send_rate_mbps = 0;
  gint negotiated_latency_ms = 0;
  gdouble bandwidth_mbps = 0;
  gdouble rtt_ms = 0;

  GVariantDict dict;
  g_autoptr (GVariant) variant = NULL;

  variant = gaeguli_target_get_stats (nest->target_stream);
  g_variant_dict_init (&dict, variant);

  g_variant_dict_lookup (&dict, "packets-sent", "x", &packets_sent);
  g_variant_dict_lookup (&dict, "packets-sent-lost", "i", &packets_sent_lost);
  g_variant_dict_lookup (&dict, "packets-retransmitted", "i",
      &packets_retransmitted);
  g_variant_dict_lookup (&dict, "packet-ack-received", "i",
      &packet_ack_received);
  g_variant_dict_lookup (&dict, "packet-nack-received", "i",
      &packet_nack_received);
  g_variant_dict_lookup (&dict, "send-duration-us", "x", &send_duration_us);
  g_variant_dict_lookup (&dict, "bytes-sent", "t", &bytes_sent);
  g_variant_dict_lookup (&dict, "bytes-retransmitted", "t",
      &bytes_retransmitted);
  g_variant_dict_lookup (&dict, "bytes-sent-dropped", "t", &bytes_sent_dropped);
  g_variant_dict_lookup (&dict, "packets-sent-dropped", "t",
      &packets_sent_dropped);
  g_variant_dict_lookup (&dict, "send-rate-mbps", "d", &send_rate_mbps);
  g_variant_dict_lookup (&dict, "negotiated-latency-ms", "i",
      &negotiated_latency_ms);
  g_variant_dict_lookup (&dict, "bandwidth-mbps", "d", &bandwidth_mbps);
  g_variant_dict_lookup (&dict, "rtt-ms", "d", &rtt_ms);

  g_variant_dict_clear (&dict);

  gaeul2_dbus_source_channel_complete_get_stats (nest->dbus,
      invocation,
      packets_sent,
      packets_sent_lost,
      packets_retransmitted,
      packet_ack_received,
      packet_nack_received,
      send_duration_us,
      bytes_sent,
      bytes_retransmitted,
      bytes_sent_dropped,
      packets_sent_dropped,
      send_rate_mbps, negotiated_latency_ms, bandwidth_mbps, rtt_ms);

  return TRUE;
}

static gboolean
gaeul_source_application_dbus_register (GApplication * app,
    GDBusConnection * connection, const gchar * object_path, GError ** error)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (app);

  g_debug ("register dbus (%s)", object_path);
  if (!self->dbus_service) {
    self->dbus_service = gaeul2_dbus_source_skeleton_new ();

    g_signal_connect_swapped (self->dbus_service, "handle-list-channels",
        (GCallback) gaeul_source_application_handle_list_channels, self);
  }

  if (!G_APPLICATION_CLASS
      (gaeul_source_application_parent_class)->dbus_register (app, connection,
          object_path, error)) {
    return FALSE;
  }

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON
          (self->dbus_service), connection, "/org/hwangsaeul/Gaeul2/Source",
          error)) {
    g_warning ("Failed to export Gaeul2Source D-Bus interface (reason: %s)",
        (*error)->message);
  }

  return TRUE;
}

static void
gaeul_source_application_dbus_unregister (GApplication * app,
    GDBusConnection * connection, const gchar * object_path)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (app);

  if (self->dbus_service) {
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON
        (self->dbus_service));
    g_clear_object (&self->dbus_service);
  }

  G_APPLICATION_CLASS (gaeul_source_application_parent_class)->dbus_unregister
      (app, connection, object_path);
}

static void
gaeul_source_application_class_init (GaeulSourceApplicationClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  object_class->dispose = gaeul_source_application_dispose;
  object_class->set_property = gaeul_source_application_set_property;
  object_class->get_property = gaeul_source_application_get_property;

  properties[PROP_TMPDIR] =
      g_param_spec_string ("tmpdir", "tmpdir", "tmpdir", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties),
      properties);

  app_class->command_line = gaeul_source_application_command_line;
  app_class->activate = gaeul_source_application_activate;
  app_class->shutdown = gaeul_source_application_shutdown;
  app_class->dbus_register = gaeul_source_application_dbus_register;
  app_class->dbus_unregister = gaeul_source_application_dbus_unregister;
}

static void
gaeul_source_application_init (GaeulSourceApplication * self)
{
  g_application_set_flags (G_APPLICATION (self),
      G_APPLICATION_HANDLES_COMMAND_LINE);
}
